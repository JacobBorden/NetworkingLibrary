#include "server/server.h"
#include <array>
#include <stdexcept>
#include <dirent.h>
#include <poll.h>
#include <fcntl.h>
#include <cerrno>
#include <chrono>
#include <signal.h>
#include <thread>

// Linker seam forces fragmentation/short writes independently of OS scheduling.
static bool fragment = false, short_write = false;
static bool reset_send = false, zero_send = false, send_bounds_ok = true;
static size_t send_calls = 0;
static int send_socket = -1;
// Stateful receive script: consume actual bytes, then EOF; record native bounds.
static bool scripted_receive = false, script_bounds_ok = true;
static bool error_after_progress = false;
static bool interrupt_after_progress = false, interrupt_delivered = false;
static int recovery_error = EINTR;
static size_t script_offset = 0, script_calls = 0;
static std::array<char,515> script_payload{};
// Record the native recv sequence without injecting results.
struct RecvObservation {
    ssize_t result;
    int error;
};
static int observed_recv_socket = -1;
static std::array<RecvObservation, 3> observed_recvs{};
static size_t observed_recv_calls = 0;
// Capture the listener created by Server without replacing the native call.
static bool capture_listener = false;
static int server_listener = -1;
extern "C" int __real_listen(int, int);
extern "C" int __wrap_listen(int socket, int backlog) {
    const int result = __real_listen(socket, backlog);
    if (capture_listener && result == 0) server_listener = socket;
    return result;
}
extern "C" ssize_t __real_recv(int, void*, size_t, int);
extern "C" ssize_t __real_send(int, const void*, size_t, int);
extern "C" ssize_t __wrap_recv(int s, void* p, size_t n, int f) {
    if (scripted_receive) {
        ++script_calls;
        script_bounds_ok = script_bounds_ok && n == 512;
        if (interrupt_after_progress && script_offset == 512 && !interrupt_delivered) {
            interrupt_delivered = true;
            errno = recovery_error;
            return -1;
        }
        if (error_after_progress && script_offset == 512) {
            errno = ECONNRESET;
            return -1;
        }
        const size_t count = std::min(n, (error_after_progress ? size_t{512} : script_payload.size()) - script_offset);
        std::memcpy(p, script_payload.data() + script_offset, count);
        script_offset += count;
        return static_cast<ssize_t>(count);
    }
    if (fragment) { std::memcpy(p, "abc", 3); return 3; }
    const auto result = __real_recv(s,p,n,f);
    if (s == observed_recv_socket && observed_recv_socket >= 0) {
        if (observed_recv_calls < observed_recvs.size()) {
            observed_recvs[observed_recv_calls] = {result, result < 0 ? errno : 0};
        }
        ++observed_recv_calls;
    }
    return result;
}
extern "C" ssize_t __wrap_send(int s, const void* p, size_t n, int f) {
    if (reset_send || zero_send) {
        ++send_calls;
        send_bounds_ok = send_bounds_ok && s == send_socket && n == 6 && f == 0
            && std::memcmp(p,"abcdef",6) == 0;
        if (zero_send) return 0;
        errno = ECONNRESET;
        return -1;
    }
    if (short_write) return std::min<size_t>(n,2);
    return __real_send(s,p,n,f);
}
struct Fd {
    int fd = -1;
    Fd() = default;
    explicit Fd(int value):fd(value) { if(fd<0) throw std::runtime_error("socket setup"); }
    ~Fd(){if(fd>=0) ::close(fd);}
    Fd(const Fd&)=delete;
    Fd& operator=(const Fd&)=delete;
};
// Install a handler without SA_RESTART for the interrupt fixture.
struct ReceiveSignal {
    struct sigaction previous{};
    ReceiveSignal() {
        struct sigaction action{};
        action.sa_handler = [](int) {};
        ::sigemptyset(&action.sa_mask);
        if (::sigaction(SIGUSR1, &action, &previous) != 0)
            throw std::runtime_error("receive signal handler setup");
    }
    ~ReceiveSignal() { ::sigaction(SIGUSR1, &previous, nullptr); }
    ReceiveSignal(const ReceiveSignal&) = delete;
    ReceiveSignal& operator=(const ReceiveSignal&) = delete;
};
static std::jthread interrupt_receiver() {
    const pthread_t receiver = ::pthread_self();
    return std::jthread([receiver](std::stop_token stop) {
        // Keep trying in case a signal arrives before the receiver blocks.
        while (!stop.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            if (!stop.stop_requested() && ::pthread_kill(receiver, SIGUSR1) != 0)
                return;
        }
    });
}
struct Pair {
    Fd listener{::socket(AF_INET,SOCK_STREAM,0)};
    Fd sender{::socket(AF_INET,SOCK_STREAM,0)};
    int accepted = -1;
    Pair() {
        sockaddr_in addr{}; addr.sin_family=AF_INET; addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
        if(::bind(listener.fd,(sockaddr*)&addr,sizeof addr)||::listen(listener.fd,1)) throw std::runtime_error("listen");
        socklen_t size=sizeof addr;
        if(::getsockname(listener.fd,(sockaddr*)&addr,&size)||::connect(sender.fd,(sockaddr*)&addr,size)) throw std::runtime_error("connect");
        accepted=::accept(listener.fd,nullptr,nullptr);
        if(accepted<0) throw std::runtime_error("accept");
    }
    ~Pair(){if(accepted>=0) ::close(accepted);}
    void abort_sender() {
        const linger abortive_close{1, 0};
        if (::setsockopt(sender.fd, SOL_SOCKET, SO_LINGER,
                         &abortive_close, sizeof abortive_close) != 0)
            throw std::runtime_error("receive reset linger setup");
        const int socket = sender.fd;
        sender.fd = -1;
        if (::close(socket) != 0) throw std::runtime_error("receive reset peer close");

        // POLLERR is reported even with no requested events. Queued data alone
        // cannot wake this wait, and polling leaves the pending error intact.
        pollfd ready{accepted, 0, 0};
        if (::poll(&ready, 1, 1000) != 1 || !(ready.revents & POLLERR))
            throw std::runtime_error("receive reset did not become ready");
    }
    std::vector<char> read(size_t count) {
        std::vector<char> out(count);size_t offset=0;
        while(offset<count) {
            pollfd p{accepted,POLLIN,0};
            if(::poll(&p,1,1000)!=1) throw std::runtime_error("read deadline");
            auto n=::recv(accepted,out.data()+offset,count-offset,0);
            if(n<=0) throw std::runtime_error("read failure");offset+=n;
        }
        return out;
    }
};
static void queue_binary_chunk(const Pair& pair) {
    std::array<char, 512> payload{};
    for (size_t i = 0; i < payload.size(); ++i)
        payload[i] = static_cast<char>(i % 256);
    size_t sent = 0;
    while (sent < payload.size()) {
        const auto count = ::send(pair.sender.fd, payload.data() + sent,
                                  payload.size() - sent, MSG_NOSIGNAL);
        if (count <= 0) throw std::runtime_error("binary chunk fixture send");
        sent += static_cast<size_t>(count);
    }
    // Confirm the full chunk is queued. The peer stays open and idle.
    std::array<char, 512> queued{};
    if (::recv(pair.accepted, queued.data(), queued.size(),
               MSG_PEEK | MSG_WAITALL) != static_cast<ssize_t>(queued.size())
        || queued != payload)
        throw std::runtime_error("binary chunk fixture precondition");
}
static void connect_loopback_peer(int listener, int peer) {
    sockaddr_in listener_address{};
    socklen_t address_size = sizeof listener_address;
    if (listener < 0
        || ::getsockname(listener, reinterpret_cast<sockaddr*>(&listener_address),
                         &address_size) != 0
        || listener_address.sin_family != AF_INET
        || listener_address.sin_addr.s_addr != htonl(INADDR_LOOPBACK)
        || listener_address.sin_port == 0)
        throw std::runtime_error("server fixture requires an ephemeral IPv4 loopback listener");
    if (::connect(peer, reinterpret_cast<const sockaddr*>(&listener_address), address_size) != 0)
        throw std::runtime_error("server fixture connect");
    pollfd listener_ready{listener, POLLIN, 0};
    if (::poll(&listener_ready, 1, 1000) != 1 || !(listener_ready.revents & POLLIN))
        throw std::runtime_error("server fixture accept deadline");
}
static int fd_count(){DIR* d=opendir("/proc/self/fd");if(!d) throw std::runtime_error("fd inventory");int n=0;while(readdir(d))++n;closedir(d);return n;}
int main(int argc,char**) {
    const bool contract=argc>1;
    int passes=0,failures=0;
    auto check=[&](bool result,const char* name){std::cout<<(result?"PASS ":"FAIL ")<<name<<'\n';result?++passes:++failures;};
    int before=fd_count();
    {
        capture_listener = true;
        Networking::Server server(0,Networking::IPv4,"baseline.log");
        capture_listener = false;
        Networking::ClientConnection connection{};
        Pair pair; connection.clientSocket=pair.sender.fd;
        check(server.Send("",connection)==0,"empty send returns zero");
        for(size_t length:{511u,512u,513u}) {
            std::string bytes(length,'x');
            int n=server.Send(bytes.c_str(),connection);
            check(n==(int)length && pair.read(length)==std::vector<char>(bytes.begin(),bytes.end()),"ASCII boundary exact bytes");
        }
        const char binary[]{'A','\0','B','\0'};
        int n=server.Send(binary,connection);
        auto bytes=pair.read(n);
        check(contract ? n==3 && bytes==std::vector<char>({'A','\0','B'}) : n==1 && bytes==std::vector<char>({'A'}),"embedded NUL truncation");
        std::array<char,257> all{};for(int i=0;i<256;++i)all[i]=(char)i;
        n=server.Send(all.data(),connection);
        check(contract ? n==256 : n==0,"every-byte payload starts with NUL");
        fragment=true;auto received=server.Receive(connection);fragment=false;
        check(contract ? received==std::vector<char>({'a','b','c','d','e','f'}) : received==std::vector<char>({'a','b','c'}),"short read returns immediately");
        short_write=true;n=server.Send("abcdef",connection);short_write=false;
        check(contract ? n==6 : n==2,"short write returns partial count");
        // Zero progress is distinct from a native error: legacy returns zero
        // without closing or retrying. No data is delivered by this seam.
        {
            Pair zero_pair;
            Networking::ClientConnection zero_connection{};
            zero_connection.clientSocket=zero_pair.accepted;
            send_socket=zero_pair.accepted; send_calls=0; send_bounds_ok=true;
            zero_send=true;
            int result=server.Send("abcdef",zero_connection);
            zero_send=false;
            check(result==0 && send_calls==1 && send_bounds_ok,
                  "nonempty zero send returns zero without retry");
            check(::fcntl(zero_pair.accepted,F_GETFD)!=-1,
                  "zero send preserves accepted socket ownership");
            char byte{};
            errno=0;
            auto count=::recv(zero_pair.sender.fd,&byte,1,MSG_DONTWAIT);
            check(count==-1 && (errno==EAGAIN || errno==EWOULDBLOCK),
                  "zero send leaves peer open without payload");
        }
        // A terminal send error returns -1 and consumes the supplied socket.
        // Inject only send; real loopback shutdown/close must reach peer EOF.
        {
            Pair reset_pair;
            Networking::ClientConnection reset_connection{};
            reset_connection.clientSocket=reset_pair.accepted;
            send_socket=reset_pair.accepted; send_calls=0; send_bounds_ok=true;
            reset_send=true;
            int result=server.Send("abcdef",reset_connection);
            reset_send=false;
            check(result==-1 && send_calls==1 && send_bounds_ok,
                  "terminal send reset returns error with exact native arguments");
            errno=0;
            const bool closed=::fcntl(reset_pair.accepted,F_GETFD)==-1 && errno==EBADF;
            if (closed) reset_pair.accepted=-1;
            check(closed,"terminal send reset consumes accepted socket ownership");
            pollfd ready{reset_pair.sender.fd,POLLIN,0};
            if (::poll(&ready,1,1000)!=1) throw std::runtime_error("send reset EOF deadline");
            char byte{};
            check(::recv(reset_pair.sender.fd,&byte,1,0)==0,
                  "terminal send reset disconnect reaches peer EOF without payload");
        }
        // A full chunk must append into the next buffer region, then a short
        // chunk ends this legacy call. EOF belongs to the following call.
        for (size_t i=0; i<script_payload.size(); ++i)
            script_payload[i]=static_cast<char>(i%256);
        script_offset=0; script_calls=0; script_bounds_ok=true;
        scripted_receive=true;
        auto scripted_bytes=server.Receive(connection);
        check(scripted_bytes==std::vector<char>(script_payload.begin(),script_payload.end())
              && script_offset==515 && script_calls==2 && script_bounds_ok,
              "stateful full and short reads append exact binary bytes");
        auto scripted_eof=server.Receive(connection);
        scripted_receive=false;
        check(scripted_eof.empty() && script_offset==515 && script_calls==3 && script_bounds_ok,
              "stateful EOF follows consumed fragmented bytes");
        // With progress already buffered, -1 shrinks 512 bytes to 511 before
        // the error handler closes the socket and returns apparent success.
        {
            Pair error_pair;
            Networking::ClientConnection error_connection{};
            error_connection.clientSocket=error_pair.accepted;
            script_offset=0; script_calls=0; script_bounds_ok=true;
            error_after_progress=true; scripted_receive=true;
            auto partial=server.Receive(error_connection);
            scripted_receive=false; error_after_progress=false;
            check(partial==std::vector<char>(script_payload.begin(),script_payload.begin()+511)
                  && script_offset==512 && script_calls==2 && script_bounds_ok,
                  "error after full read silently drops final byte");
            errno=0;
            const bool closed=::fcntl(error_pair.accepted,F_GETFD)==-1 && errno==EBADF;
            if (closed) error_pair.accepted=-1; // legacy handler consumed ownership
            check(closed,"error after progress closes accepted socket");
        }
        // One EINTR or EAGAIN after a full chunk, then three recoverable bytes.
        // Legacy recursion discards its result and falls through to disconnect.
        for (int error : {EINTR, EAGAIN}) {
            recovery_error=error;
            Pair interrupted_pair;
            Networking::ClientConnection interrupted_connection{};
            interrupted_connection.clientSocket=interrupted_pair.accepted;
            script_offset=0; script_calls=0; script_bounds_ok=true;
            interrupt_delivered=false; interrupt_after_progress=true; scripted_receive=true;
            auto partial=server.Receive(interrupted_connection);
            scripted_receive=false; interrupt_after_progress=false;
            check(partial==std::vector<char>(script_payload.begin(),script_payload.begin()+511)
                  && interrupt_delivered && script_offset==515
                  && script_calls==(error==EINTR ? 3u : 4u) && script_bounds_ok,
                  error==EINTR ? "interrupted receive discards recursive recovery and final buffered byte"
                               : "would-block receive discards recursive recovery and final buffered byte");
            errno=0;
            const bool closed=::fcntl(interrupted_pair.accepted,F_GETFD)==-1 && errno==EBADF;
            if (closed) interrupted_pair.accepted=-1;
            check(closed,error==EINTR ? "interrupted receive falls through to disconnect after recovery"
                               : "would-block receive falls through to disconnect after recovery");
        }
        // Native EAGAIN before progress: no receive seam and no timing sleep.
        // Preflight proves the empty, connected socket is actually would-block.
        {
            Pair idle_pair;
            int flags=::fcntl(idle_pair.accepted,F_GETFL);
            if (flags<0 || ::fcntl(idle_pair.accepted,F_SETFL,flags|O_NONBLOCK))
                throw std::runtime_error("nonblocking fixture setup");
            char byte{};
            errno=0;
            auto count=::recv(idle_pair.accepted,&byte,1,MSG_PEEK);
            if (count!=-1 || (errno!=EAGAIN && errno!=EWOULDBLOCK))
                throw std::runtime_error("nonblocking fixture precondition");
            Networking::ClientConnection idle_connection{};
            idle_connection.clientSocket=idle_pair.accepted;
            bool threw=false;
            try { server.Receive(idle_connection); }
            catch (const std::length_error&) { threw=true; }
            check(threw,"native would-block receive throws length_error before error handling");
            errno=0;
            count=::recv(idle_pair.sender.fd,&byte,1,MSG_DONTWAIT);
            check(::fcntl(idle_pair.accepted,F_GETFD)!=-1 && count==-1
                  && (errno==EAGAIN || errno==EWOULDBLOCK),
                  "native would-block exception retains socket and connected peer");
        }
        // Native would-block after progress reaches a recursive empty Receive,
        // whose resize throws. The consumed full chunk cannot reach the caller.
        {
            Pair progress_pair;
            std::array<char,512> payload{};
            for (size_t i=0; i<payload.size(); ++i) payload[i]=static_cast<char>(i%256);
            size_t sent=0;
            while (sent<payload.size()) {
                auto n=::send(progress_pair.sender.fd,payload.data()+sent,payload.size()-sent,MSG_NOSIGNAL);
                if (n<=0) throw std::runtime_error("progress fixture send");
                sent+=static_cast<size_t>(n);
            }
            // Blocking peek establishes the entire chunk is queued, without consuming it.
            std::array<char,512> queued{};
            if (::recv(progress_pair.accepted,queued.data(),queued.size(),MSG_PEEK|MSG_WAITALL)!=512
                || queued!=payload) throw std::runtime_error("progress fixture precondition");
            int flags=::fcntl(progress_pair.accepted,F_GETFL);
            if (flags<0 || ::fcntl(progress_pair.accepted,F_SETFL,flags|O_NONBLOCK))
                throw std::runtime_error("progress nonblocking setup");
            Networking::ClientConnection progress_connection{};
            progress_connection.clientSocket=progress_pair.accepted;
            bool threw=false;
            try { server.Receive(progress_connection); }
            catch (const std::length_error&) { threw=true; }
            check(threw,"native would-block after progress throws instead of returning bytes");
            char byte{};
            errno=0;
            auto remaining=::recv(progress_pair.accepted,&byte,1,MSG_PEEK);
            check(remaining==-1 && (errno==EAGAIN || errno==EWOULDBLOCK)
                  && ::fcntl(progress_pair.accepted,F_GETFD)!=-1,
                  "native progress exception consumes full chunk and retains socket");
            // The connection remains usable, but a later call cannot recover the lost chunk.
            const char tail[]{'x','\0','y'};
            if (::send(progress_pair.sender.fd,tail,sizeof tail,MSG_NOSIGNAL)!=sizeof tail
                || ::shutdown(progress_pair.sender.fd,SHUT_WR))
                throw std::runtime_error("progress recovery setup");
            pollfd ready{progress_pair.accepted,POLLIN,0};
            if (::poll(&ready,1,1000)!=1) throw std::runtime_error("progress recovery deadline");
            check(server.Receive(progress_connection)==std::vector<char>(tail,tail+sizeof tail),
                  "native progress exception permits later binary receive without lost chunk");
        }
        // Real IPv4 loopback, no linker seam: accumulate stream chunks until EOF.
        // Exactly two legacy buffer lengths also exercises EOF after a full read.
        {
            Pair binary_pair;
            std::vector<char> payload(1024);
            for (size_t i=0; i<payload.size(); ++i) payload[i]=static_cast<char>(i%256);
            size_t sent=0;
            while (sent<payload.size()) {
                auto count=::send(binary_pair.sender.fd,payload.data()+sent,payload.size()-sent,MSG_NOSIGNAL);
                if (count<=0) throw std::runtime_error("binary fixture send");
                sent+=static_cast<size_t>(count);
            }
            if (::shutdown(binary_pair.sender.fd,SHUT_WR)) throw std::runtime_error("binary fixture shutdown");
            Networking::ClientConnection binary_connection{};
            binary_connection.clientSocket=binary_pair.accepted;
            std::vector<char> actual;
            for (;;) {
                pollfd ready{binary_pair.accepted,POLLIN,0};
                if (::poll(&ready,1,1000)!=1) throw std::runtime_error("binary fixture deadline");
                auto chunk=server.Receive(binary_connection);
                if (chunk.empty()) break;
                actual.insert(actual.end(),chunk.begin(),chunk.end());
                if (actual.size()>payload.size()) throw std::runtime_error("binary fixture excess bytes");
            }
            check(actual==payload,"binary receive drains exact bytes before EOF");
            check(server.Receive(binary_connection).empty(),"EOF remains observable after binary drain");
            check(::fcntl(binary_pair.accepted,F_GETFD)!=-1,
                  "peer half-close preserves accepted socket ownership");
            // FIN closes only the peer's sending direction. Exercise the live
            // reverse direction with explicit lengths and every byte value.
            size_t replied=0;
            while (replied<payload.size()) {
                auto count=::send(binary_pair.accepted,payload.data()+replied,
                                  payload.size()-replied,MSG_NOSIGNAL);
                if (count<=0) throw std::runtime_error("half-close reply send");
                replied+=static_cast<size_t>(count);
            }
            std::vector<char> reply(payload.size());
            size_t read=0;
            while (read<reply.size()) {
                pollfd ready{binary_pair.sender.fd,POLLIN,0};
                if (::poll(&ready,1,1000)!=1) throw std::runtime_error("half-close reply deadline");
                auto count=::recv(binary_pair.sender.fd,reply.data()+read,reply.size()-read,0);
                if (count<=0) throw std::runtime_error("half-close reply receive");
                read+=static_cast<size_t>(count);
            }
            check(reply==payload,"peer half-close permits exact binary reverse transfer");
        }
        // Native timeout with receive seams disabled and peer still connected.
        {
            Pair timed_pair;
            timeval timeout{0,20000};
            if (::setsockopt(timed_pair.accepted,SOL_SOCKET,SO_RCVTIMEO,
                             &timeout,sizeof timeout)) throw std::runtime_error("receive timeout setup");
            Networking::ClientConnection timed_connection{};
            timed_connection.clientSocket=timed_pair.accepted;
            const auto start=std::chrono::steady_clock::now();
            bool threw=false;
            try { server.Receive(timed_connection); }
            catch (const std::length_error&) { threw=true; }
            const auto elapsed=std::chrono::steady_clock::now()-start;
            check(threw && elapsed>=std::chrono::milliseconds(10),
                  "native receive timeout throws length_error after waiting");
            char byte{};
            errno=0;
            auto count=::recv(timed_pair.sender.fd,&byte,1,MSG_DONTWAIT);
            const int peer_error=errno;
            check(::fcntl(timed_pair.accepted,F_GETFD)!=-1 && count==-1
                  && (peer_error==EAGAIN || peer_error==EWOULDBLOCK),
                  "native receive timeout retains descriptor and connected peer");
            const char payload[]{'x','\0','y'};
            if (::send(timed_pair.sender.fd,payload,sizeof payload,MSG_NOSIGNAL)!=sizeof payload)
                throw std::runtime_error("timeout recovery send");
            if (::shutdown(timed_pair.sender.fd,SHUT_WR)) throw std::runtime_error("timeout recovery shutdown");
            check(server.Receive(timed_connection)==std::vector<char>(payload,payload+sizeof payload),
                  "native receive timeout permits later exact binary receive");
        }
        {
            Pair reset_pair;
            reset_pair.abort_sender();

            Networking::ClientConnection reset_connection{};
            reset_connection.clientSocket = reset_pair.accepted;
            observed_recv_socket = reset_pair.accepted;
            bool threw = false;
            try { server.Receive(reset_connection); }
            catch (const std::length_error&) { threw = true; }
            observed_recv_socket = -1;

            check(observed_recv_calls == 1 && observed_recvs[0].result == -1
                  && observed_recvs[0].error == ECONNRESET,
                  "abortive peer close delivers native ECONNRESET to receive");
            check(threw, "native receive reset throws length_error");
            const bool closed = ::fcntl(reset_pair.accepted, F_GETFD) == -1 && errno == EBADF;
            if (closed) reset_pair.accepted = -1;
            check(!closed, "native receive reset retains accepted descriptor");
        }
        {
            Pair reset_pair;
            std::array<char, 512> payload{};
            for (size_t i = 0; i < payload.size(); ++i)
                payload[i] = static_cast<char>(i % 256);

            size_t sent = 0;
            while (sent < payload.size()) {
                const auto count = ::send(reset_pair.sender.fd, payload.data() + sent,
                                          payload.size() - sent, MSG_NOSIGNAL);
                if (count <= 0) throw std::runtime_error("reset progress fixture send");
                sent += static_cast<size_t>(count);
            }
            std::array<char, 512> queued{};
            if (::recv(reset_pair.accepted, queued.data(), queued.size(), MSG_PEEK | MSG_WAITALL)
                    != static_cast<ssize_t>(queued.size()) || queued != payload)
                throw std::runtime_error("reset progress fixture precondition");
            reset_pair.abort_sender();

            Networking::ClientConnection reset_connection{};
            reset_connection.clientSocket = reset_pair.accepted;
            observed_recv_calls = 0;
            observed_recv_socket = reset_pair.accepted;
            const auto partial = server.Receive(reset_connection);
            observed_recv_socket = -1;
            const bool closed = ::fcntl(reset_pair.accepted, F_GETFD) == -1 && errno == EBADF;
            if (closed) reset_pair.accepted = -1;

            check(observed_recv_calls == 2 && observed_recvs[0].result == 512
                  && observed_recvs[1].result == -1 && observed_recvs[1].error == ECONNRESET,
                  "native receive consumes a full chunk before ECONNRESET");
            check(partial == std::vector<char>(payload.begin(), payload.end() - 1),
                  "native reset after progress returns 511 of 512 bytes");
            check(closed, "native reset after progress closes accepted descriptor");
        }
        {
            Pair interrupted_pair;
            const timeval timeout{1, 0};
            if (::setsockopt(interrupted_pair.accepted, SOL_SOCKET, SO_RCVTIMEO,
                             &timeout, sizeof timeout) != 0)
                throw std::runtime_error("interrupted receive watchdog setup");

            Networking::ClientConnection interrupted_connection{};
            interrupted_connection.clientSocket = interrupted_pair.accepted;
            bool threw = false;
            {
                ReceiveSignal handler;
                auto interrupter = interrupt_receiver();
                observed_recv_calls = 0;
                observed_recv_socket = interrupted_pair.accepted;
                try { server.Receive(interrupted_connection); }
                catch (const std::length_error&) { threw = true; }
                observed_recv_socket = -1;
            } // Join the signal sender before restoring the handler.

            check(observed_recv_calls == 1 && observed_recvs[0].result == -1
                  && observed_recvs[0].error == EINTR,
                  "targeted signal interrupts native recv with EINTR");
            check(threw, "native EINTR before progress throws length_error");
            const bool open = ::fcntl(interrupted_pair.accepted, F_GETFD) != -1;
            if (!open) interrupted_pair.accepted = -1;
            const char payload[]{'x', '\0', 'y'};
            if (open && (::send(interrupted_pair.sender.fd, payload, sizeof payload,
                                 MSG_NOSIGNAL) != sizeof payload
                         || ::shutdown(interrupted_pair.sender.fd, SHUT_WR) != 0))
                throw std::runtime_error("interrupted receive recovery setup");
            check(open && server.Receive(interrupted_connection)
                              == std::vector<char>(payload, payload + sizeof payload),
                  "native EINTR retains the descriptor for exact binary receive");
        }
        {
            Pair interrupted_pair;
            const timeval timeout{1, 0};
            if (::setsockopt(interrupted_pair.accepted, SOL_SOCKET, SO_RCVTIMEO,
                             &timeout, sizeof timeout) != 0)
                throw std::runtime_error("interruption after progress watchdog setup");
            queue_binary_chunk(interrupted_pair);

            Networking::ClientConnection interrupted_connection{};
            interrupted_connection.clientSocket = interrupted_pair.accepted;
            bool threw = false;
            {
                ReceiveSignal handler;
                auto interrupter = interrupt_receiver();
                observed_recv_calls = 0;
                observed_recv_socket = interrupted_pair.accepted;
                try { server.Receive(interrupted_connection); }
                catch (const std::length_error&) { threw = true; }
                observed_recv_socket = -1;
            }

            const auto interrupted = [](const RecvObservation& read) {
                return read.result == -1 && read.error == EINTR;
            };
            check(observed_recv_calls == 3 && observed_recvs[0].result == 512
                  && interrupted(observed_recvs[1]) && interrupted(observed_recvs[2]),
                  "native receive makes progress before two EINTR results");
            check(threw, "native EINTR after progress throws instead of returning consumed bytes");

            const bool open = ::fcntl(interrupted_pair.accepted, F_GETFD) != -1;
            if (!open) interrupted_pair.accepted = -1;
            const char tail[]{'x', '\0', 'y'};
            if (open && (::send(interrupted_pair.sender.fd, tail, sizeof tail, MSG_NOSIGNAL)
                             != sizeof tail || ::shutdown(interrupted_pair.sender.fd, SHUT_WR) != 0))
                throw std::runtime_error("interruption after progress recovery setup");
            check(open && server.Receive(interrupted_connection)
                              == std::vector<char>(tail, tail + sizeof tail),
                  "native EINTR after progress retains the descriptor for later binary receive");
        }
        {
            Pair timed_pair;
            const timeval timeout{0, 20000};
            if (::setsockopt(timed_pair.accepted, SOL_SOCKET, SO_RCVTIMEO,
                             &timeout, sizeof timeout) != 0)
                throw std::runtime_error("timeout after progress setup");

            queue_binary_chunk(timed_pair);

            Networking::ClientConnection timed_connection{};
            timed_connection.clientSocket = timed_pair.accepted;
            observed_recv_calls = 0;
            observed_recv_socket = timed_pair.accepted;
            const auto start = std::chrono::steady_clock::now();
            bool threw = false;
            try { server.Receive(timed_connection); }
            catch (const std::length_error&) { threw = true; }
            const auto elapsed = std::chrono::steady_clock::now() - start;
            observed_recv_socket = -1;

            // Linux reports SO_RCVTIMEO expiry as EAGAIN or EWOULDBLOCK.
            const auto timed_out = [](const RecvObservation& read) {
                return read.result == -1 && (read.error == EAGAIN || read.error == EWOULDBLOCK);
            };
            check(observed_recv_calls == 3 && observed_recvs[0].result == 512
                  && timed_out(observed_recvs[1]) && timed_out(observed_recvs[2])
                  && elapsed >= std::chrono::milliseconds(20),
                  "native receive makes progress before timing out twice");
            check(threw, "native timeout after progress throws instead of returning consumed bytes");

            const bool open = ::fcntl(timed_pair.accepted, F_GETFD) != -1;
            if (!open) timed_pair.accepted = -1;
            const char tail[]{'x', '\0', 'y'};
            if (open && (::send(timed_pair.sender.fd, tail, sizeof tail, MSG_NOSIGNAL)
                             != sizeof tail || ::shutdown(timed_pair.sender.fd, SHUT_WR) != 0))
                throw std::runtime_error("timeout progress recovery setup");
            check(open && server.Receive(timed_connection) == std::vector<char>(tail, tail + sizeof tail),
                  "native timeout after progress retains the descriptor for later binary receive");
        }
        connection.clientSocket=pair.accepted;
        ::shutdown(pair.sender.fd,SHUT_WR);
        check(server.Receive(connection).empty(),"orderly EOF empty vector");
        connection.clientSocket=-1;bool length_error=false;
        try {server.Receive(connection);}catch(const std::length_error&){length_error=true;}
        check(contract ? !length_error : length_error,"negative recv resized before error check");
        // Exercise the actual listener and the connection registered by Accept.
        Fd peer{::socket(AF_INET, SOCK_STREAM, 0)};
        connect_loopback_peer(server_listener, peer.fd);
        const auto accepted = server.Accept();
        Fd accepted_fd{accepted.clientSocket};
        server.Shutdown();
        const bool listener_closed = ::fcntl(server_listener, F_GETFD) == -1 && errno == EBADF;
        check(listener_closed && !server.ServerIsRunning(),
              "shutdown closes the listener and clears running state");

        const char live_payload[] = {'x', '\0', 'y'};
        const bool accepted_open = ::fcntl(accepted_fd.fd, F_GETFD) != -1;
        if (!accepted_open) accepted_fd.fd = -1;
        bool received_after_shutdown = false;
        if (accepted_open) {
            if (::send(peer.fd, live_payload, sizeof live_payload, MSG_NOSIGNAL) != sizeof live_payload
                || ::shutdown(peer.fd, SHUT_WR) != 0)
                throw std::runtime_error("shutdown fixture send");
            pollfd ready{accepted_fd.fd, POLLIN, 0};
            if (::poll(&ready, 1, 1000) != 1 || !(ready.revents & POLLIN))
                throw std::runtime_error("shutdown fixture receive deadline");
            const std::vector<char> expected(live_payload, live_payload + sizeof live_payload);
            received_after_shutdown = server.Receive(accepted) == expected;
        }
        check(received_after_shutdown, "accepted connection receives exact binary data after listener shutdown");

        bool accepted_closed = false;
        if (accepted_open) {
            server.DisconnectClient(accepted);
            accepted_closed = ::fcntl(accepted_fd.fd, F_GETFD) == -1 && errno == EBADF;
            if (accepted_closed) accepted_fd.fd = -1;
        }
        check(accepted_closed, "explicit disconnect closes the accepted descriptor after listener shutdown");
    }
    {
        Fd listener;
        Fd accepted_fd;
        Fd peer{::socket(AF_INET, SOCK_STREAM, 0)};
        {
            server_listener = -1;
            capture_listener = true;
            Networking::Server server(0, Networking::IPv4, "destructor.log");
            capture_listener = false;
            listener.fd = server_listener;
            connect_loopback_peer(listener.fd, peer.fd);
            accepted_fd.fd = server.Accept().clientSocket;
            const auto clients = server.getClients();
            if (accepted_fd.fd < 0 || clients.size() != 1
                || clients.front().clientSocket != accepted_fd.fd)
                throw std::runtime_error("destructor fixture requires one registered client");
        }

        // Check both descriptors before allocating any new ones. If a future
        // destructor closes them, the guards must relinquish the stale numbers.
        int accepting = 0;
        socklen_t accepting_size = sizeof accepting;
        const int listener_status = ::getsockopt(listener.fd, SOL_SOCKET, SO_ACCEPTCONN,
                                                 &accepting, &accepting_size);
        if (listener_status == -1 && errno == EBADF) listener.fd = -1;
        check(listener_status == 0 && accepting == 1,
              "server destruction leaves its TCP listener accepting");

        sockaddr_in remote{};
        socklen_t remote_size = sizeof remote;
        const int peer_status = ::getpeername(accepted_fd.fd,
                                             reinterpret_cast<sockaddr*>(&remote), &remote_size);
        if (peer_status == -1 && errno == EBADF) accepted_fd.fd = -1;
        sockaddr_in local{};
        socklen_t local_size = sizeof local;
        const int local_status = ::getsockname(peer.fd, reinterpret_cast<sockaddr*>(&local),
                                               &local_size);
        check(peer_status == 0 && local_status == 0 && remote.sin_family == AF_INET
              && remote.sin_addr.s_addr == htonl(INADDR_LOOPBACK)
              && remote.sin_addr.s_addr == local.sin_addr.s_addr
              && remote.sin_port == local.sin_port,
              "server destruction leaves its registered TCP socket connected");
    }
    check(fd_count()==before,"harness explicit cleanup restores fd count");
    std::cout<<"RESULT "<<passes<<" passed "<<failures<<" failed\n";
    return failures?1:0;
}

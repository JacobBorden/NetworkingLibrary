#include "server/server.h"
#include <array>
#include <stdexcept>
#include <dirent.h>
#include <poll.h>
#include <fcntl.h>
#include <cerrno>

// Linker seam forces fragmentation/short writes independently of OS scheduling.
static bool fragment = false, short_write = false;
// Stateful receive script: consume actual bytes, then EOF; record native bounds.
static bool scripted_receive = false, script_bounds_ok = true;
static bool error_after_progress = false;
static bool interrupt_after_progress = false, interrupt_delivered = false;
static size_t script_offset = 0, script_calls = 0;
static std::array<char,515> script_payload{};
extern "C" ssize_t __real_recv(int, void*, size_t, int);
extern "C" ssize_t __real_send(int, const void*, size_t, int);
extern "C" ssize_t __wrap_recv(int s, void* p, size_t n, int f) {
    if (scripted_receive) {
        ++script_calls;
        script_bounds_ok = script_bounds_ok && n == 512;
        if (interrupt_after_progress && script_offset == 512 && !interrupt_delivered) {
            interrupt_delivered = true;
            errno = EINTR;
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
    return __real_recv(s,p,n,f);
}
extern "C" ssize_t __wrap_send(int s, const void* p, size_t n, int f) {
    if (short_write) return std::min<size_t>(n,2);
    return __real_send(s,p,n,f);
}
struct Fd {
    int fd = -1;
    explicit Fd(int value):fd(value) { if(fd<0) throw std::runtime_error("socket setup"); }
    ~Fd(){if(fd>=0) ::close(fd);}
    Fd(const Fd&)=delete;
    Fd& operator=(const Fd&)=delete;
};
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
static int fd_count(){DIR* d=opendir("/proc/self/fd");if(!d) throw std::runtime_error("fd inventory");int n=0;while(readdir(d))++n;closedir(d);return n;}
int main(int argc,char**) {
    const bool contract=argc>1;
    int passes=0,failures=0;
    auto check=[&](bool result,const char* name){std::cout<<(result?"PASS ":"FAIL ")<<name<<'\n';result?++passes:++failures;};
    int before=fd_count();
    {
        Networking::Server server(0,Networking::IPv4,"baseline.log");
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
        // One EINTR after a full chunk, followed by three recoverable bytes.
        // Legacy recursion discards its result and falls through to disconnect.
        {
            Pair interrupted_pair;
            Networking::ClientConnection interrupted_connection{};
            interrupted_connection.clientSocket=interrupted_pair.accepted;
            script_offset=0; script_calls=0; script_bounds_ok=true;
            interrupt_delivered=false; interrupt_after_progress=true; scripted_receive=true;
            auto partial=server.Receive(interrupted_connection);
            scripted_receive=false; interrupt_after_progress=false;
            check(partial==std::vector<char>(script_payload.begin(),script_payload.begin()+511)
                  && interrupt_delivered && script_offset==515 && script_calls==3 && script_bounds_ok,
                  "interrupted receive discards recursive recovery and final buffered byte");
            errno=0;
            const bool closed=::fcntl(interrupted_pair.accepted,F_GETFD)==-1 && errno==EBADF;
            if (closed) interrupted_pair.accepted=-1;
            check(closed,"interrupted receive falls through to disconnect after recovery");
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
        connection.clientSocket=pair.accepted;
        ::shutdown(pair.sender.fd,SHUT_WR);
        check(server.Receive(connection).empty(),"orderly EOF empty vector");
        connection.clientSocket=-1;bool length_error=false;
        try {server.Receive(connection);}catch(const std::length_error&){length_error=true;}
        check(contract ? !length_error : length_error,"negative recv resized before error check");
        server.Shutdown();
    }
    check(fd_count()==before,"harness explicit cleanup restores fd count");
    std::cout<<"RESULT "<<passes<<" passed "<<failures<<" failed\n";
    return failures?1:0;
}

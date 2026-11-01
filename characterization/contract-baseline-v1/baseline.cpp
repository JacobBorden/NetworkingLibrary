#include "server/server.h"
#include <array>
#include <stdexcept>
#include <dirent.h>
#include <poll.h>

// Linker seam forces fragmentation/short writes independently of OS scheduling.
static bool fragment = false, short_write = false;
extern "C" ssize_t __real_recv(int, void*, size_t, int);
extern "C" ssize_t __real_send(int, const void*, size_t, int);
extern "C" ssize_t __wrap_recv(int s, void* p, size_t n, int f) {
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

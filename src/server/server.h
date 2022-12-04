#pragma once
#ifndef _NET_SERVER_
#define _NET_SERVER_
#ifdef _WIN32
#include <WinSock2.h>
#include <ws2tcpip.h>
#pragma comment("Ws2_32.lib")
#ifndef _NETWORK_MACROS_
#define _NETWORK_MACROS
#define VERSIONREQUESTED MAKEWORD(2,2)
#define INVALIDSOCKET(s) ((s) == INVALID_SOCKET)
#define GETERROR() (WSAGetLastError())
#define CLOSESOCKET(s) (closesocket(s))
#endif
#else 
#include  <sys/socket.h>
#include <netinet/in.h>
#include  <errno.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <bits/stdc++.h>
#include <netdb.h>
#ifndef _NETWORK_MACROS_
#define _NETWORK_MACROS_
#define SOCKET int
#define INVALIDSOCKET(s) ((s) < 0)
#define GETERROR() (errno)
#define CLOSESOCKET(s) (close(s))
#define ZeroMemory(dest, count) (memset(dest,0,count))
typedef const char* PCSTR;
#endif
#endif
#include <iostream>
#include <string>
#include <vector>

namespace Networking{

struct ClientConnection{
SOCKET clientSocket;
sockaddr_in clientInfo;
};


    class Server{
        public:
        Server();
        Server(int _pPortNumber);
        ~Server();
        bool InitServer();
        bool CreateServerSocket(int _pPortNumber);
        Networking::ClientConnection Listen();
        void SetSocketType(int _pSockType);
        void SetFamily(int _pFamily);
        void SetProtocol(int _pProtocol);
        bool Send(char* _SendBuffer);
        std::vector<char> Receive();
        bool ServerIsRunning();
    };
}

#endif
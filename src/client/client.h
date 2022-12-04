#pragma once
#ifndef _NET_CLIENT_
#define _NET_CLIENT_
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

class Client{
public:
    Client();
    Client(char* _pHost, int _pPortNumber);
    ~Client();
    bool InitClientSocket();
    bool CreateClientTCPSocket(char* _pHost, int pPort);
    bool CreateClientUDPSocket(char* _pHost, int pPort);
    bool CreateClientSocket(char* _pHost, int pPort);
    bool ConnectClientSocket();
    void SetSocketType(int _pSocktype);
    void SetProtocol(int _pProtocol);
    void SetFamily(int _pFamily);
   int Send(char* _pSendBuffer);
    std::vector<char> Receive();
    bool Disconnect();
    bool IsConnected();

private:
bool clientIsConnected =false;
SOCKET connectionSocket;
#ifdef _WIN32
WSADATA wsaData;
#endif
addrinfo addressInfo;
addrinfo* hostAddressInfo;

    };

}

#endif
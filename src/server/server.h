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
#define SOCKET_ERROR -1
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
        int Send(char* _pSendBuffer, Networking::ClientConnection _pClient);
        int SendTo(char* _pBuffer, char* _pAddress, int _pPort);
        int SendToAll(char* _pSendBuffer);
        void SendFile(const std::string& _pFilePath, Networking::ClientConnection client);
        std::vector<char> Receive(Networking::ClientConnection client);
        std::vector<char> ReceiveFrom(char* _pAddress, int _pPort);
        void ReceiveFile(const std::string& _pFilePath, Networking::ClientConnection client);
        bool ServerIsRunning();
        void Shutdown();
        void DisconnectClient(Networking::ClientConnection _pClient);
        std::vector<Networking::ClientConnection> getClients() const;

        private:
        #ifdef _WIN32
        WSADATA wsaData;
        #endif
        addrinfo addressInfo;
        SOCKET serverSocket;
        sockaddr_in serverInfo;
        bool serverIsConnected = false;
        std::vector<Networking::ClientConnection> clients;
    };
}

#endif
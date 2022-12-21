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
#include "networkexception.h"

enum ServerType
{
	IPv4,
	IPv6
};

namespace Networking {

// Struct to hold information about a connected client
struct ClientConnection {
	SOCKET clientSocket;
	sockaddr_in clientInfo;
	sockaddr_in6 clientInfo6;
};


class Server {
public:
// Default constructor
Server();

// Constructor that takes in a port number
Server(int _pPortNumber);

//Constructor thst takes in a server type
Server(ServerType _pServerType);

//Constructor that takes in a port number and a server type
Server(int _pPortNumber, ServerType _pServerType);

// Destructor
~Server();

// Initializes the server
bool InitServer();

// Creates a socket for the server using the specified port number
bool CreateServerSocket(int _pPortNumber);

//Creates a socket for the server using the specified port number and server type
bool CreateServerSocket(int _pPortNumber, ServerType _pServerType);

// Listens for incoming client connections and returns a
// Networking::ClientConnection object representing the connected client
Networking::ClientConnection Listen();

// Sets the socket type
void SetSocketType(int _pSockType);

// Sets the socket family
void SetFamily(int _pFamily);

// Sets the socket protocol
void SetProtocol(int _pProtocol);

// Sends data to a specific client
int Send(char* _pSendBuffer, Networking::ClientConnection _pClient);

// Sends data to a specific address and port
int SendTo(char* _pBuffer, char* _pAddress, int _pPort);

// Sends data to all connected clients
int SendToAll(char* _pSendBuffer);

// Sends a file to a specific client
void SendFile(const std::string& _pFilePath, Networking::ClientConnection client);

// Receives data from a specific client
std::vector<char> Receive(Networking::ClientConnection client);

// Receives data from a specific address and port
std::vector<char> ReceiveFrom(char* _pAddress, int _pPort);

// Receives a file from a specific client
void ReceiveFile(const std::string& _pFilePath, Networking::ClientConnection client);

// Returns true if the server is currently running and listening for connections
// Returns false otherwise
bool ServerIsRunning();
// Shut down the server
void Shutdown();

// Disconnects a specific client
void DisconnectClient(Networking::ClientConnection _pClient);

// Returns a vector of Networking::ClientConnection objects representing all
// currently connected clients
std::vector<Networking::ClientConnection> getClients() const;

//Handles errors
void ErrorHandling(NetworkException _pNetEx);

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
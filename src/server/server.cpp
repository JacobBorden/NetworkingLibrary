#include "server.h"

Networking::Server::Server()
{
    try{
        Networking::Server::InitServer();
        Networking::Server::CreateServerSocket(8080);
    }
    catch(NetworkException netEx)
    {
        std::cerr<<"Exception thrown.  "<<netEx.what() <<std::endl;
        ErrorHandling(netEx);
    }
}

Networking::Server::Server(ServerType _pServerType)
{
    try{
        Networking::Server::InitServer();
        Networking::Server::CreateServerSocket(8080, _pServerType);
    }
   catch(NetworkException netEx)
    {
        std::cerr<<"Exception thrown.  "<<netEx.what() <<std::endl;
        ErrorHandling(netEx);
    }
}

Networking::Server::Server(int _pPortNumber,  ServerType _pServerType)
{
    try{
        Networking::Server::InitServer();
        Networking::Server::CreateServerSocket(_pPortNumber, _pServerType);
    }
   catch(NetworkException netEx)
    {
        std::cerr<<"Exception thrown.  "<<netEx.what() <<std::endl;
        ErrorHandling(netEx);
    }
}

Networking::Server::Server(int _pPortNumber)
{
    try{
        Networking::Server::InitServer();
        Networking::Server::CreateServerSocket(_pPortNumber);
    }
   catch(NetworkException netEx)
    {
        std::cerr<<"Exception thrown.  "<<netEx.what() <<std::endl;
        ErrorHandling(netEx);
    }
}

Networking::Server::~Server()
{
}

bool Networking::Server::InitServer()
{
    #ifdef _WIN32
    int erroCode = WSAStartup(VERSIONREQUESTED, wsaData);
    if(errorCode)
        {
            Networking::NetworkException netEx(-1, errorCode, "Unable to Initilize Server");
            throw netEx;
        }
   #endif

    return true;
}

bool  Networking::Server::CreateServerSocket(int _pPortNumber)
{
ZeroMemory(&addressInfo, sizeof(addressInfo));
SetFamily(AF_INET);
SetSocketType(SOCK_STREAM);
SetProtocol(IPPROTO_TCP);

// Create a server socket
serverSocket = socket(addressInfo.ai_family, addressInfo.ai_socktype, addressInfo.ai_protocol);

// Check if the socket was successfully created
if (INVALIDSOCKET(serverSocket))
{
    int errorCode = GETERROR();
  Networking::NetworkException netEx(serverSocket, errorCode, "Unable to create socket");
  throw netEx;
}

// Set up the sockaddr_in structure
serverInfo.sin_family = addressInfo.ai_family;
serverInfo.sin_addr.s_addr = INADDR_ANY;
serverInfo.sin_port = htons(_pPortNumber);

// Bind the server socket to a port
int bindResult = bind(serverSocket, (sockaddr*)&serverInfo, sizeof(serverInfo));

// Check if the socket was successfully bound
if (bindResult == SOCKET_ERROR)
{
    // Get the error code
    int errorCode = GETERROR();
      Networking::NetworkException netEx(serverSocket, errorCode, "Unable to bind socket");
  throw netEx;

}

// Start listening for incoming connections
int listenResult = listen(serverSocket, SOMAXCONN);

// Check if the socket is listening
if (listenResult == SOCKET_ERROR)
{
    // Get the error code
    int errorCode = GETERROR();
       Networking::NetworkException netEx(serverSocket, errorCode, "Unable to listen to socket");
  throw netEx;

}

serverIsConnected = true;
// The server socket was created successfully
return true;

}


bool Networking::Server::CreateServerSocket(int _pPortNumber,  ServerType _pServerType)
{
    // Set the address family based on the server type
    int addressFamily = _pServerType == IPv4 ? AF_INET : AF_INET6;
ZeroMemory(&addressInfo, sizeof(addressInfo));
SetFamily(addressFamily);
SetSocketType(SOCK_STREAM);
SetProtocol(IPPROTO_TCP);

// Create a server socket
serverSocket = socket(addressInfo.ai_family, addressInfo.ai_socktype, addressInfo.ai_protocol);

// Check if the socket was successfully created
if (INVALIDSOCKET(serverSocket))
{
        int errorCode = GETERROR();
  Networking::NetworkException netEx(serverSocket, errorCode, "Unable to create socket");
  throw netEx;
}

// Set up the sockaddr_in structure
serverInfo.sin_family = addressInfo.ai_family;
serverInfo.sin_addr.s_addr = INADDR_ANY;
serverInfo.sin_port = htons(_pPortNumber);

// Bind the server socket to a port
int bindResult = bind(serverSocket, (sockaddr*)&serverInfo, sizeof(serverInfo));

// Check if the socket was successfully bound
if (bindResult == SOCKET_ERROR)
{
    // Get the error code
    int errorCode = GETERROR();
      Networking::NetworkException netEx(serverSocket, errorCode, "Unable to bind socket");
  throw netEx;

}

// Start listening for incoming connections
int listenResult = listen(serverSocket, SOMAXCONN);

// Check if the socket is listening
if (listenResult == SOCKET_ERROR)
{
    // Get the error code
    int errorCode = GETERROR();
       Networking::NetworkException netEx(serverSocket, errorCode, "Unable to listen to socket");
  throw netEx;

}

serverIsConnected = true;
// The server socket was created successfully
return true;


}

Networking::ClientConnection Networking::Server::Listen()
{
// Create a client connection structure to store information about the client
Networking::ClientConnection client;

// Accept a connection from a client

if(serverInfo.sin_family == AF_INET)
{
int clientAddrSize = sizeof(client.clientInfo);
client.clientSocket = accept(serverSocket, (sockaddr*)&client.clientInfo, (socklen_t *)&clientAddrSize);
}
else if (serverInfo.sin_family == AF_INET)
{
int clientAddrSize = sizeof(client.clientInfo6);
client.clientSocket = accept(serverSocket, (sockaddr*)&client.clientInfo6, (socklen_t *)&clientAddrSize);
}
// If there was an error, throw an exception
if ( INVALIDSOCKET(client.clientSocket))
{
    // Get the error code
    int errorCode = GETERROR();

    #ifdef _WIN32
    // Clean up the Windows Sockets DLL
    WSACleanup();
    #endif

    // Throw the error code
    throw errorCode;
}

// Add the client connection to the list of clients
clients.push_back(client);

return client;
}

void Networking::Server::SetSocketType(int _pSocktype)
{
	addressInfo.ai_socktype = _pSocktype;
}

void Networking::Server::SetProtocol(int _pProtocol)
{
	addressInfo.ai_protocol = _pProtocol;
}

void Networking::Server::SetFamily(int _pFamily)
{
	addressInfo.ai_family = _pFamily;
}


// Send data to the client
int Networking::Server::Send(char* _pSendBuffer, Networking::ClientConnection _pClient)
{
    // Send the data to the client
    int bytesSent = send(_pClient.clientSocket,_pSendBuffer, strlen(_pSendBuffer),0 );
 
     // If there was an error, throw an exception
    if(bytesSent ==SOCKET_ERROR)
    {
        // Get the error code
        int errorCode = GETERROR();
        
         // Close the socket
        CLOSESOCKET(_pClient.clientSocket);
        #ifdef _WIN32
        // Clean up the Windows Sockets DLL
        WSACleanup();
        #endif
        // Throw the error code
        throw errorCode;
    }

       // Return  the number of bytes sent if the data was sent successfully
    return bytesSent;
}

// Send data to a specified address and port
int Networking::Server::SendTo(char* _pBuffer, char* _pAddress, int _pPort)
{
    // Create a sockaddr_in structure to hold the address and port of the recipient
    sockaddr_storage sockAddress;
ZeroMemory(&sockAddress, sizeof(sockAddress));
if (serverInfo.sin_family == AF_INET)
{
    // Zero out the sockaddr_in structure
    
sockaddr_in* recipient =(sockaddr_in*) &sockAddress;
    // Set the address family, port, and address of the recipient
    recipient->sin_family = serverInfo.sin_family;
    recipient->sin_port = htons(_pPort);
    inet_pton(serverInfo.sin_family, _pAddress, &recipient->sin_addr);
}

else if(serverInfo.sin_family == AF_INET6)
{
     
    
sockaddr_in6* recipient =(sockaddr_in6*) &sockAddress;
    // Set the address family, port, and address of the recipient
    recipient->sin6_family = serverInfo.sin_family;
    recipient->sin6_port = htons(_pPort);
    inet_pton(serverInfo.sin_family, _pAddress, &recipient->sin6_addr);
}

    // Send the data to the specified recipient
    int bytesSent = sendto(serverSocket, _pBuffer, strlen(_pBuffer), 0, (sockaddr*)&sockAddress, sizeof(sockAddress));

    // If there was an error, throw an exception
    if(bytesSent == SOCKET_ERROR)
    {
        // Get the error code
        int errorCode = GETERROR();

         // Close the socket
        CLOSESOCKET(serverSocket);
        #ifdef _WIN32
        // Clean up the Windows Sockets DLL
        WSACleanup();
        #endif
        // Throw the error code
        throw errorCode;
    }

    // Return  the number of bytes sent if the data was sent successfully
    return bytesSent;
}


// Send data to all connected clients
int Networking::Server::SendToAll(char* _pSendBuffer)
{

     int bytesSent ;
    // Iterate over all connected clients
    for (auto client : clients)
    {
        // Send the data to the current client
        bytesSent = send(client.clientSocket, _pSendBuffer, strlen(_pSendBuffer), 0);

        // If there was an error, throw an exception
        if (bytesSent == SOCKET_ERROR)
        {
            // Get the error code
            int errorCode = GETERROR();

            // Close the socket
            CLOSESOCKET(client.clientSocket);

            #ifdef _WIN32
            // Clean up the Windows Sockets DLL
            WSACleanup();
            #endif

            // Throw the error code
            throw errorCode;
        }
    }

    // Return the number of bytes sent if the data was sent successfully
    return bytesSent;
}



// Send a file to the server
void Networking::Server::SendFile(const std::string& _pFilePath, Networking::ClientConnection client)
{
    // Open the file for reading
    std::ifstream file(_pFilePath, std::ios::in | std::ios::binary);

    // Check if the file was opened successfully
    if(!file)
    {
        // Throw an exception if the file could not be opened
        throw std::runtime_error("Error: Unable to open file '" + _pFilePath + "'");
    }

    // Read the file data into a buffer
    std::vector<char> fileData((std::istreambuf_iterator<char>(file)), (std::istreambuf_iterator<char>()));

    // Send the file data to the server
    Send(&fileData[0], client);
}


// Receive data from the server
std::vector <char> Networking::Server::Receive(Networking::ClientConnection client)
{
    // Initialize the number of bytes received to 0
    int bytesReceived =0;

    // Create a vector to store the received data
    std::vector<char> receiveBuffer;

   // Receive data from the server in a loop
    do{
        // Get the current size of the receive buffer
        int bufferStart = receiveBuffer.size();
        // Resize the buffer to make room for more data
        receiveBuffer.resize(bufferStart+512);
        // Receive data from the server
        bytesReceived = recv(client.clientSocket, &receiveBuffer[bufferStart],512,0);
       // Resize the buffer to the actual size of the received data
        receiveBuffer.resize(bufferStart + bytesReceived);
    } while (bytesReceived == 512);

// If there was an error, throw an exception
 if (bytesReceived == SOCKET_ERROR)
 {
    // Get the error code
    int errorCode = GETERROR();
    
     // Close the socket
    CLOSESOCKET(client.clientSocket);
    #ifdef _WIN32
    // Clean up the Windows Sockets DLL
    WSACleanup();
    #endif
    // Throw the error code
    throw errorCode;
 }   
 // Return the vector containing the received data
 return receiveBuffer;
}


// Receive data from a specified address and port
std::vector<char> Networking::Server::ReceiveFrom(char* _pAddress, int _pPort)
{
       // Initialize the number of bytes received to 0
    int bytesReceived =0;

    sockaddr_storage sockAddress;
    ZeroMemory(&sockAddress, sizeof(sockAddress));
if(serverInfo.sin_family == AF_INET)
{

    // Create a sockaddr_in structure to hold the address and port of the sender
    sockaddr_in* sender = (sockaddr_in*) &sockAddress;

    // Set the address family, port, and address of the sender
    sender->sin_family = AF_INET;
    sender->sin_port = htons(_pPort);
    inet_pton(AF_INET, _pAddress, &sender->sin_addr);
}

else if (serverInfo.sin_family == AF_INET6)
{
    // Create a sockaddr_in structure to hold the address and port of the sender
    sockaddr_in6* sender = (sockaddr_in6*) &sockAddress;

    // Set the address family, port, and address of the sender
    sender->sin6_family = AF_INET6;
    sender->sin6_port = htons(_pPort);
    inet_pton(AF_INET6, _pAddress, &sender->sin6_addr);
}
    // Receive data from the specified sender
    std::vector<char> receiveBuffer;


   // Receive data from the server in a loop
    do{
        // Get the current size of the receive buffer
        int bufferStart = receiveBuffer.size();
        // Resize the buffer to make room for more data
        receiveBuffer.resize(bufferStart+512);
        // Receive data from the server
        bytesReceived = recvfrom(serverSocket, &receiveBuffer[bufferStart], 512, 0, (sockaddr*)&sockAddress, (socklen_t*)sizeof(sockAddress));
       // Resize the buffer to the actual size of the received data
        receiveBuffer.resize(bufferStart + bytesReceived);
    } while (bytesReceived == 512);

 
    // If there was an error, throw an exception
    if(bytesReceived == SOCKET_ERROR)
    {
        // Get the error code
        int errorCode = GETERROR();

          // Close the socket
        CLOSESOCKET(serverSocket);
        #ifdef _WIN32
        // Clean up the Windows Sockets DLL
        WSACleanup();
        #endif
        // Throw the error code
        throw errorCode;
    }

    // Return the received data
    return receiveBuffer;
}

// Receive a file from the server
void Networking::Server::ReceiveFile(const std::string& _pFilePath, Networking::ClientConnection client)
{
    // Open the file for writing
    std::ofstream file(_pFilePath, std::ios::out | std::ios::binary);

    // Check if the file was opened successfully
    if(!file)
    {
        // Throw an exception if the file could not be opened
        throw std::runtime_error("Error: Unable to open file '" + _pFilePath + "'");
    }

    // Receive the file data from the server
    std::vector<char> fileData = Receive(client);

    // Write the file data to the file
    file.write(&fileData[0], fileData.size());
}


bool Networking::Server::ServerIsRunning()
{
	return serverIsConnected;
}


void Networking::Server::DisconnectClient(Networking::ClientConnection _pClient)
{
    // Disconnect the client from the server
     // Shut down the server socket
    #ifdef _WIN32
    int shutdownResult = shutdown(_pClient.clientSocket, SD_BOTH);
    #else
    int shutdownResult = shutdown(_pClient.clientSocket, SHUT_RDWR);
    #endif
    // If there was an error, throw an exception
    if (shutdownResult == SOCKET_ERROR)
    {
        // Get the error code
        int errorCode = GETERROR();

        #ifdef _WIN32
        // Clean up the Windows Sockets DLL
        WSACleanup();
        #endif

        // Throw the error code
        throw errorCode;
    }

    // Close the socket
    CLOSESOCKET(_pClient.clientSocket);

    // Remove the client from the list of clients
    clients.erase(std::remove(clients.begin(), clients.end(), _pClient), clients.end());
}


// Shut down the server
void Networking::Server::Shutdown()
{
    // Disconnect the server socket
    if (serverIsConnected)
    {
        // Shut down the server socket
        #ifdef _WIN32
         int shutdownResult= shutdown(serverSocket, SD_BOTH);
        #else
        int shutdownResult= shutdown(serverSocket, SHUT_RDWR);
        #endif


        // Check if the socket was successfully shut down
        if (shutdownResult == SOCKET_ERROR)
        {
            // Get the error code
            int errorCode = GETERROR();

            // Close the socket
            CLOSESOCKET(serverSocket);

            #ifdef _WIN32
            // Clean up the Windows Sockets DLL
            WSACleanup();
            #endif

            // Throw the error code
            throw errorCode;
        }

        // Close the server socket
        CLOSESOCKET(serverSocket);

        #ifdef _WIN32
        // Clean up the Windows Sockets DLL
        WSACleanup();
        #endif

        // Set the server connected flag to false
        serverIsConnected = false;
    }
}

// Return a vector of ClientConnection objects representing the currently connected clients
std::vector<Networking::ClientConnection> Networking::Server::getClients() const
{
    return clients;
}

//Handles Errors

void Networking::Server::ErrorHandling(Networking::NetworkException _pNetEx)
{
    switch(_pNetEx.GetErrorCode()){
        #ifdef _WIN32
            case WSASYSNOTREADY:
                 std::cerr<<"The underlying network subsystem is not ready for network communication."<<std::endl;
                break;
            case WSAVERNOTSUPPORTED:
                std::cerr<<"The version of Windows Sockets support requested is not provided by this particular Windows Sockets implementation."<<std::endl;
                break;
            case WSAEINPROGRESS:
                std::cerr<<"A blocking Windows Sockets 1.1 operation is in progress."<<std::endl;
                if(!INVALIDSOCKET(_pNetEx.GetSocket()))
                    CLOSESOCKET(_pNetEx.GetSoclet());
                  WSACleanup();
                break;
            case WSAEPROCLIM:
                std::cerr<<"A limit on the number of tasks supported by the Windows Sockets implementation has been reached."<<std::endl;
                break;
            case WSAEFAULT:
                std::cerr<<"The lpWSAData parameter is not a valid pointer."<<std::endl;
                break;
            case WSANOTINITIALISED:
                std::cerr<<"A successful WSAStartup call must occur before using this function."<<std::endl;
                break;
            case WSAENETDOWN:
                std::cerr<<"The network subsystem or the associated service provider has failed."<<std::endl;
                WSACleanup();
                break;
            case WSAEAFNOSUPPORT:
                std::cerr<<"The specified address family is not supported. For example, an application tried to create a socket for the AF_IRDA address family but an infrared adapter and device driver is not installed on the local computer."<<std::endl;
                WSACleanup();
                break;
            case WSAEMFILE:
                std::cerr<<"No more socket descriptors are available."<<std::endl;
                WSACleanup();
                break;
            case WSAINVAL:
                std::cerr<<"An invalid argument was supplied. This error is returned if the af parameter is set to AF_UNSPEC and the type and protocol parameter are unspecified."<<std::endl;
                WSACleanup();
                break;
            case WSAEINVALIDPROVIDER:
                std::cerr<<"The service provider returned a version other than 2.2."<<std::endl;
                WSACleanup();
                break;
            case WSAEINVALIDPROCTABLE:
                std::cerr<<"The service provider returned an invalid or incomplete procedure table to the WSPStartup."<<std::endl;
                WSACleanup();
                break;
            case WSAENOBUFS:
                std::cerr<<"o buffer space is available. The socket cannot be created."<<std::endl;
                WSACleanup();
                break;
            case WSAEPROTONOSUPPORT:
                std::cerr<<"he specified protocol is not supported."<<std::endl;
                WSACleanup();
                break;
            case WSAEPROTOTYPE:
                std::cerr<<"he specified protocol is the wrong type for this socket."<<std:endl;
                WSACleanup();
                break;
            case WSAEPROVIDERFAILEDINIT:
                std::cerr<<"The service provider failed to initialize. This error is returned if a layered service provider (LSP) or namespace provider was improperly installed or the provider fails to operate correctly."<<std::endl;
                WSACleanup();
                break;
            case WSAESOCKTNOSUPPORT:
                std::cerr<<"The specified socket type is not supported in this address family."<<std::endl;
                WSACleanup();
                break;
        #else
            case EACCES:
                std::cerr<<"The process does not have the required privileges to create a socket."<<std::endl;
                break;
            case EAFNOSUPPORT:
                std::cerr<<"The specified address domain is not supported."<<std::endl;
                break;
            case EINVAL:
                std::cerr<<"The specified address domain, socket type, or protocol is invalid."<<std::endl;
                break;
            case EMFILE:
                std::cerr<<"The process has reached its limit for the number of open file descriptors."<<std::endl;
                break;
            case ENFILE:
                std::cerr<<"The system has reached its limit for the total number of open files."<<std::endl;
                break;
            case ENOBUFS:
                std::cerr<<"There is not enough memory available to create a new socket."<<std::endl;
                break;
            case ENOMEM:
                std::cerr<<"There is not enough memory available to create a new socket."<<std::endl;
                break;
            case EPROTONOSUPPORT:
                std::cerr<<"The specified protocol is not supported."<<std::endl;
                break;
        #endif
    }
}
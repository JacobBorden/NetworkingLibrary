# Client Class
The Client class provides a basic framework for implementing a client in a network application. The Client class has several member functions that can be used to initialize, create, and connect a socket, send and receive data, and manage the connection.

## Usage
To use the Client class, include the client.h header file in your project and create an instance of the Client class. You can specify a host and port to automatically connect to the host when the Client object is constructed, or you can use the various member functions to create and connect the socket manually.

Here is an example of how to use the Client class:

```C++
#include "client.h"

using namespace Networking;

int main() {
    // Create a client object and connect to localhost on port 8080
    Client client("localhost", 8080);

    // Send a message to the server
    client.Send("Hello, server!");

    // Receive a response from the server
    std::vector<char> response = client.Receive();

    // Print the response
    std::cout << "Response from server: " << response << std::endl;

    // Disconnect from the server
    client.Disconnect();

    return 0;
}
```
## API

### Client()
Constructs a new client object with no host or port specified.

### Client(char* _pHost, int _pPortNumber)
Constructs a new client object with the specified host and port. The client will automatically connect to the host on the specified port.

### ~Client()
Destroys the client object and cleans up any resources it was using.

### bool InitClientSocket()
Initializes the client socket.

### bool CreateClientTCPSocket(char* _pHost, int _pPort)
Creates a TCP client socket with the specified host and port.

### bool CreateClientUDPSocket(char* _pHost, int pPort)
Creates a UDP client socket with the specified host and port.

### bool CreateClientSocket(char* _pHost, int pPort)
Creates a client socket with the specified host, port, socket type, and protocol.

### bool ConnectClientSocket()
Connects the client socket to the specified host on the specified port.

### void SetSocketType(int _pSocktype)
Sets the socket type for the client socket.

### void SetProtocol(int _pProtocol)
Sets the protocol for the client socket.

### void SetFamily(int _pFamily)
Sets the address family for the client socket

### int Send(char* _pSendBuffer)
Sends the specified data buffer to the connected host. Returns the number of bytes sent, or -1 on error.

### std::vector<char> Receive()
Receives data from the connected host and stores it in a vector.

### bool Disconnect()
Disconnects the client socket from the host.

### bool IsConnected()
Returns whether the client is currently connected to a host.

## Private Members
The Client class has the following private member variables:

### bool clientIsConnected
Whether the client is currently connected to a host.

### SOCKET connectionSocket
The client socket.

### WSADATA wsaData
Windows-specific data for socket initialization.

### addrinfo addressInfo
Address information for the client socket.

### addrinfo* hostAddressInfo
Pointer to the address information for the host the client is connected to.

# Server Class

The class Server is used for creating a server socket that can accept incoming connections from clients and send/receive data to/from them.

Server(): The default constructor that initializes the server using the InitServer() method.
Server(int _pPortNumber): This constructor initializes the server using the InitServer() method and creates the server socket with the specified port number using the CreateServerSocket() method.
~Server(): The destructor that cleans up any resources allocated by the class.
bool InitServer(): This method initializes the server by starting the Windows Sockets DLL (on Windows) and returning a boolean indicating whether the initialization was successful.
bool CreateServerSocket(int _pPortNumber): This method creates the server socket and binds it to the specified port number. It returns a boolean indicating whether the socket was created and bound successfully.
Networking::ClientConnection Listen(): This method listens for incoming connections from clients and returns a Networking::ClientConnection struct containing information about the client that connected.
void SetSocketType(int _pSockType): This method sets the socket type (e.g. SOCK_STREAM or SOCK_DGRAM) for the server socket.
void SetFamily(int _pFamily): This method sets the address family (e.g. AF_INET or AF_INET6) for the server socket.
void SetProtocol(int _pProtocol): This method sets the protocol (e.g. IPPROTO_TCP or IPPROTO_UDP) for the server socket.
int Send(char* _pSendBuffer, Networking::ClientConnection _pClient): This method sends data to the specified client. It returns the number of bytes sent.
int SendTo(char* _pBuffer, char* _pAddress, int _pPort): This method sends data to the specified address and port. It returns the number of bytes sent.
int SendToAll(char* _pSendBuffer): This method sends data to all connected clients. It returns the number of bytes sent to each client.
void SendFile(const std::string& _pFilePath, Networking::ClientConnection client): This method sends a file to the specified client.
std::vector<char> Receive(Networking::ClientConnection client): This method receives data from the specified client and returns it as a vector of chars.
std::vector<char> ReceiveFrom(char* _pAddress, int _pPort): This method receives data from the specified address and port and returns it as a vector of chars.
void ReceiveFile(const std::string& _pFilePath, Networking::ClientConnection client): This method receives a file from the specified client and saves it to the specified file path.
bool ServerIsRunning(): This method returns a boolean indicating whether the server is currently running (i.e. has a valid server socket).
void Shutdown(): This method shuts down the server socket.


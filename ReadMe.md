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

### bool CreateClientUDPSocket(char* _pHost, int _pPort)
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



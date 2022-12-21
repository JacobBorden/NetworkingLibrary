# Change Log

## v0.3

### Server

- Added support for IPv6
- Moved error messages to the error stream instead of output stream
- Improved error handling

## v0.2

### Client

- Added a SOCKET_ERROR macro to define the SOCKET_ERROR constant for Linux.
-  Added a ReceiveFrom() method to receive data from a specified address and port.
-  Added a SendTo() method to send data to a specified address and port.
-  Added a GetHostName() method to get the hostname of the client.
-  Added a GetServerHostName() method to get the hostname of the server
-  Added CreateClientUDPSocket() method
- Added comments to the code to explain what each method does.
- Added SendFile() and ReceiveFile()

### Server

- Added a SendToAll() method to send data to all connected clients
- Added a Shutdown() method to gracefully shutdown the server
- Added a Disconnect() method to disconnect a specified client from the server and remove them from the list of clients
- Added a getClients() method to return the list of currently connected clients.
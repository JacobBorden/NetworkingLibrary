# Networking Library for C++

This repository contains a C++ library for creating and managing a network server. The `Networking::Server` class provides a simple interface for creating a server socket, listening for client connections, sending and receiving data, and managing connected clients. The code uses platform-specific APIs and is compatible with Windows and Linux operating systems.

## Features
- Simple interface for creating and managing a network server
- Support for sending and receiving data
- Support for sending and receiving files
- Support for managing connected clients

## Requirements
- A C++11-compatible compiler
- Windows: [Windows Sockets 2](https://en.wikipedia.org/wiki/Windows_Sockets)
- Linux: [Berkeley sockets](https://en.wikipedia.org/wiki/Berkeley_sockets)

## Usage

```cpp
// Include the server header file
#include "server.h"

// Use the Networking namespace
using namespace Networking;

int main()
{



// This will open a server on port 8080 with IPv4
Networking::Server server;

// This will open a server on port 80 with IPv4
Networking::Server server(80);

// This will open a server on port 8080 with IPv6
Networking::Server server(ServerType::IPv6);

// This will open a server on port 80 with IPv6
Networking::Server server(80, ServerType::IPv6);


  // Initialize the server
  if (server.ServerIsConnected())

  {
    std::cout << "Server initialized" << std::endl;
  }

  // Listen for incoming connections
  while (true)
  {
    // Accept a client connection

    ClientConnection client = server.Accept();


    // Send a message to the client
    char buffer[] = "Hello from the server!";
    server.Send(buffer, client);
  }

  return 0;
}



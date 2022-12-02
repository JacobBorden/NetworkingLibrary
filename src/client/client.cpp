#include "client.h"

Networking::Client::Client()
{
    try {
        Client::InitClientSocket();
    }

    catch(int errorCode){
        std::cout<<"Exception thrown. Error Code"<<errorCode;
    }
}

Networking::Client::Client(char* _pHost, int _pPortNumber)
{
    try{
        Client::InitClientSocket();
        Client::CreateClientTCPSocket(_pHost, _pPortNumber);
        Client::ConnectClientSocket();
    }
    catch(int errorCode){
        std::cout<<"Exception thrown. Error Code "<<errorCode;
    }
}

Networking::Client::~Client()
{
}

bool Networking::Client::InitClientSocket()
{
    #ifdef _WIN32
    int errorCode = WSAStartup(VERSIONREQUESTED, &this->wsaData);
    if(errorCode)
        throw errorCode;
    #endif
    return true;
}

bool Networking::Client::Client::CreateClientTCPSocket(char* _pHost, int _pPort)
{
    
    ZeroMemory(&addressInfo, sizeof(addressInfo));
    Client::SetFamily(AF_INET);
    Client::SetSocketType(SOCK_STREAM);
    Client::SetProtocol(IPPROTO_TCP); 

    int errorCode = getaddrinfo((PCSTR)_pHost, (PCSTR)getservbyport(_pPort, NULL)->s_name,(const addrinfo*) &addressInfo,&hostAddressInfo );

if(errorCode)
{
    #ifdef _WIN32
    WSACleanup();
    #endif
    throw errorCode;
}

    connectionSocket = socket(hostAddressInfo->ai_family, hostAddressInfo->ai_socktype,  hostAddressInfo->ai_protocol);

    if(INVALIDSOCKET(connectionSocket))
    {
        int errorCode = GETERROR();
        #ifdef _WIN32
       WSACleanup();
        #endif
        throw errorCode;
    }   
    return true;
}

bool Networking::Client::Client::ConnectClientSocket()
{
int result = connect(connectionSocket, (sockaddr*) &hostAddressInfo->ai_addr, hostAddressInfo->ai_addrlen);
if (result != 0)
{
    int errorCode = GETERROR();
    CLOSESOCKET(connectionSocket);
    #ifdef _WIN32
    WSACleanup();
    #endif
    throw errorCode;
}
clientIsConnected = true;
return true;

}

void Networking::Client::Client::SetSocketType(int _pSocketType)
{
    addressInfo.ai_socktype = _pSocketType;
}

void Networking::Client::Client::SetFamily(int _pFamily)
{
    addressInfo.ai_family = _pFamily;
}

void Networking::Client::Client::SetProtocol(int _pProtocol)
{
    addressInfo.ai_protocol = _pProtocol;
}

int Networking::Client::Client::Send(char* _pSendBuffer)
{
    int bytesSent = send(connectionSocket,_pSendBuffer, strlen(_pSendBuffer),0 );
    if(bytesSent == -1)
    {
        int errorCode = GETERROR();
        CLOSESOCKET(connectionSocket);
        #ifdef _WIN32
        WSACleanup();
        #endif
        throw errorCode;
    }
    return bytesSent;
}

std::vector <char> Networking::Client::Client::Receive()
{
    int bytesReceived =0;
    std::vector<char> receiveBuffer;

    do{
        int bufferStart = receiveBuffer.size();
        receiveBuffer.resize(bufferStart+512);
        bytesReceived = recv(connectionSocket, &receiveBuffer[bufferStart],512,0);
        receiveBuffer.resize(bufferStart + bytesReceived);
    } while (bytesReceived == 512);

 if (bytesReceived == -1)
 {
    int errorCode = GETERROR();
    CLOSESOCKET(connectionSocket);
    #ifdef _WIN32
    WSACleanup();
    #endif
 }   
}
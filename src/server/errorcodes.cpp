#include "errorcodes.h"

void Networking::ThrowSocketException(int socket, int errorCode)
{
	throw Networking::NetworkException(socket, errorCode, Networking::Error::socketMap.at(errorCode));
}

void Networking::ThrowBindException(int socket, int errorCode)
{
	throw Networking::NetworkException(socket, errorCode, Networking::Error::bindMap.at(errorCode));
}

void Networking::ThrowListenException(int socket, int errorCode)
{
	throw Networking::NetworkException(socket, errorCode, Networking::Error::listenMap.at(errorCode));
}

void Networking::ThrowAcceptException(int socket, int errorCode)
{
	throw Networking::NetworkException(socket, errorCode, Networking::Error::acceptMap.at(errorCode));
}

void Networking::ThrowSendException(int socket, int errorCode)
{
	throw Networking::NetworkException(socket, errorCode, Networking::Error::sendMap.at(errorCode));
}

void Networking::ThrowReceiveException(int socket, int errorCode)
{
	throw Networking::NetworkException(socket, errorCode, Networking::Error::receiveMap.at(errorCode));
}


void Networking::ThrowShutdownException(int socket, int errorCode)
{
	throw Networking::NetworkException(socket, errorCode, Networking::Error::shutdownMap.at(errorCode));
}

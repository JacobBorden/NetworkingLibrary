#include "errorcodes.h"

void Networking::ThrowAcceptException(int socket, int errorCode)
{
	throw Networking::NetworkException(socket, errorCode, Networking::Error::acceptMap.at(errorCode));
}
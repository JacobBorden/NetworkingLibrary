#pragma once
#ifndef _ERROR_CODES_
#define _ERROR_CODES_

#include <unordered_map>
#include <string>
#include "networkexception.h"



namespace Networking
{

namespace Error
{
typedef std::unordered_map<int, std::string>  ErrorMap;

static ErrorMap listenMap = {
#ifdef _WIN32
// Windows specific error messages here
#else
	{EBADF, "The socket parameter is not within the acceptable range for a socket descriptor."},
	{EINTR, "A signal interrupted the accept() call before any connections were available."},
	{EINVAL, "listen() was not called for socket descriptor socket."},
	{EMFILE, "An attempt was made to open more than the maximum number of file descriptors allowed for this process."},
	{ENFILE, "The maximum number of file descriptors in the system are already open."},
	{ENOBUFS, "Insufficient buffer space is available to create the new socket."},
	{EOPNOTSUPP, "The socket type of the specified socket does not support accepting connections."}
#endif
};
}

void ThrowAcceptException(int socket, int errorCode);

}
#endif
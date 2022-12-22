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

static const ErrorMap acceptMap = {
#ifdef _WIN32
// Windows specific error messages here
	{WSAEACCES, "An attempt was made to access a socket in a way forbidden by its access permissions."},
	{WSAEFAULT, "The system detected an invalid pointer address in attempting to use a pointer argument in a call."},
	{WSAEINPROGRESS, "A blocking Windows Sockets 1.1 call is in progress, or the service provider is still processing a callback function."},
	{WSAEINTR, "A blocking Windows Socket 1.1 call was canceled through WSACancelBlockingCall."},
	{WSAEINVAL, "The listen function was not invoked prior to accept."},
	{WSAEMFILE, "No more file descriptors are available."},
	{WSAENOBUFS, "No buffer space is available."},
	{WSAENOTSOCK, "The descriptor is not a socket."},
	{WSAEOPNOTSUPP, "The referenced socket is not of a type that supports the accept function."},
	{WSAEWOULDBLOCK,"The socket is marked as nonblocking and no connections are present to be accepted."}
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
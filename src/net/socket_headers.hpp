#pragma once

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#define SOCKET_INVALID INVALID_SOCKET
using SockLen = int;
using SendReceiveSize = int;
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#define SOCKET_INVALID (-1)
#define SOCKET_ERROR (-1)
#define closesocket close
using SockLen = unsigned int;
using SendReceiveSize = std::size_t;
#endif

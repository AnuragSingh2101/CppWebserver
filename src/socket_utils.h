#ifndef SOCKET_UTILS_H
#define SOCKET_UTILS_H

#include <winsock2.h>
#include <string>

// Reliably send all data over Winsock TCP socket
bool sendAll(SOCKET socket, const std::string& data);

#endif // SOCKET_UTILS_H

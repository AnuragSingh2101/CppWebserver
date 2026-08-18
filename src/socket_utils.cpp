#include "socket_utils.h"

bool sendAll(SOCKET socket, const std::string& data) {
    size_t totalSent = 0;
    while (totalSent < data.size()) {
        int bytesSent = send(
            socket,
            data.c_str() + totalSent,
            static_cast<int>(data.size() - totalSent),
            0
        );

        if (bytesSent == SOCKET_ERROR) {
            return false;
        }

        if (bytesSent == 0) {
            // Socket was closed by peer
            return false;
        }

        totalSent += bytesSent;
    }
    return true;
}

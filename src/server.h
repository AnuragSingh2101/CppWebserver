#ifndef SERVER_H
#define SERVER_H

#include <winsock2.h>
#include <string>
#include <atomic>
#include "server_config.h"

class HttpServer {
public:
    explicit HttpServer(const ServerConfig& config);
    ~HttpServer();

    // Run the server (blocking loop). Returns false if startup fails.
    bool run();

    // Stop the server (thread-safe, will unblock run())
    void stop();

private:
    bool initializeWinsock();
    void handleClient(SOCKET clientSocket);
    bool receiveHttpRequest(SOCKET clientSocket, std::string& requestData, std::string& connectionBuffer);

    ServerConfig config;
    std::atomic<bool> stopping;
    SOCKET serverSocket;
    WSADATA wsaData;
    bool wsaInitialized;
};

#endif // SERVER_H

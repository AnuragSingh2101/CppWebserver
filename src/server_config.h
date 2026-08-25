#ifndef SERVER_CONFIG_H
#define SERVER_CONFIG_H

#include <string>

struct ServerConfig {
    int port = 8080;
    std::string host = "127.0.0.1";
    int threadCount = 4;
    int requestTimeoutMs = 5000;
    size_t maxHeaderSize = 16384;      // 16 KB
    size_t maxBodySize = 1048576;      // 1 MB
    std::string publicDir = "public";

    static ServerConfig loadFromFile(const std::string& filePath);
    bool overrideWithArgs(int argc, char* argv[]);
};

#endif // SERVER_CONFIG_H

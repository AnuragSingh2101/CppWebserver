#include "server.h"
#include "logger.h"
#include "socket_utils.h"
#include "concurrency/thread_pool.h"
#include "metrics.h"
#include "http_request.h"
#include "http_response.h"
#include "route_handler.h"
#include "router.h"

#include <iostream>
#include <sstream>
#include <chrono>
#include <algorithm>
#include <filesystem>

#pragma comment(lib, "ws2_32.lib")

// Define the extern global for router compatibility
std::string g_documentRoot = "public";

struct ConnectionGuard {
    ConnectionGuard() { ServerMetrics::getInstance().addConnection(); }
    ~ConnectionGuard() { ServerMetrics::getInstance().removeConnection(); }
};

HttpServer::HttpServer(const ServerConfig& config)
    : config(config), stopping(false), serverSocket(INVALID_SOCKET), wsaInitialized(false) {
}

HttpServer::~HttpServer() {
    stop();
}

bool HttpServer::initializeWinsock() {
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        Logger::error("WSAStartup failed. Error Code: " + std::to_string(result));
        return false;
    }
    wsaInitialized = true;
    Logger::info("Winsock Initialized Successfully!");
    return true;
}

void HttpServer::stop() {
    stopping = true;
    if (serverSocket != INVALID_SOCKET) {
        SOCKET temp = serverSocket;
        serverSocket = INVALID_SOCKET;
        closesocket(temp);
    }
    if (wsaInitialized) {
        WSACleanup();
        wsaInitialized = false;
    }
}

bool HttpServer::receiveHttpRequest(SOCKET clientSocket, std::string& requestData, std::string& connectionBuffer) {
    constexpr int BUFFER_SIZE = 4096;
    char buffer[BUFFER_SIZE];

    size_t headerEnd = connectionBuffer.find("\r\n\r\n");

    // Receive until we have complete HTTP headers
    while (headerEnd == std::string::npos) {
        int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0);

        if (bytesReceived <= 0) {
            if (bytesReceived == SOCKET_ERROR) {
                int err = WSAGetLastError();
                if (err == WSAETIMEDOUT || err == WSAEWOULDBLOCK) {
                    Logger::error("Receive timeout during header read");
                } else {
                    Logger::error("Receive error during header read: " + std::to_string(err));
                }
            }
            return false;
        }

        ServerMetrics::getInstance().addBytesReceived(bytesReceived);
        connectionBuffer.append(buffer, bytesReceived);
        headerEnd = connectionBuffer.find("\r\n\r\n");

        if (connectionBuffer.size() > config.maxHeaderSize) {
            // Send 413 Payload Too Large
            std::string response = "HTTP/1.1 413 Payload Too Large\r\nContent-Type: text/plain\r\nContent-Length: 17\r\nConnection: close\r\n\r\nPayload Too Large";
            sendAll(clientSocket, response);
            return false;
        }
    }

    // Validate Content-Length in headers
    std::string headerSection = connectionBuffer.substr(0, headerEnd);
    std::string lowerHeaders = headerSection;
    std::transform(lowerHeaders.begin(), lowerHeaders.end(), lowerHeaders.begin(), ::tolower);

    size_t pos = 0;
    std::string firstVal;
    bool hasContentLength = false;
    bool duplicateContentLength = false;

    while ((pos = lowerHeaders.find("content-length:", pos)) != std::string::npos) {
        if (pos > 0 && lowerHeaders[pos - 1] != '\n' && lowerHeaders[pos - 1] != '\r') {
            pos += 15;
            continue;
        }

        size_t lineEnd = lowerHeaders.find("\r\n", pos);
        if (lineEnd == std::string::npos) {
            lineEnd = lowerHeaders.size();
        }

        std::string val = headerSection.substr(pos + 15, lineEnd - (pos + 15));
        while (!val.empty() && (val.front() == ' ' || val.front() == '\t')) val.erase(val.begin());
        while (!val.empty() && (val.back() == ' ' || val.back() == '\t' || val.back() == '\r')) val.pop_back();

        if (hasContentLength) {
            if (val != firstVal) {
                duplicateContentLength = true;
            }
        } else {
            firstVal = val;
            hasContentLength = true;
        }
        pos = lineEnd;
    }

    if (duplicateContentLength) {
        std::string response = "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\nContent-Length: 32\r\nConnection: close\r\n\r\nConflicting Content-Length Headers";
        sendAll(clientSocket, response);
        return false;
    }

    int contentLength = 0;
    if (hasContentLength) {
        if (firstVal.empty()) {
            std::string response = "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\nContent-Length: 22\r\nConnection: close\r\n\r\nInvalid Content-Length";
            sendAll(clientSocket, response);
            return false;
        }
        for (char c : firstVal) {
            if (!isdigit(static_cast<unsigned char>(c))) {
                std::string response = "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\nContent-Length: 22\r\nConnection: close\r\n\r\nInvalid Content-Length";
                sendAll(clientSocket, response);
                return false;
            }
        }
        try {
            unsigned long long parsedLen = std::stoull(firstVal);
            if (parsedLen > config.maxBodySize) {
                std::string response = "HTTP/1.1 413 Payload Too Large\r\nContent-Type: text/plain\r\nContent-Length: 17\r\nConnection: close\r\n\r\nPayload Too Large";
                sendAll(clientSocket, response);
                return false;
            }
            contentLength = static_cast<int>(parsedLen);
        } catch (...) {
            std::string response = "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\nContent-Length: 22\r\nConnection: close\r\n\r\nInvalid Content-Length";
            sendAll(clientSocket, response);
            return false;
        }
    }

    size_t bodyStart = headerEnd + 4;

    // Receive remaining body
    while (connectionBuffer.size() < bodyStart + contentLength) {
        int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0);

        if (bytesReceived <= 0) {
            if (bytesReceived == SOCKET_ERROR) {
                int err = WSAGetLastError();
                if (err == WSAETIMEDOUT || err == WSAEWOULDBLOCK) {
                    Logger::error("Receive timeout during body read");
                } else {
                    Logger::error("Receive error during body read: " + std::to_string(err));
                }
            }
            return false;
        }

        ServerMetrics::getInstance().addBytesReceived(bytesReceived);
        connectionBuffer.append(buffer, bytesReceived);

        if (connectionBuffer.size() > config.maxHeaderSize + contentLength) {
            std::string response = "HTTP/1.1 413 Payload Too Large\r\nContent-Type: text/plain\r\nContent-Length: 17\r\nConnection: close\r\n\r\nPayload Too Large";
            sendAll(clientSocket, response);
            return false;
        }
    }

    // Copy this request out and keep remainder in connectionBuffer
    requestData = connectionBuffer.substr(0, bodyStart + contentLength);
    connectionBuffer.erase(0, bodyStart + contentLength);

    return true;
}

void HttpServer::handleClient(SOCKET clientSocket) {
    ConnectionGuard connGuard;
    int timeout = config.requestTimeoutMs;
    if (setsockopt(
            clientSocket,
            SOL_SOCKET,
            SO_RCVTIMEO,
            reinterpret_cast<const char*>(&timeout),
            sizeof(timeout)) == SOCKET_ERROR) {
        Logger::error("Failed to set SO_RCVTIMEO");
    }
    if (setsockopt(
            clientSocket,
            SOL_SOCKET,
            SO_SNDTIMEO,
            reinterpret_cast<const char*>(&timeout),
            sizeof(timeout)) == SOCKET_ERROR) {
        Logger::error("Failed to set SO_SNDTIMEO");
    }

    Logger::info("Client connected");

    std::string connectionBuffer;
    bool keepAlive = true;
    int requestCount = 0;
    const int maxKeepAliveRequests = 100;

    while (keepAlive && !stopping && requestCount < maxKeepAliveRequests) {
        std::string rawRequest;
        if (!receiveHttpRequest(clientSocket, rawRequest, connectionBuffer)) {
            break;
        }

        auto startTime = std::chrono::high_resolution_clock::now();
        requestCount++;
        HttpRequest request(rawRequest);

        ServerMetrics::getInstance().incrementTotalRequests();

        if (!request.isValid()) {
            ServerMetrics::getInstance().incrementClientErrors();
            std::string errMessage = request.getErrorMessage();
            int errCode = request.getErrorCode();
            Logger::error("HTTP Request validation failed: " + errMessage);

            HttpResponse response(errCode, "text/plain", errMessage);
            response.setHeader("Connection", "close");

            std::string responseStr = response.toString();
            sendAll(clientSocket, responseStr);
            ServerMetrics::getInstance().addBytesSent(responseStr.size());
            break;
        }

        std::string connectionHeader = request.getHeader("Connection");
        std::transform(connectionHeader.begin(), connectionHeader.end(), connectionHeader.begin(), ::tolower);

        if (request.getVersion() == "HTTP/1.1") {
            keepAlive = (connectionHeader != "close");
        } else {
            keepAlive = (connectionHeader == "keep-alive");
        }

        HttpResponse response = RouteHandler::handleRequest(request);
        if (request.getMethod() == "HEAD") {
            response.setSendBody(false);
        }

        if (keepAlive) {
            response.setHeader("Connection", "keep-alive");
            response.setHeader("Keep-Alive", "timeout=5, max=" + std::to_string(maxKeepAliveRequests - requestCount));
        } else {
            response.setHeader("Connection", "close");
        }

        std::string responseStr = response.toString();
        bool success = sendAll(clientSocket, responseStr);

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

        int status = response.getStatusCode();
        if (status >= 200 && status < 300) {
            ServerMetrics::getInstance().incrementSuccessfulRequests();
        } else if (status >= 400 && status < 500) {
            ServerMetrics::getInstance().incrementClientErrors();
        } else if (status >= 500 && status < 600) {
            ServerMetrics::getInstance().incrementServerErrors();
        }

        if (!success) {
            Logger::error("Send failed. Error Code: " + std::to_string(WSAGetLastError()));
            break;
        } else {
            ServerMetrics::getInstance().addBytesSent(responseStr.size());

            std::string logMsg = request.getMethod() + " " + request.getPath() + " " + std::to_string(status) + " " + std::to_string(responseStr.size()) + "B " + std::to_string(duration) + "ms";
            if (status >= 400 && status < 500) {
                Logger::warn(logMsg);
            } else if (status >= 500) {
                Logger::error(logMsg);
            } else {
                Logger::info(logMsg);
            }
        }
    }

    closesocket(clientSocket);
    Logger::info("Client disconnected");
}

bool HttpServer::run() {
    g_documentRoot = config.publicDir;

    // Validate root directory exists
    if (!std::filesystem::exists(g_documentRoot) || !std::filesystem::is_directory(g_documentRoot)) {
        Logger::error("Document root directory does not exist or is not a directory: " + g_documentRoot);
        return false;
    }

    if (!initializeWinsock()) {
        return false;
    }

    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) {
        Logger::error("Socket creation failed. Error Code: " + std::to_string(WSAGetLastError()));
        WSACleanup();
        return false;
    }

    // Set SO_REUSEADDR
    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt)) == SOCKET_ERROR) {
        Logger::warn("Failed to set SO_REUSEADDR");
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    if (config.host == "0.0.0.0" || config.host.empty()) {
        serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        serverAddress.sin_addr.s_addr = inet_addr(config.host.c_str());
        if (serverAddress.sin_addr.s_addr == INADDR_NONE) {
            Logger::error("Invalid host IP address: " + config.host);
            closesocket(serverSocket);
            WSACleanup();
            return false;
        }
    }
    serverAddress.sin_port = htons(config.port);

    if (bind(serverSocket, (sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) {
        Logger::error("Bind failed. Error Code: " + std::to_string(WSAGetLastError()));
        closesocket(serverSocket);
        WSACleanup();
        return false;
    }

    Logger::info("Socket Bound Successfully!");
    Logger::info("Server running at http://" + config.host + ":" + std::to_string(config.port) + " using " + std::to_string(config.threadCount) + " threads");
    Logger::info("Document root directory: " + g_documentRoot);

    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        Logger::error("Listen failed. Error Code: " + std::to_string(WSAGetLastError()));
        closesocket(serverSocket);
        WSACleanup();
        return false;
    }

    Logger::info("Server is Listening...");

    ThreadPool pool(config.threadCount);

    while (!stopping) {
        SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);

        if (clientSocket == INVALID_SOCKET) {
            if (stopping) {
                break; // Expected during shutdown
            }
            Logger::error("Accept failed. Error Code: " + std::to_string(WSAGetLastError()));
            continue;
        }

        pool.enqueue([this, clientSocket]() {
            handleClient(clientSocket);
        });
    }

    Logger::info("Shutting down worker threads...");
    pool.shutdown();

    Logger::info("Closing server socket...");
    if (serverSocket != INVALID_SOCKET) {
        closesocket(serverSocket);
    }
    WSACleanup();

    Logger::info("Server stopped gracefully.");
    return true;
}

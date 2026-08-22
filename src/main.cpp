#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstring>
#include <thread>
#include <sstream>
#include <atomic>
#include <filesystem>
#include <algorithm>
#include <chrono>

#include "http_request.h"
#include "router.h"
#include "file_handler.h"
#include "http_response.h"
#include "route_handler.h"
#include "logger.h"
#include "socket_utils.h"
#include "concurrency/thread_pool.h"
#include "metrics.h"

using namespace std;

#pragma comment(lib, "ws2_32.lib")

constexpr int BUFFER_SIZE = 4096;

WSADATA wsaData;

std::atomic<bool> g_serverStopping(false);
SOCKET g_serverSocket = INVALID_SOCKET;
std::string g_documentRoot = "public";

// Initialize Winsock
bool initializeWinsock(){
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0){
        cerr << "WSAStartup failed. Error Code: "
             << result << endl;
        return false;
    }

    cout << "Winsock Initialized Successfully!" << endl;
    return true;
}


constexpr size_t MAX_HEADER_SIZE = 16 * 1024;    // 16 KB
constexpr size_t MAX_BODY_SIZE = 1024 * 1024;    // 1 MB
constexpr size_t MAX_REQUEST_SIZE = MAX_HEADER_SIZE + MAX_BODY_SIZE;

bool receiveHttpRequest(SOCKET clientSocket, string& requestData, string& connectionBuffer)
{
    constexpr int BUFFER_SIZE = 4096;
    char buffer[BUFFER_SIZE];

    size_t headerEnd = connectionBuffer.find("\r\n\r\n");

    // Receive until we have complete HTTP headers
    while (headerEnd == string::npos)
    {
        int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0);

        if (bytesReceived <= 0)
        {
            if (bytesReceived == SOCKET_ERROR)
            {
                int err = WSAGetLastError();
                if (err == WSAETIMEDOUT || err == WSAEWOULDBLOCK)
                {
                    Logger::error("Receive timeout during header read");
                }
                else
                {
                    Logger::error("Receive error during header read: " + to_string(err));
                }
            }
            return false;
        }

        ServerMetrics::getInstance().addBytesReceived(bytesReceived);
        connectionBuffer.append(buffer, bytesReceived);
        headerEnd = connectionBuffer.find("\r\n\r\n");

        if (connectionBuffer.size() > MAX_HEADER_SIZE)
        {
            // Send 413 Payload Too Large
            string response = "HTTP/1.1 413 Payload Too Large\r\nContent-Type: text/plain\r\nContent-Length: 17\r\nConnection: close\r\n\r\nPayload Too Large";
            sendAll(clientSocket, response);
            return false;
        }
    }

    // Validate Content-Length in headers
    string headerSection = connectionBuffer.substr(0, headerEnd);
    string lowerHeaders = headerSection;
    std::transform(lowerHeaders.begin(), lowerHeaders.end(), lowerHeaders.begin(), ::tolower);

    size_t pos = 0;
    string firstVal;
    bool hasContentLength = false;
    bool duplicateContentLength = false;

    while ((pos = lowerHeaders.find("content-length:", pos)) != string::npos) {
        if (pos > 0 && lowerHeaders[pos - 1] != '\n' && lowerHeaders[pos - 1] != '\r') {
            pos += 15;
            continue;
        }

        size_t lineEnd = lowerHeaders.find("\r\n", pos);
        if (lineEnd == string::npos) {
            lineEnd = lowerHeaders.size();
        }

        string val = headerSection.substr(pos + 15, lineEnd - (pos + 15));
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
        string response = "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\nContent-Length: 32\r\nConnection: close\r\n\r\nConflicting Content-Length Headers";
        sendAll(clientSocket, response);
        return false;
    }

    int contentLength = 0;
    if (hasContentLength) {
        if (firstVal.empty()) {
            string response = "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\nContent-Length: 22\r\nConnection: close\r\n\r\nInvalid Content-Length";
            sendAll(clientSocket, response);
            return false;
        }
        for (char c : firstVal) {
            if (!isdigit(static_cast<unsigned char>(c))) {
                string response = "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\nContent-Length: 22\r\nConnection: close\r\n\r\nInvalid Content-Length";
                sendAll(clientSocket, response);
                return false;
            }
        }
        try {
            unsigned long long parsedLen = stoull(firstVal);
            if (parsedLen > MAX_BODY_SIZE) {
                string response = "HTTP/1.1 413 Payload Too Large\r\nContent-Type: text/plain\r\nContent-Length: 17\r\nConnection: close\r\n\r\nPayload Too Large";
                sendAll(clientSocket, response);
                return false;
            }
            contentLength = static_cast<int>(parsedLen);
        } catch (...) {
            string response = "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\nContent-Length: 22\r\nConnection: close\r\n\r\nInvalid Content-Length";
            sendAll(clientSocket, response);
            return false;
        }
    }

    size_t bodyStart = headerEnd + 4;

    // Receive remaining body
    while (connectionBuffer.size() < bodyStart + contentLength)
    {
        int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0);

        if (bytesReceived <= 0)
        {
            if (bytesReceived == SOCKET_ERROR)
            {
                int err = WSAGetLastError();
                if (err == WSAETIMEDOUT || err == WSAEWOULDBLOCK)
                {
                    Logger::error("Receive timeout during body read");
                }
                else
                {
                    Logger::error("Receive error during body read: " + to_string(err));
                }
            }
            return false;
        }

        ServerMetrics::getInstance().addBytesReceived(bytesReceived);
        connectionBuffer.append(buffer, bytesReceived);

        if (connectionBuffer.size() > MAX_HEADER_SIZE + contentLength) {
            string response = "HTTP/1.1 413 Payload Too Large\r\nContent-Type: text/plain\r\nContent-Length: 17\r\nConnection: close\r\n\r\nPayload Too Large";
            sendAll(clientSocket, response);
            return false;
        }
    }

    // Copy this request out and keep remainder in connectionBuffer
    requestData = connectionBuffer.substr(0, bodyStart + contentLength);
    connectionBuffer.erase(0, bodyStart + contentLength);

    return true;
}

struct ConnectionGuard {
    ConnectionGuard() { ServerMetrics::getInstance().addConnection(); }
    ~ConnectionGuard() { ServerMetrics::getInstance().removeConnection(); }
};

// Handle single client connection
void handleClient(SOCKET clientSocket){
    ConnectionGuard connGuard;
    int timeout = 5000;
    if (setsockopt(
            clientSocket,
            SOL_SOCKET,
            SO_RCVTIMEO,
            reinterpret_cast<const char*>(&timeout),
            sizeof(timeout)) == SOCKET_ERROR)
    {
        Logger::error("Failed to set SO_RCVTIMEO");
    }
    if (setsockopt(
            clientSocket,
            SOL_SOCKET,
            SO_SNDTIMEO,
            reinterpret_cast<const char*>(&timeout),
            sizeof(timeout)) == SOCKET_ERROR)
    {
        Logger::error("Failed to set SO_SNDTIMEO");
    }

    Logger::info("Client connected");

    std::string connectionBuffer;
    bool keepAlive = true;
    int requestCount = 0;
    const int maxKeepAliveRequests = 100;

    while (keepAlive && !g_serverStopping && requestCount < maxKeepAliveRequests)
    {
        string rawRequest;
        if (!receiveHttpRequest(clientSocket, rawRequest, connectionBuffer)){
            break;
        }

        auto startTime = std::chrono::high_resolution_clock::now();
        requestCount++;
        HttpRequest request(rawRequest);

        // Increment total requests
        ServerMetrics::getInstance().incrementTotalRequests();

        if (!request.isValid())
        {
            ServerMetrics::getInstance().incrementClientErrors();
            string errMessage = request.getErrorMessage();
            int errCode = request.getErrorCode();
            Logger::error("HTTP Request validation failed: " + errMessage);

            HttpResponse response(errCode, "text/plain", errMessage);
            response.setHeader("Connection", "close");
            
            string responseStr = response.toString();
            sendAll(clientSocket, responseStr);
            ServerMetrics::getInstance().addBytesSent(responseStr.size());
            break;
        }

        std::ostringstream ss;
        ss << "\n===== PARSED HTTP REQUEST (Count: " << requestCount << ") =====\n"
           << "Method  : " << request.getMethod() << "\n"
           << "Path    : " << request.getPath() << "\n"
           << "Query   : " << request.getQueryString() << "\n"
           << "Version : " << request.getVersion() << "\n"
           << "Content-Type : " << request.getHeader("Content-Type") << "\n"
           << "Content-Length : " << request.getHeader("Content-Length") << "\n"
           << "Body : " << request.getBody() << "\n"
           << "==============================";
        Logger::info(ss.str());

        string connectionHeader = request.getHeader("Connection");
        std::transform(connectionHeader.begin(), connectionHeader.end(), connectionHeader.begin(), ::tolower);

        if (request.getVersion() == "HTTP/1.1")
        {
            if (connectionHeader == "close")
            {
                keepAlive = false;
            }
            else
            {
                keepAlive = true;
            }
        }
        else
        {
            if (connectionHeader == "keep-alive")
            {
                keepAlive = true;
            }
            else
            {
                keepAlive = false;
            }
        }

        HttpResponse response = RouteHandler::handleRequest(request);
        if (request.getMethod() == "HEAD") {
            response.setSendBody(false);
        }

        if (keepAlive)
        {
            response.setHeader("Connection", "keep-alive");
            response.setHeader("Keep-Alive", "timeout=5, max=" + to_string(maxKeepAliveRequests - requestCount));
        }
        else
        {
            response.setHeader("Connection", "close");
        }

        string responseStr = response.toString();
        bool success = sendAll(clientSocket, responseStr);

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

        // Increment response status counters
        int status = response.getStatusCode();
        if (status >= 200 && status < 300) {
            ServerMetrics::getInstance().incrementSuccessfulRequests();
        } else if (status >= 400 && status < 500) {
            ServerMetrics::getInstance().incrementClientErrors();
        } else if (status >= 500 && status < 600) {
            ServerMetrics::getInstance().incrementServerErrors();
        }

        if (!success){
            Logger::error("Send failed. Error Code: " + to_string(WSAGetLastError()));
            break;
        }else{
            ServerMetrics::getInstance().addBytesSent(responseStr.size());

            // Format log: METHOD PATH STATUS BYTES DURATION
            string logMsg = request.getMethod() + " " + request.getPath() + " " + to_string(status) + " " + to_string(responseStr.size()) + "B " + to_string(duration) + "ms";
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

bool parseCommandLine(int argc, char* argv[], int& port, int& threadCount, string& root) {
    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            cout << "Usage: server.exe [options]\n"
                 << "Options:\n"
                 << "  --port <number>     Port to listen on (default: 8080)\n"
                 << "  --threads <number>  Number of worker threads (default: CPU cores)\n"
                 << "  --root <path>       Document root directory (default: public)\n"
                 << "  --help, -h          Show this help message\n";
            return false;
        } else if (arg == "--port") {
            if (i + 1 >= argc) {
                cerr << "Error: --port requires a value\n";
                return false;
            }
            string val = argv[++i];
            if (val.empty() || !all_of(val.begin(), val.end(), ::isdigit)) {
                cerr << "Error: Invalid port number: " << val << "\n";
                return false;
            }
            try {
                int p = stoi(val);
                if (p < 1 || p > 65535) {
                    cerr << "Error: Port number out of range: " << val << "\n";
                    return false;
                }
                port = p;
            } catch (...) {
                cerr << "Error: Invalid port number: " << val << "\n";
                return false;
            }
        } else if (arg == "--threads") {
            if (i + 1 >= argc) {
                cerr << "Error: --threads requires a value\n";
                return false;
            }
            string val = argv[++i];
            if (val.empty() || !all_of(val.begin(), val.end(), ::isdigit)) {
                cerr << "Error: Invalid thread count: " << val << "\n";
                return false;
            }
            try {
                int t = stoi(val);
                if (t <= 0 || t > 1024) {
                    cerr << "Error: Thread count must be between 1 and 1024\n";
                    return false;
                }
                threadCount = t;
            } catch (...) {
                cerr << "Error: Invalid thread count: " << val << "\n";
                return false;
            }
        } else if (arg == "--root") {
            if (i + 1 >= argc) {
                cerr << "Error: --root requires a value\n";
                return false;
            }
            root = argv[++i];
        } else {
            cerr << "Error: Unknown argument: " << arg << "\n";
            return false;
        }
    }
    return true;
}

BOOL WINAPI ctrlHandler(DWORD ctrlType) {
    switch (ctrlType) {
        case CTRL_C_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_BREAK_EVENT:
            Logger::info("Shutdown signal received. Stopping server...");
            g_serverStopping = true;
            if (g_serverSocket != INVALID_SOCKET) {
                closesocket(g_serverSocket);
                g_serverSocket = INVALID_SOCKET;
            }
            return TRUE;
        default:
            return FALSE;
    }
}

int main(int argc, char* argv[]){
    int port = 8080;
    int threadCount = std::thread::hardware_concurrency();
    if (threadCount <= 0) {
        threadCount = 4; // Sensible fallback
    }

    if (!parseCommandLine(argc, argv, port, threadCount, g_documentRoot)) {
        return 0; // Handled help or parse error
    }

    // Validate root directory exists
    if (!std::filesystem::exists(g_documentRoot) || !std::filesystem::is_directory(g_documentRoot)) {
        Logger::error("Document root directory does not exist or is not a directory: " + g_documentRoot);
        return 1;
    }

    // Set console signal handler
    if (!SetConsoleCtrlHandler(ctrlHandler, TRUE)) {
        Logger::error("Failed to set console control handler.");
        return 1;
    }

    // Step 1: Initialize Winsock
    if (!initializeWinsock()){
        return 1;
    }

    // Step 2: Create TCP Server Socket
    SOCKET serverSocket = socket(
        AF_INET,
        SOCK_STREAM,
        IPPROTO_TCP);

    if (serverSocket == INVALID_SOCKET){
        Logger::error("Socket creation failed. Error Code: " + to_string(WSAGetLastError()));
        WSACleanup();
        return 1;
    }

    g_serverSocket = serverSocket;
    Logger::info("Server Socket Created Successfully!");

    // Step 3: Bind Socket
    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddress.sin_port = htons(port);

    if (bind(serverSocket,
             (sockaddr *)&serverAddress,
             sizeof(serverAddress)) == SOCKET_ERROR)
    {
        Logger::error("Bind failed. Error Code: " + to_string(WSAGetLastError()));
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    Logger::info("Socket Bound Successfully!");
    Logger::info("Server running at http://localhost:" + to_string(port) + " using " + to_string(threadCount) + " threads");
    Logger::info("Document root directory: " + g_documentRoot);

    // Step 4: Listen
    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR){
        Logger::error("Listen failed. Error Code: " + to_string(WSAGetLastError()));
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    Logger::info("Server is Listening...");

    // Create ThreadPool
    ThreadPool pool(threadCount);

    // Step 5: Accept Clients
    while (!g_serverStopping){
        SOCKET clientSocket = accept(
            serverSocket,
            nullptr,
            nullptr);

        if (clientSocket == INVALID_SOCKET){
            if (g_serverStopping) {
                break; // Expected during shutdown
            }
            Logger::error("Accept failed. Error Code: " + to_string(WSAGetLastError()));
            continue;
        }

        // Queue connection handler to thread pool
        pool.enqueue([clientSocket]() {
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
    return 0;
}
#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstring>
#include <thread>
#include <sstream>

#include "http_request.h"
#include "router.h"
#include "file_handler.h"
#include "http_response.h"
#include "route_handler.h"
#include "logger.h"
#include "socket_utils.h"

using namespace std;

#pragma comment(lib, "ws2_32.lib")

constexpr int BUFFER_SIZE = 4096;

WSADATA wsaData;

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

bool receiveHttpRequest(SOCKET clientSocket, string& requestData)
{
    constexpr int BUFFER_SIZE = 4096;

    char buffer[BUFFER_SIZE];

    requestData.clear();

    //--------------------------------------------------------
    // Receive until we have the complete HTTP headers
    //--------------------------------------------------------

    size_t headerEnd = string::npos;

    while (headerEnd == string::npos)
    {
        int bytesReceived = recv(
            clientSocket,
            buffer,
            BUFFER_SIZE,
            0
        );

        if (bytesReceived <= 0)
        {
            if (bytesReceived == SOCKET_ERROR)
            {
                int err = WSAGetLastError();
                if (err == WSAETIMEDOUT || err == WSAEWOULDBLOCK)
                {
                    Logger::error("Receive timeout");
                }
                else
                {
                    Logger::error("Receive error: " + to_string(err));
                }
            }
            return false;
        }

        requestData.append(
            buffer,
            bytesReceived
        );

        headerEnd =
            requestData.find("\r\n\r\n");

        if (requestData.size() > MAX_HEADER_SIZE)
        {
            // Send 413 Payload Too Large
            string response = "HTTP/1.1 413 Payload Too Large\r\nContent-Type: text/plain\r\nContent-Length: 17\r\nConnection: close\r\n\r\nPayload Too Large";
            sendAll(clientSocket, response);
            return false;
        }
    }

    //--------------------------------------------------------
    // Determine Content-Length
    //--------------------------------------------------------

    size_t contentLengthPos =
        requestData.find("Content-Length:");

    // No body
    if (contentLengthPos == string::npos)
    {
        return true;
    }

    size_t lineEnd =
        requestData.find(
            "\r\n",
            contentLengthPos
        );

    if (lineEnd == string::npos)
    {
        return false;
    }

    string lengthValue =
        requestData.substr(
            contentLengthPos + 15,
            lineEnd - (contentLengthPos + 15)
        );

    // Remove leading/trailing spaces/tabs/carriage returns
    while (!lengthValue.empty() &&
           (lengthValue.front() == ' ' ||
            lengthValue.front() == '\t'))
    {
        lengthValue.erase(
            lengthValue.begin()
        );
    }
    while (!lengthValue.empty() &&
           (lengthValue.back() == ' ' ||
            lengthValue.back() == '\t' ||
            lengthValue.back() == '\r'))
    {
        lengthValue.pop_back();
    }

    int contentLength = 0;
    bool isContentLengthValid = true;

    if (lengthValue.empty()) {
        isContentLengthValid = false;
    } else {
        for (char c : lengthValue) {
            if (!isdigit(static_cast<unsigned char>(c))) {
                isContentLengthValid = false;
                break;
            }
        }
    }

    if (isContentLengthValid) {
        try {
            unsigned long long parsedLen = stoull(lengthValue);
            if (parsedLen > MAX_BODY_SIZE) {
                // Send 413 Payload Too Large
                string response = "HTTP/1.1 413 Payload Too Large\r\nContent-Type: text/plain\r\nContent-Length: 17\r\nConnection: close\r\n\r\nPayload Too Large";
                sendAll(clientSocket, response);
                return false;
            }
            contentLength = static_cast<int>(parsedLen);
        } catch (...) {
            isContentLengthValid = false;
        }
    }

    if (!isContentLengthValid) {
        // Send 400 Bad Request
        string response = "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\nContent-Length: 22\r\nConnection: close\r\n\r\nInvalid Content-Length";
        sendAll(clientSocket, response);
        return false;
    }

    //--------------------------------------------------------
    // Calculate current body size
    //--------------------------------------------------------

    size_t bodyStart =
        headerEnd + 4;

    size_t currentBodySize =
        requestData.size() - bodyStart;

    // Safety check before receiving
    if (bodyStart + contentLength > MAX_REQUEST_SIZE) {
        // Send 413 Payload Too Large
        string response = "HTTP/1.1 413 Payload Too Large\r\nContent-Type: text/plain\r\nContent-Length: 17\r\nConnection: close\r\n\r\nPayload Too Large";
        sendAll(clientSocket, response);
        return false;
    }

    //--------------------------------------------------------
    // Receive remaining body
    //--------------------------------------------------------

    while (
        currentBodySize <
        static_cast<size_t>(contentLength)
    )
    {
        int bytesReceived =
            recv(
                clientSocket,
                buffer,
                BUFFER_SIZE,
                0
            );

        if (bytesReceived <= 0)
        {
            if (bytesReceived == SOCKET_ERROR)
            {
                int err = WSAGetLastError();
                if (err == WSAETIMEDOUT || err == WSAEWOULDBLOCK)
                {
                    Logger::error("Receive timeout");
                }
                else
                {
                    Logger::error("Receive error: " + to_string(err));
                }
            }
            return false;
        }

        requestData.append(
            buffer,
            bytesReceived
        );

        currentBodySize =
            requestData.size() - bodyStart;

        if (requestData.size() > MAX_REQUEST_SIZE) {
            // Send 413 Payload Too Large
            string response = "HTTP/1.1 413 Payload Too Large\r\nContent-Type: text/plain\r\nContent-Length: 17\r\nConnection: close\r\n\r\nPayload Too Large";
            sendAll(clientSocket, response);
            return false;
        }
    }

    //--------------------------------------------------------
    // Debug: show exactly what was received
    //--------------------------------------------------------

    std::ostringstream ss;
    ss << "\n===== RAW HTTP BODY =====\n"
       << "[" << requestData.substr(bodyStart, contentLength) << "]\n"
       << "Expected Body Length: " << contentLength << "\n"
       << "Actual Body Length: " << requestData.substr(bodyStart, contentLength).size() << "\n"
       << "=========================";
    Logger::info(ss.str());

    return true;
}


// Handle single client connection
void handleClient(SOCKET clientSocket){
    // Set socket receive and send timeouts to 5000ms
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

    // Step 6: Receive HTTP Request
    string rawRequest;
    if (!receiveHttpRequest(clientSocket, rawRequest)){
        closesocket(clientSocket);
        Logger::info("Client disconnected");
        return;
    }
    HttpRequest request(rawRequest);

    // Dynamic request diagnostic logging in a synchronized log block
    std::ostringstream ss;
    ss << "\n===== PARSED HTTP REQUEST =====\n"
       << "Method  : " << request.getMethod() << "\n"
       << "Path    : " << request.getPath() << "\n"
       << "Query   : " << request.getQueryString() << "\n"
       << "Version : " << request.getVersion() << "\n"
       << "Content-Type : " << request.getHeader("Content-Type") << "\n"
       << "Content-Length : " << request.getHeader("Content-Length") << "\n"
       << "Body : " << request.getBody() << "\n"
       << "==============================";
    Logger::info(ss.str());

    // Route request and generate response
    HttpResponse response = RouteHandler::handleRequest(request);
    if (request.getMethod() == "HEAD") {
        response.setSendBody(false);
    }
    string responseStr = response.toString();

    // Step 7: Send Response using reliable sendAll
    bool success = sendAll(clientSocket, responseStr);

    if (!success){
        Logger::error("Send failed. Error Code: " + to_string(WSAGetLastError()));
    }else{
        Logger::info(request.getMethod() + " " + request.getPath() + " -> Response " + to_string(response.getStatusCode()) + " (" + to_string(responseStr.size()) + " bytes)");
    }

    // Step 8: Close Client Connection
    closesocket(clientSocket);
    Logger::info("Client disconnected");
}

int main(){
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

    Logger::info("Server Socket Created Successfully!");

    // Step 3: Bind Socket
    sockaddr_in serverAddress{};

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddress.sin_port = htons(8080);

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
    Logger::info("Server running at http://localhost:8080");

    // Step 4: Listen
    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR){
        Logger::error("Listen failed. Error Code: " + to_string(WSAGetLastError()));

        closesocket(serverSocket);
        WSACleanup();

        return 1;
    }

    Logger::info("Server is Listening...");

    // Step 5: Accept Clients Forever
    while (true){
        SOCKET clientSocket = accept(
            serverSocket,
            nullptr,
            nullptr);

        if (clientSocket == INVALID_SOCKET){
            Logger::error("Accept failed. Error Code: " + to_string(WSAGetLastError()));

            continue;
        }

        // Spawn a detached worker thread to handle client concurrently
        std::thread clientThread(handleClient, clientSocket);
        clientThread.detach();
    }

    // Graceful Shutdown
    // (Reached only if the server loop is exited.)
    closesocket(serverSocket);
    WSACleanup();

    Logger::info("Server Socket Closed.");
    Logger::info("Winsock Cleaned Up Successfully!");
    return 0;
}
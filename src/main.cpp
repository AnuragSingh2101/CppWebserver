#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstring>

#include "http_request.h"
#include "router.h"
#include "file_handler.h"
#include "http_response.h"
#include "route_handler.h"

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
            send(clientSocket, response.c_str(), static_cast<int>(response.size()), 0);
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
                send(clientSocket, response.c_str(), static_cast<int>(response.size()), 0);
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
        send(clientSocket, response.c_str(), static_cast<int>(response.size()), 0);
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
        send(clientSocket, response.c_str(), static_cast<int>(response.size()), 0);
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
            send(clientSocket, response.c_str(), static_cast<int>(response.size()), 0);
            return false;
        }
    }

    //--------------------------------------------------------
    // Debug: show exactly what was received
    //--------------------------------------------------------

    cout << "\n===== RAW HTTP BODY =====" << endl;

    cout << "["
         << requestData.substr(
                bodyStart,
                contentLength
            )
         << "]"
         << endl;

    cout << "Expected Body Length: "
         << contentLength
         << endl;

    cout << "Actual Body Length: "
         << requestData.substr(
                bodyStart,
                contentLength
            ).size()
         << endl;

    return true;
}


// Handle single client connection
void handleClient(SOCKET clientSocket){
    cout << "\n==========================================" << endl;
    cout << "Client Connected!" << endl;

    // Step 6: Receive HTTP Request
    string rawRequest;
    if (!receiveHttpRequest(clientSocket, rawRequest)){
        cerr << "Failed to receive HTTP request." << endl;
        closesocket(clientSocket);
        return;
    }
    HttpRequest request(rawRequest);

    cout << "\n===== PARSED HTTP REQUEST =====" << endl;
    cout << "Method  : "
        << request.getMethod()
        << endl;

    cout << "Path    : "
        << request.getPath()
        << endl;

    cout << "Query   : "
        << request.getQueryString()
        << endl;

    cout << "Version : "
        << request.getVersion()
        << endl;

    cout << "Content-Type : "
        << request.getHeader("Content-Type")
        << endl;

    cout << "Content-Length : "
        << request.getHeader("Content-Length")
        << endl;

    cout << "Body : "
        << request.getBody()
        << endl;

    // Route request and generate response
    HttpResponse response = RouteHandler::handleRequest(request);
    if (request.getMethod() == "HEAD") {
        response.setSendBody(false);
    }
    string responseStr = response.toString();

    // Step 7: Send Response
    int bytesSent = send(
        clientSocket,
        responseStr.c_str(),
        static_cast<int>(responseStr.size()),
        0);

    if (bytesSent == SOCKET_ERROR){
        cerr << "Send failed. Error Code: "
             << WSAGetLastError() << endl;
    }else{
        cout << "Response Sent Successfully!" << endl;
        cout << "Bytes Sent : "
             << bytesSent << endl;
    }

    // Step 8: Close Client Connection
    closesocket(clientSocket);
    cout << "Client Disconnected." << endl;
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
        cerr << "Socket creation failed. Error Code: "
             << WSAGetLastError() << endl;

        WSACleanup();
        return 1;
    }

    cout << "Server Socket Created Successfully!" << endl;

    // Step 3: Bind Socket
    sockaddr_in serverAddress{};

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddress.sin_port = htons(8080);

    if (bind(serverSocket,
             (sockaddr *)&serverAddress,
             sizeof(serverAddress)) == SOCKET_ERROR)
    {
        cerr << "Bind failed. Error Code: "
             << WSAGetLastError() << endl;

        closesocket(serverSocket);
        WSACleanup();

        return 1;
    }

    cout << "Socket Bound Successfully!" << endl;
    cout << "Server running at http://localhost:8080" << endl;

    // Step 4: Listen
    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR){
        cerr << "Listen failed. Error Code: "
             << WSAGetLastError() << endl;

        closesocket(serverSocket);
        WSACleanup();

        return 1;
    }

    cout << "Server is Listening..." << endl;

    // Step 5: Accept Clients Forever
    while (true){
        SOCKET clientSocket = accept(
            serverSocket,
            nullptr,
            nullptr);

        if (clientSocket == INVALID_SOCKET){
            cerr << "Accept failed. Error Code: "
                 << WSAGetLastError() << endl;

            continue;
        }

        // Delegate to the specialized handler
        handleClient(clientSocket);
    }

    // Graceful Shutdown
    // (Reached only if the server loop is exited.)
    closesocket(serverSocket);
    WSACleanup();

    cout << "Server Socket Closed." << endl;
    cout << "Winsock Cleaned Up Successfully!" << endl;
    return 0;
}
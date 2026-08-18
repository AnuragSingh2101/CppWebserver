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

    // Remove leading spaces
    while (!lengthValue.empty() &&
           (lengthValue.front() == ' ' ||
            lengthValue.front() == '\t'))
    {
        lengthValue.erase(
            lengthValue.begin()
        );
    }

    int contentLength = 0;

    try
    {
        contentLength =
            stoi(lengthValue);
    }
    catch (...)
    {
        cerr << "Invalid Content-Length: "
             << lengthValue
             << endl;

        return false;
    }

    //--------------------------------------------------------
    // Calculate current body size
    //--------------------------------------------------------

    size_t bodyStart =
        headerEnd + 4;

    size_t currentBodySize =
        requestData.size() - bodyStart;

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
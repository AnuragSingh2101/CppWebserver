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

// Handle single client connection
void handleClient(SOCKET clientSocket)
{
    cout << "\n==========================================" << endl;
    cout << "Client Connected!" << endl;

    // Step 6: Receive HTTP Request
    char buffer[BUFFER_SIZE] = {};

    int bytesReceived = recv(
        clientSocket,
        buffer,
        BUFFER_SIZE - 1,
        0);

    if (bytesReceived == SOCKET_ERROR){
        cerr << "Receive failed. Error Code: "
             << WSAGetLastError() << endl;

        closesocket(clientSocket);
        return;
    }

    // Parse Request
    HttpRequest request(buffer);

    cout << "\n===== PARSED HTTP REQUEST =====" << endl;
    cout << "Method  : " << request.getMethod() << endl;
    cout << "Path    : " << request.getPath() << endl;
    cout << "Version : " << request.getVersion() << endl;

    // Route request and generate response
    HttpResponse response = RouteHandler::handleRequest(request);
    string responseStr = response.toString();

    cout << "\nBytes Received : "
         << bytesReceived << endl;

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
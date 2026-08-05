#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstring>

using namespace std;

#pragma comment(lib, "ws2_32.lib")

WSADATA wsaData;

// Initialize Winsock
bool initializeWinsock()
{
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);

    if (result != 0)
    {
        cerr << "WSAStartup failed. Error Code: " << result << endl;
        return false;
    }

    cout << "Winsock Initialized Successfully!" << endl;
    return true;
}

int main()
{
    // Step 1: Initialize Winsock
    if (!initializeWinsock())
    {
        return 1;
    }

    // Step 2: Create TCP Server Socket
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (serverSocket == INVALID_SOCKET)
    {
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
             (sockaddr*)&serverAddress,
             sizeof(serverAddress)) == SOCKET_ERROR)
    {
        cerr << "Bind failed. Error Code: "
             << WSAGetLastError() << endl;

        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    cout << "Socket Bound Successfully!" << endl;
    cout << "Listening Address : 0.0.0.0" << endl;
    cout << "Port : 8080" << endl;

    // Step 4: Listen
    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        cerr << "Listen failed. Error Code: "
             << WSAGetLastError() << endl;

        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    cout << "Server is Listening..." << endl;
    cout << "Waiting for Clients..." << endl;

    // Step 5: Accept Client
    SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);

    if (clientSocket == INVALID_SOCKET)
    {
        cerr << "Accept failed. Error Code: "
             << WSAGetLastError() << endl;

        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    cout << "Client Connected!" << endl;

    // Step 6: Receive HTTP Request
    char buffer[4096] = {0};

    int bytesReceived = recv(
        clientSocket,
        buffer,
        sizeof(buffer) - 1,
        0);

    if (bytesReceived == SOCKET_ERROR)
    {
        cerr << "Receive failed. Error Code: "
             << WSAGetLastError() << endl;
    }
    else
    {
        cout << "\n===== HTTP REQUEST =====\n";
        cout << buffer << endl;

        cout << "Bytes Received : "
             << bytesReceived
             << endl;
    }

    // Step 7: Send HTTP Response
    const char* response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 13\r\n"
        "Connection: close\r\n"
        "\r\n"
        "Hello, World!";

    int bytesSent = send(
        clientSocket,
        response,
        strlen(response),
        0);

    if (bytesSent == SOCKET_ERROR)
    {
        cerr << "Send failed. Error Code: "
             << WSAGetLastError() << endl;
    }
    else
    {
        cout << "Response sent successfully!" << endl;
        cout << "Bytes Sent : "
             << bytesSent
             << endl;
    }

    // Step 8: Cleanup
    closesocket(clientSocket);
    closesocket(serverSocket);

    WSACleanup();

    cout << "Client Socket Closed." << endl;
    cout << "Server Socket Closed." << endl;
    cout << "Winsock Cleaned Up Successfully!" << endl;

    return 0;
}
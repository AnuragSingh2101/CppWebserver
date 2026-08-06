#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstring>
#include <fstream>
#include <sstream>

#include "http_request.h"
#include "router.h"

using namespace std;

#pragma comment(lib, "ws2_32.lib")

constexpr int BUFFER_SIZE = 4096;

WSADATA wsaData;

//------------------------------------------------------------
// Initialize Winsock
//------------------------------------------------------------
bool initializeWinsock()
{
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);

    if (result != 0)
    {
        cerr << "WSAStartup failed. Error Code: "
             << result << endl;
        return false;
    }

    cout << "Winsock Initialized Successfully!" << endl;
    return true;
}

//------------------------------------------------------------
// Read file from disk
// Returns true if file exists and was opened successfully.
//------------------------------------------------------------
bool readFile(const string &filePath, string &content)
{
    ifstream file(filePath, ios::binary);

    if (!file.is_open())
    {
        return false;
    }

    stringstream buffer;
    buffer << file.rdbuf();

    content = buffer.str();
    return true;
}

int main()
{
    //------------------------------------------------------------
    // Step 1: Initialize Winsock
    //------------------------------------------------------------
    if (!initializeWinsock())
    {
        return 1;
    }

    //------------------------------------------------------------
    // Step 2: Create TCP Server Socket
    //------------------------------------------------------------
    SOCKET serverSocket = socket(
        AF_INET,
        SOCK_STREAM,
        IPPROTO_TCP);

    if (serverSocket == INVALID_SOCKET)
    {
        cerr << "Socket creation failed. Error Code: "
             << WSAGetLastError() << endl;

        WSACleanup();
        return 1;
    }

    cout << "Server Socket Created Successfully!" << endl;

    //------------------------------------------------------------
    // Step 3: Bind Socket
    //------------------------------------------------------------
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

    //------------------------------------------------------------
    // Step 4: Listen
    //------------------------------------------------------------
    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        cerr << "Listen failed. Error Code: "
             << WSAGetLastError() << endl;

        closesocket(serverSocket);
        WSACleanup();

        return 1;
    }

    cout << "Server is Listening..." << endl;

    //------------------------------------------------------------
    // Step 5: Accept Clients Forever
    //------------------------------------------------------------
    while (true)
    {
        SOCKET clientSocket = accept(
            serverSocket,
            nullptr,
            nullptr);

        if (clientSocket == INVALID_SOCKET)
        {
            cerr << "Accept failed. Error Code: "
                 << WSAGetLastError() << endl;

            continue;
        }

        cout << "\n==========================================" << endl;
        cout << "Client Connected!" << endl;

        //--------------------------------------------------------
        // Step 6: Receive HTTP Request
        //--------------------------------------------------------
        char buffer[BUFFER_SIZE] = {};

        int bytesReceived = recv(
            clientSocket,
            buffer,
            BUFFER_SIZE - 1,
            0);

        if (bytesReceived == SOCKET_ERROR)
        {
            cerr << "Receive failed. Error Code: "
                 << WSAGetLastError() << endl;

            closesocket(clientSocket);
            continue;
        }

        //--------------------------------------------------------
        // Parse Request
        //--------------------------------------------------------
        HttpRequest request(buffer);

        cout << "\n===== PARSED HTTP REQUEST =====" << endl;
        cout << "Method  : " << request.getMethod() << endl;
        cout << "Path    : " << request.getPath() << endl;
        cout << "Version : " << request.getVersion() << endl;

        //--------------------------------------------------------
        // Directory Traversal Protection
        //--------------------------------------------------------
        if (request.getPath().find("..") != string::npos)
        {
            cout << "403 Forbidden (Directory Traversal Attempt)"
                 << endl;

            string response =
                "HTTP/1.1 403 Forbidden\r\n"
                "Content-Type: text/plain\r\n"
                "Connection: close\r\n"
                "\r\n"
                "403 Forbidden";

            send(
                clientSocket,
                response.c_str(),
                response.size(),
                0);

            closesocket(clientSocket);
            continue;
        }

        //--------------------------------------------------------
        // Router
        //--------------------------------------------------------
        string filePath = Router::getFilePath(
            request.getPath());

        cout << "Requested File : "
             << filePath << endl;

        string response;

        //--------------------------------------------------------
        // Route not found
        //--------------------------------------------------------
        if (filePath.empty())
        {
            cout << "404 Not Found (Route)" << endl;

            response =
                "HTTP/1.1 404 Not Found\r\n"
                "Content-Type: text/plain\r\n"
                "Connection: close\r\n"
                "\r\n"
                "404 Not Found";
        }
        else
        {
            //----------------------------------------------------
            // Read File
            //----------------------------------------------------
            string fileContent;

            if (!readFile(filePath, fileContent))
            {
                cout << "404 Not Found (File Missing)"
                     << endl;

                response =
                    "HTTP/1.1 404 Not Found\r\n"
                    "Content-Type: text/plain\r\n"
                    "Connection: close\r\n"
                    "\r\n"
                    "404 Not Found";
            }
            else
            {
                string mimeType =
                    Router::getMimeType(filePath);

                response =
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: " + mimeType + "\r\n"
                    "Content-Length: " +
                    to_string(fileContent.size()) + "\r\n"
                    "Connection: close\r\n"
                    "\r\n" +
                    fileContent;

                cout << "\n===== FILE CONTENT ====="
                     << endl;

                cout << fileContent << endl;
            }
        }

        cout << "\nBytes Received : "
             << bytesReceived << endl;

        //--------------------------------------------------------
        // Step 7: Send Response
        //--------------------------------------------------------
        int bytesSent = send(
            clientSocket,
            response.c_str(),
            static_cast<int>(response.size()),
            0);

        if (bytesSent == SOCKET_ERROR)
        {
            cerr << "Send failed. Error Code: "
                 << WSAGetLastError() << endl;
        }
        else
        {
            cout << "Response Sent Successfully!" << endl;
            cout << "Bytes Sent : "
                 << bytesSent << endl;
        }

        //--------------------------------------------------------
        // Step 8: Close Client Connection
        //--------------------------------------------------------
        closesocket(clientSocket);

        cout << "Client Disconnected." << endl;
    }

    //------------------------------------------------------------
    // Graceful Shutdown
    // (Reached only if the server loop is exited.)
    //------------------------------------------------------------
    closesocket(serverSocket);

    WSACleanup();

    cout << "Server Socket Closed." << endl;
    cout << "Winsock Cleaned Up Successfully!" << endl;

    return 0;
}
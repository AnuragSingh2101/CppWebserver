#include <gtest/gtest.h>
#include "../src/server.h"
#include "../src/server_config.h"
#include "../src/user_store.h"
#include <winsock2.h>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <vector>
#include <atomic>

class IntegrationTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        // Clear UserStore first to make tests deterministic
        UserStore::clear();
        UserStore::addUser("Anurag", "anurag@example.com");
        UserStore::addUser("Rahul", "rahul@example.com");

        // Run server in background thread with 1 second timeout for fast testing
        ServerConfig config;
        config.port = 8081;
        config.host = "127.0.0.1";
        config.threadCount = 16;
        config.requestTimeoutMs = 1000; // 1 second timeout
        config.publicDir = "test_public_dir";

        std::filesystem::create_directory("test_public_dir");
        {
            std::ofstream indexFile("test_public_dir/index.html");
            indexFile << "index content";
        }
        {
            std::ofstream cssFile("test_public_dir/style.css");
            cssFile << "body { color: red; }";
        }
        {
            std::ofstream jsFile("test_public_dir/script.js");
            jsFile << "console.log('test');";
        }

        serverInstance = new HttpServer(config);
        serverThread = new std::thread([]() {
            serverInstance->run();
        });

        // Wait for server to bind
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    static void TearDownTestSuite() {
        if (serverInstance) {
            serverInstance->stop();
            if (serverThread && serverThread->joinable()) {
                serverThread->join();
            }
            delete serverThread;
            delete serverInstance;
            serverThread = nullptr;
            serverInstance = nullptr;
        }
        std::filesystem::remove_all("test_public_dir");
    }

    static HttpServer* serverInstance;
    static std::thread* serverThread;
};

HttpServer* IntegrationTest::serverInstance = nullptr;
std::thread* IntegrationTest::serverThread = nullptr;

std::string sendRawRequest(const std::string& requestStr) {
    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) return "ERROR_SOCKET";

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8081);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(clientSocket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        closesocket(clientSocket);
        return "ERROR_CONNECT";
    }

    int bytesSent = send(clientSocket, requestStr.c_str(), static_cast<int>(requestStr.size()), 0);
    if (bytesSent == SOCKET_ERROR) {
        closesocket(clientSocket);
        return "ERROR_SEND";
    }

    std::string response;
    char buffer[4096];
    int bytesReceived;
    while ((bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0)) > 0) {
        response.append(buffer, bytesReceived);
    }

    closesocket(clientSocket);
    return response;
}

// ----------------------------------------------------
// 1. HTTP PARSER TESTS
// ----------------------------------------------------

TEST_F(IntegrationTest, ParserValidGet) {
    std::string res = sendRawRequest("GET /api/info HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");
    EXPECT_NE(res.find("HTTP/1.1 200 OK"), std::string::npos);
}

TEST_F(IntegrationTest, ParserValidPost) {
    std::string body = "{\"name\":\"Alice\",\"email\":\"alice@example.com\"}";
    std::string req = 
        "POST /api/users HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n"
        "Connection: close\r\n\r\n" + body;
    std::string res = sendRawRequest(req);
    EXPECT_NE(res.find("HTTP/1.1 201 Created"), std::string::npos);
}

TEST_F(IntegrationTest, ParserMalformedRequestLine) {
    std::string res = sendRawRequest("GET\r\nHost: localhost\r\n\r\n");
    EXPECT_NE(res.find("HTTP/1.1 400 Bad Request"), std::string::npos);
}

TEST_F(IntegrationTest, ParserMissingVersion) {
    std::string res = sendRawRequest("GET /api/info\r\nHost: localhost\r\n\r\n");
    EXPECT_NE(res.find("HTTP/1.1 400 Bad Request"), std::string::npos);
}

TEST_F(IntegrationTest, ParserMalformedHeaders) {
    std::string res = sendRawRequest("GET /api/info HTTP/1.1\r\nHost localhost\r\n\r\n");
    EXPECT_NE(res.find("HTTP/1.1 400 Bad Request"), std::string::npos);
}

TEST_F(IntegrationTest, ParserDuplicateHostHeaders) {
    std::string res = sendRawRequest("GET /api/info HTTP/1.1\r\nHost: localhost\r\nHost: otherhost\r\n\r\n");
    EXPECT_NE(res.find("HTTP/1.1 400 Bad Request"), std::string::npos);
}

TEST_F(IntegrationTest, ParserInvalidContentLength) {
    std::string res = sendRawRequest("POST /api/users HTTP/1.1\r\nHost: localhost\r\nContent-Length: abc\r\n\r\n");
    EXPECT_NE(res.find("HTTP/1.1 400 Bad Request"), std::string::npos);
}

TEST_F(IntegrationTest, ParserBodyTooLarge) {
    // Declaring a Content-Length > 1MB. We don't send the body payload,
    // which prevents Windows TCP socket resets while validating server limits.
    std::string req = 
        "POST /api/users HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 2097152\r\n\r\n";
    std::string res = sendRawRequest(req);
    EXPECT_NE(res.find("HTTP/1.1 413 Payload Too Large"), std::string::npos);
}

// ----------------------------------------------------
// 2. ROUTER & STATIC FILES TESTS
// ----------------------------------------------------

TEST_F(IntegrationTest, RouteDefaultIndex) {
    std::string res = sendRawRequest("GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");
    EXPECT_NE(res.find("HTTP/1.1 200 OK"), std::string::npos);
    EXPECT_NE(res.find("index content"), std::string::npos);
    EXPECT_NE(res.find("Content-Type: text/html"), std::string::npos);
}

TEST_F(IntegrationTest, RouteIndexHtml) {
    std::string res = sendRawRequest("GET /index.html HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");
    EXPECT_NE(res.find("HTTP/1.1 200 OK"), std::string::npos);
    EXPECT_NE(res.find("index content"), std::string::npos);
}

TEST_F(IntegrationTest, RouteStyleCss) {
    std::string res = sendRawRequest("GET /style.css HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");
    EXPECT_NE(res.find("HTTP/1.1 200 OK"), std::string::npos);
    EXPECT_NE(res.find("Content-Type: text/css"), std::string::npos);
}

TEST_F(IntegrationTest, RouteScriptJs) {
    std::string res = sendRawRequest("GET /script.js HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");
    EXPECT_NE(res.find("HTTP/1.1 200 OK"), std::string::npos);
    EXPECT_NE(res.find("Content-Type: application/javascript"), std::string::npos);
}

TEST_F(IntegrationTest, RouteUnknown) {
    std::string res = sendRawRequest("GET /doesnotexist HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");
    EXPECT_NE(res.find("HTTP/1.1 404 Not Found"), std::string::npos);
}

// ----------------------------------------------------
// 3. API METHOD & PARAMETER VALIDATION TESTS
// ----------------------------------------------------

TEST_F(IntegrationTest, ApiGetUsersAll) {
    std::string res = sendRawRequest("GET /api/users HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");
    EXPECT_NE(res.find("HTTP/1.1 200 OK"), std::string::npos);
    EXPECT_NE(res.find("Anurag"), std::string::npos);
}

TEST_F(IntegrationTest, ApiGetUsersById) {
    std::string res = sendRawRequest("GET /api/users/1 HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");
    EXPECT_NE(res.find("HTTP/1.1 200 OK"), std::string::npos);
    EXPECT_NE(res.find("anurag@example.com"), std::string::npos);
}

TEST_F(IntegrationTest, ApiPostUsersMalformedJson) {
    std::string body = "{\"malformed\"";
    std::string req = 
        "POST /api/users HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n"
        "Connection: close\r\n\r\n" + body;
    std::string res = sendRawRequest(req);
    EXPECT_NE(res.find("HTTP/1.1 400 Bad Request"), std::string::npos);
}

TEST_F(IntegrationTest, ApiPostUsersEmptyBody) {
    std::string res = sendRawRequest("POST /api/users HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
    EXPECT_NE(res.find("HTTP/1.1 400 Bad Request"), std::string::npos);
}

TEST_F(IntegrationTest, ApiPostUsersUnsupportedContentType) {
    std::string res = sendRawRequest("POST /api/users HTTP/1.1\r\nHost: localhost\r\nContent-Type: text/plain\r\nContent-Length: 5\r\nConnection: close\r\n\r\nhello");
    EXPECT_NE(res.find("HTTP/1.1 415 Unsupported Media Type"), std::string::npos);
}

TEST_F(IntegrationTest, ApiMethodNotAllowed) {
    std::string res = sendRawRequest("POST /api/info HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");
    EXPECT_NE(res.find("HTTP/1.1 405 Method Not Allowed"), std::string::npos);
    EXPECT_NE(res.find("Allow: GET, OPTIONS"), std::string::npos);
}

// ----------------------------------------------------
// 4. SECURITY & DIRECTORY TRAVERSAL TESTS
// ----------------------------------------------------

TEST_F(IntegrationTest, SecurityDirectoryTraversalBlockedDotDot) {
    std::string res = sendRawRequest("GET /../CMakeLists.txt HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");
    EXPECT_NE(res.find("HTTP/1.1 403 Forbidden"), std::string::npos);
}

TEST_F(IntegrationTest, SecurityDirectoryTraversalBlockedPercentEncoded) {
    std::string res = sendRawRequest("GET /%2e%2e/CMakeLists.txt HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");
    EXPECT_NE(res.find("HTTP/1.1 403 Forbidden"), std::string::npos);
}

TEST_F(IntegrationTest, SecurityDirectoryTraversalBlockedObfuscated) {
    std::string res = sendRawRequest("GET /test_public_dir/../CMakeLists.txt HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");
    EXPECT_NE(res.find("HTTP/1.1 403 Forbidden"), std::string::npos);
}

TEST_F(IntegrationTest, SecurityHiddenFilesBlocked) {
    {
        std::ofstream secretFile("test_public_dir/.secret");
        secretFile << "secret data";
    }
    std::string res = sendRawRequest("GET /.secret HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");
    EXPECT_NE(res.find("HTTP/1.1 404 Not Found"), std::string::npos); // resolves to empty because it's a hidden file
}

// ----------------------------------------------------
// 5. SOCKET ROBUSTNESS TESTS
// ----------------------------------------------------

TEST_F(IntegrationTest, SocketClientDisconnectGraceful) {
    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8081);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    connect(clientSocket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    send(clientSocket, "GET /api/inf", 12, 0);
    closesocket(clientSocket); // abrupt close

    // Verify server is still alive
    std::string res = sendRawRequest("GET /api/info HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");
    EXPECT_NE(res.find("HTTP/1.1 200 OK"), std::string::npos);
}

TEST_F(IntegrationTest, SocketReceiveTimeoutGraceful) {
    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8081);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    connect(clientSocket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    
    // Sleep longer than the 1s server timeout
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    // Try reading - the server should have closed the connection
    char buf[10];
    int recvRes = recv(clientSocket, buf, sizeof(buf), 0);
    EXPECT_LE(recvRes, 0); // Server closed socket

    closesocket(clientSocket);
}

// ----------------------------------------------------
// 6. CONCURRENCY LIMITS TESTS
// ----------------------------------------------------

void runParallelRequests(int clientCount) {
    std::vector<std::thread> threads;
    threads.reserve(clientCount);
    std::atomic<int> successCount(0);
    std::atomic<int> failCount(0);

    for (int i = 0; i < clientCount; ++i) {
        threads.emplace_back([&]() {
            std::string res = sendRawRequest("GET /api/info HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");
            if (res.find("HTTP/1.1 200 OK") != std::string::npos) {
                successCount++;
            } else {
                failCount++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(successCount, clientCount);
    EXPECT_EQ(failCount, 0);
}

TEST_F(IntegrationTest, Concurrency10Clients) {
    runParallelRequests(10);
}

TEST_F(IntegrationTest, Concurrency25Clients) {
    runParallelRequests(25);
}

TEST_F(IntegrationTest, Concurrency50Clients) {
    runParallelRequests(50);
}

TEST_F(IntegrationTest, Concurrency100Clients) {
    runParallelRequests(100);
}

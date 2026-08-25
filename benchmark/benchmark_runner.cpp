#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <algorithm>
#include <numeric>
#include <fstream>
#include <sstream>

#pragma comment(lib, "ws2_32.lib")

struct RequestResult {
    double latencyMs;
    bool success;
};

// Thread function
void runClient(
    const std::string& host,
    int port,
    const std::string& path,
    int requestCount,
    std::vector<RequestResult>& results)
{
    results.reserve(requestCount);

    SOCKET clientSocket = INVALID_SOCKET;
    std::string requestStr = 
        "GET " + path + " HTTP/1.1\r\n"
        "Host: " + host + ":" + std::to_string(port) + "\r\n"
        "Connection: keep-alive\r\n\r\n";

    auto connectSocket = [&]() -> bool {
        if (clientSocket != INVALID_SOCKET) {
            closesocket(clientSocket);
            clientSocket = INVALID_SOCKET;
        }

        clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (clientSocket == INVALID_SOCKET) return false;

        // Set short timeout
        int timeout = 3000;
        setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
        setsockopt(clientSocket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(host.c_str());

        if (addr.sin_addr.s_addr == INADDR_NONE) {
            // Resolve host if it's not a numeric IP
            addrinfo hints{}, *res = nullptr;
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;
            if (getaddrinfo(host.c_str(), nullptr, &hints, &res) == 0) {
                addr.sin_addr = reinterpret_cast<sockaddr_in*>(res->ai_addr)->sin_addr;
                freeaddrinfo(res);
            } else {
                return false;
            }
        }

        if (connect(clientSocket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
            closesocket(clientSocket);
            clientSocket = INVALID_SOCKET;
            return false;
        }
        return true;
    };

    if (!connectSocket()) {
        for (int i = 0; i < requestCount; ++i) {
            results.push_back({0.0, false});
        }
        return;
    }

    std::string responseBuf;
    char recvBuf[4096];

    for (int i = 0; i < requestCount; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        
        // Ensure connected
        if (clientSocket == INVALID_SOCKET && !connectSocket()) {
            results.push_back({0.0, false});
            continue;
        }

        // Send request
        int bytesSent = send(clientSocket, requestStr.c_str(), static_cast<int>(requestStr.size()), 0);
        if (bytesSent == SOCKET_ERROR) {
            results.push_back({0.0, false});
            connectSocket(); // reconnect
            continue;
        }

        // Read response
        responseBuf.clear();
        size_t headerEnd = std::string::npos;
        int contentLength = 0;
        bool readFailed = false;

        // 1. Read until headers complete
        while (headerEnd == std::string::npos) {
            int bytesRecv = recv(clientSocket, recvBuf, sizeof(recvBuf), 0);
            if (bytesRecv <= 0) {
                readFailed = true;
                break;
            }
            responseBuf.append(recvBuf, bytesRecv);
            headerEnd = responseBuf.find("\r\n\r\n");
        }

        if (readFailed) {
            results.push_back({0.0, false});
            connectSocket(); // reconnect
            continue;
        }

        // 2. Parse status code and Content-Length
        size_t statusLineEnd = responseBuf.find("\r\n");
        bool success = false;
        if (statusLineEnd != std::string::npos) {
            std::string statusLine = responseBuf.substr(0, statusLineEnd);
            std::stringstream ss(statusLine);
            std::string version;
            int status = 0;
            ss >> version >> status;
            if (status >= 200 && status < 400) {
                success = true;
            }
        }

        size_t clPos = responseBuf.find("Content-Length:");
        if (clPos == std::string::npos) {
            clPos = responseBuf.find("content-length:");
        }
        if (clPos != std::string::npos && clPos < headerEnd) {
            size_t clEnd = responseBuf.find("\r\n", clPos);
            if (clEnd != std::string::npos) {
                std::string clVal = responseBuf.substr(clPos + 15, clEnd - (clPos + 15));
                contentLength = std::stoi(clVal);
            }
        }

        // 3. Read body if incomplete
        size_t bodyBytesRead = responseBuf.size() - (headerEnd + 4);
        while (bodyBytesRead < static_cast<size_t>(contentLength)) {
            int toRead = static_cast<int>(static_cast<size_t>(contentLength) - bodyBytesRead);
            if (toRead > static_cast<int>(sizeof(recvBuf))) {
                toRead = sizeof(recvBuf);
            }
            int bytesRecv = recv(clientSocket, recvBuf, toRead, 0);
            if (bytesRecv <= 0) {
                readFailed = true;
                break;
            }
            bodyBytesRead += bytesRecv;
        }

        auto end = std::chrono::high_resolution_clock::now();
        double elapsedMs = std::chrono::duration<double, std::milli>(end - start).count();

        if (readFailed) {
            results.push_back({elapsedMs, false});
            connectSocket(); // reconnect
        } else {
            results.push_back({elapsedMs, success});
        }
    }

    if (clientSocket != INVALID_SOCKET) {
        closesocket(clientSocket);
    }
}

int main(int argc, char* argv[]) {
    // Arguments parsing
    int connections = 10;
    int totalRequests = 1000;
    std::string host = "127.0.0.1";
    int port = 8080;
    std::string path = "/api/info";
    std::string outputCsv = "";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--connections" && i + 1 < argc) {
            connections = std::stoi(argv[++i]);
        } else if (arg == "--requests" && i + 1 < argc) {
            totalRequests = std::stoi(argv[++i]);
        } else if (arg == "--host" && i + 1 < argc) {
            host = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "--path" && i + 1 < argc) {
            path = argv[++i];
        } else if (arg == "--output" && i + 1 < argc) {
            outputCsv = argv[++i];
        }
    }

    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed.\n";
        return 1;
    }

    std::cout << "Starting benchmark:\n"
              << "  Target: http://" << host << ":" << port << path << "\n"
              << "  Concurrency: " << connections << " clients\n"
              << "  Total requests: " << totalRequests << "\n\n";

    // Split requests among threads
    int requestsPerClient = totalRequests / connections;
    int adjustedTotal = requestsPerClient * connections;

    std::vector<std::vector<RequestResult>> threadResults(connections);
    std::vector<std::thread> threads;
    threads.reserve(connections);

    auto startBenchmark = std::chrono::high_resolution_clock::now();

    for (int c = 0; c < connections; ++c) {
        threads.emplace_back(runClient, host, port, path, requestsPerClient, std::ref(threadResults[c]));
    }

    for (auto& t : threads) {
        t.join();
    }

    auto endBenchmark = std::chrono::high_resolution_clock::now();
    double totalTimeSec = std::chrono::duration<double>(endBenchmark - startBenchmark).count();

    // Aggregate results
    std::vector<double> latencies;
    latencies.reserve(adjustedTotal);
    int successfulCount = 0;
    int failedCount = 0;

    for (const auto& resList : threadResults) {
        for (const auto& r : resList) {
            if (r.success) {
                successfulCount++;
                latencies.push_back(r.latencyMs);
            } else {
                failedCount++;
            }
        }
    }

    std::sort(latencies.begin(), latencies.end());

    double throughput = adjustedTotal / totalTimeSec;
    double minLatency = latencies.empty() ? 0.0 : latencies.front();
    double maxLatency = latencies.empty() ? 0.0 : latencies.back();
    double avgLatency = latencies.empty() ? 0.0 : std::accumulate(latencies.begin(), latencies.end(), 0.0) / latencies.size();

    double p50 = 0.0, p95 = 0.0, p99 = 0.0;
    if (!latencies.empty()) {
        p50 = latencies[static_cast<size_t>(latencies.size() * 0.50)];
        p95 = latencies[static_cast<size_t>(latencies.size() * 0.95)];
        p99 = latencies[static_cast<size_t>(latencies.size() * 0.99)];
    }

    // Display
    std::cout << "Results:\n"
              << "  Total requests:       " << adjustedTotal << "\n"
              << "  Successful requests:  " << successfulCount << "\n"
              << "  Failed requests:      " << failedCount << "\n"
              << "  Total time elapsed:   " << totalTimeSec << " seconds\n"
              << "  Throughput (req/sec): " << throughput << "\n"
              << "  Latency metrics:\n"
              << "    Average:            " << avgLatency << " ms\n"
              << "    Minimum:            " << minLatency << " ms\n"
              << "    Maximum:            " << maxLatency << " ms\n"
              << "    p50 (Median):       " << p50 << " ms\n"
              << "    p95:                " << p95 << " ms\n"
              << "    p99:                " << p99 << " ms\n\n";

    // Write to CSV
    if (!outputCsv.empty()) {
        std::ofstream csv(outputCsv, std::ios::app);
        if (csv.is_open()) {
            // Check if file is empty to write header
            csv.seekp(0, std::ios::end);
            if (csv.tellp() == 0) {
                csv << "Concurrency,TotalRequests,SuccessfulRequests,FailedRequests,Throughput(req/sec),AvgLatency(ms),MinLatency(ms),MaxLatency(ms),p50(ms),p95(ms),p99(ms)\n";
            }
            csv << connections << ","
                << adjustedTotal << ","
                << successfulCount << ","
                << failedCount << ","
                << throughput << ","
                << avgLatency << ","
                << minLatency << ","
                << maxLatency << ","
                << p50 << ","
                << p95 << ","
                << p99 << "\n";
        }
    }

    WSACleanup();
    return 0;
}

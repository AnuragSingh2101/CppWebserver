#ifndef METRICS_H
#define METRICS_H

#include <atomic>
#include <mutex>
#include <algorithm>

class ServerMetrics {
public:
    static ServerMetrics& getInstance() {
        static ServerMetrics instance;
        return instance;
    }

    void incrementTotalRequests() { totalRequests++; }
    void incrementSuccessfulRequests() { successfulRequests++; }
    void incrementClientErrors() { clientErrors++; }
    void incrementServerErrors() { serverErrors++; }

    void addConnection() {
        activeConnections++;
        // Update peak connections under a lock or loop
        int current = activeConnections.load();
        int peak = peakConnections.load();
        while (current > peak && !peakConnections.compare_exchange_weak(peak, current)) {
            peak = peakConnections.load();
        }
    }

    void removeConnection() {
        if (activeConnections > 0) {
            activeConnections--;
        }
    }

    void addBytesReceived(size_t bytes) { bytesReceived += bytes; }
    void addBytesSent(size_t bytes) { bytesSent += bytes; }

    uint64_t getTotalRequests() const { return totalRequests.load(); }
    uint64_t getSuccessfulRequests() const { return successfulRequests.load(); }
    uint64_t getClientErrors() const { return clientErrors.load(); }
    uint64_t getServerErrors() const { return serverErrors.load(); }
    int getActiveConnections() const { return activeConnections.load(); }
    int getPeakConnections() const { return peakConnections.load(); }
    uint64_t getBytesReceived() const { return bytesReceived.load(); }
    uint64_t getBytesSent() const { return bytesSent.load(); }

private:
    ServerMetrics() 
        : totalRequests(0), successfulRequests(0), clientErrors(0), serverErrors(0),
          activeConnections(0), peakConnections(0), bytesReceived(0), bytesSent(0) {}

    std::atomic<uint64_t> totalRequests;
    std::atomic<uint64_t> successfulRequests;
    std::atomic<uint64_t> clientErrors;
    std::atomic<uint64_t> serverErrors;
    std::atomic<int> activeConnections;
    std::atomic<int> peakConnections;
    std::atomic<uint64_t> bytesReceived;
    std::atomic<uint64_t> bytesSent;
};

#endif // METRICS_H

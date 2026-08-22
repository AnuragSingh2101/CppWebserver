#include "logger.h"

#include <iostream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <thread>

// Define static mutex
std::mutex Logger::logMutex;

void Logger::info(const std::string& message) {
    log("INFO", message);
}

void Logger::warn(const std::string& message) {
    log("WARN", message);
}

void Logger::error(const std::string& message) {
    log("ERROR", message);
}

std::string Logger::getTimestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    struct tm parts;
    if (localtime_s(&parts, &now_c) != 0) {
        return "0000-00-00 00:00:00";
    }
    std::ostringstream ss;
    ss << std::put_time(&parts, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::string Logger::getThreadIdString() {
    std::ostringstream ss;
    ss << std::this_thread::get_id();
    return ss.str();
}

void Logger::log(const std::string& level, const std::string& message) {
    std::lock_guard<std::mutex> lock(logMutex);
    std::cout << "[" << level << "] "
              << getTimestamp() << " "
              << "[Thread " << getThreadIdString() << "] "
              << message << std::endl;
}

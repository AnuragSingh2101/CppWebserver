#include "server_config.h"
#include "logger.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>

namespace {
    std::string trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, (last - first + 1));
    }
}

ServerConfig ServerConfig::loadFromFile(const std::string& filePath) {
    ServerConfig config;
    std::ifstream file(filePath);
    if (!file.is_open()) {
        Logger::info("Configuration file " + filePath + " not found. Using defaults.");
        return config;
    }

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue; // Skip comments and empty lines
        }

        size_t delimiterPos = line.find('=');
        if (delimiterPos == std::string::npos) {
            continue; // Invalid line format
        }

        std::string key = trim(line.substr(0, delimiterPos));
        std::string val = trim(line.substr(delimiterPos + 1));

        if (key == "PORT") {
            config.port = std::stoi(val);
        } else if (key == "HOST") {
            config.host = val;
        } else if (key == "THREAD_COUNT") {
            config.threadCount = std::stoi(val);
        } else if (key == "REQUEST_TIMEOUT_MS") {
            config.requestTimeoutMs = std::stoi(val);
        } else if (key == "MAX_HEADER_SIZE") {
            config.maxHeaderSize = std::stoull(val);
        } else if (key == "MAX_BODY_SIZE") {
            config.maxBodySize = std::stoull(val);
        } else if (key == "PUBLIC_DIR") {
            config.publicDir = val;
        }
    }

    Logger::info("Loaded configuration from " + filePath);
    return config;
}

bool ServerConfig::overrideWithArgs(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: server.exe [options]\n"
                      << "Options:\n"
                      << "  --port <number>     Port to listen on\n"
                      << "  --threads <number>  Number of worker threads\n"
                      << "  --root <path>       Document root directory\n"
                      << "  --help, -h          Show this help message\n";
            return false;
        } else if (arg == "--port") {
            if (i + 1 >= argc) {
                Logger::error("Error: --port requires a value");
                return false;
            }
            std::string val = argv[++i];
            if (val.empty() || !std::all_of(val.begin(), val.end(), ::isdigit)) {
                Logger::error("Error: Invalid port number: " + val);
                return false;
            }
            port = std::stoi(val);
        } else if (arg == "--threads") {
            if (i + 1 >= argc) {
                Logger::error("Error: --threads requires a value");
                return false;
            }
            std::string val = argv[++i];
            if (val.empty() || !std::all_of(val.begin(), val.end(), ::isdigit)) {
                Logger::error("Error: Invalid thread count: " + val);
                return false;
            }
            threadCount = std::stoi(val);
        } else if (arg == "--root") {
            if (i + 1 >= argc) {
                Logger::error("Error: --root requires a value");
                return false;
            }
            publicDir = argv[++i];
        } else {
            Logger::error("Error: Unknown argument: " + arg);
            return false;
        }
    }
    return true;
}

#include "router.h"
#include <string>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include "logger.h"

using namespace std;

extern std::string g_documentRoot;

bool Router::urlDecode(const string& src, string& dst) {
    dst.clear();
    dst.reserve(src.size());
    for (size_t i = 0; i < src.size(); ++i) {
        if (src[i] == '%') {
            if (i + 2 >= src.size()) {
                return false; // Malformed percent encoding
            }
            char h1 = src[i+1];
            char h2 = src[i+2];
            if (!isxdigit(static_cast<unsigned char>(h1)) || !isxdigit(static_cast<unsigned char>(h2))) {
                return false;
            }
            string hex = src.substr(i+1, 2);
            char byte = static_cast<char>(stoi(hex, nullptr, 16));
            dst.push_back(byte);
            i += 2;
        } else {
            dst.push_back(src[i]);
        }
    }
    return true;
}

string Router::getFilePath(const string& path) {
    string decodedPath;
    if (!urlDecode(path, decodedPath)) {
        return ""; // Decoding error
    }

    // Strip leading slashes to resolve relative to root
    string relPathStr = decodedPath;
    while (!relPathStr.empty() && (relPathStr.front() == '/' || relPathStr.front() == '\\')) {
        relPathStr.erase(relPathStr.begin());
    }

    if (relPathStr.empty()) {
        relPathStr = "index.html";
    }

    try {
        std::filesystem::path rootPath = std::filesystem::canonical(g_documentRoot);
        std::filesystem::path targetPath = rootPath / relPathStr;
        std::filesystem::path canonicalTarget = std::filesystem::weakly_canonical(targetPath);

        // Prefix match to prevent directory traversal
        auto rootIt = rootPath.begin();
        auto targetIt = canonicalTarget.begin();
        bool isSubpath = true;
        while (rootIt != rootPath.end()) {
            if (targetIt == canonicalTarget.end() || *rootIt != *targetIt) {
                isSubpath = false;
                break;
            }
            ++rootIt;
            ++targetIt;
        }

        if (!isSubpath) {
            Logger::error("Directory traversal attempt blocked: " + path);
            return "";
        }

        // Hidden files check
        for (const auto& part : canonicalTarget) {
            string partStr = part.string();
            if (!partStr.empty() && partStr.front() == '.' && partStr != "." && partStr != "..") {
                Logger::error("Hidden file access blocked: " + path);
                return "";
            }
        }

        // Directory index fallback
        if (std::filesystem::is_directory(canonicalTarget)) {
            std::filesystem::path indexPath = canonicalTarget / "index.html";
            if (std::filesystem::exists(indexPath) && std::filesystem::is_regular_file(indexPath)) {
                return indexPath.string();
            }
            return "";
        }

        // Regular file existence check
        if (std::filesystem::exists(canonicalTarget) && std::filesystem::is_regular_file(canonicalTarget)) {
            return canonicalTarget.string();
        }
    } catch (const std::exception& e) {
        Logger::error("Error resolving path " + path + ": " + e.what());
    }

    return "";
}

string Router::getMimeType(const string& filePath) {
    std::filesystem::path p(filePath);
    string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".html" || ext == ".htm") return "text/html";
    if (ext == ".css") return "text/css";
    if (ext == ".js") return "application/javascript";
    if (ext == ".json") return "application/json";
    if (ext == ".txt") return "text/plain";
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif") return "image/gif";
    if (ext == ".svg") return "image/svg+xml";
    if (ext == ".ico") return "image/x-icon";

    return "application/octet-stream";
}
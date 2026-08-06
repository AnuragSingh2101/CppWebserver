#include "file_handler.h"
#include <fstream>
#include <sstream>

bool FileHandler::readFile(const std::string& filePath, std::string& content) {
    std::ifstream file(filePath, std::ios::binary);

    if (!file.is_open()) {
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    content = buffer.str();
    return true;
}
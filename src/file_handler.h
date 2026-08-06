#ifndef FILE_HANDLER_H
#define FILE_HANDLER_H

#include <string>

class FileHandler {
public:
    static bool readFile(const std::string& filePath, std::string& content);
};

#endif // FILE_HANDLER_H
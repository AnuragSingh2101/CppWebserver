#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <mutex>

class Logger {
public:
    static void info(const std::string& message);
    static void error(const std::string& message);

private:
    static std::string getTimestamp();
    static std::string getThreadIdString();
    static void log(const std::string& level, const std::string& message);

    static std::mutex logMutex;
};

#endif // LOGGER_H

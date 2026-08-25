#include <iostream>
#include <winsock2.h>
#include <windows.h>
#include "server.h"
#include "server_config.h"
#include "logger.h"

HttpServer* g_server = nullptr;

BOOL WINAPI ctrlHandler(DWORD ctrlType) {
    switch (ctrlType) {
        case CTRL_C_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_BREAK_EVENT:
            Logger::info("Shutdown signal received. Stopping server...");
            if (g_server) {
                g_server->stop();
            }
            return TRUE;
        default:
            return FALSE;
    }
}

int main(int argc, char* argv[]) {
    // Load config from default path
    ServerConfig config = ServerConfig::loadFromFile("config/server.conf");

    // Override with command line arguments if any
    if (!config.overrideWithArgs(argc, argv)) {
        return 0; // Help shown or parse error
    }

    // Set console signal handler
    if (!SetConsoleCtrlHandler(ctrlHandler, TRUE)) {
        Logger::error("Failed to set console control handler.");
        return 1;
    }

    HttpServer server(config);
    g_server = &server;

    Logger::info("Starting server...");
    if (!server.run()) {
        Logger::error("Server failed to run.");
        return 1;
    }

    return 0;
}
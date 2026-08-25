#include <gtest/gtest.h>
#include "../src/router.h"
#include <string>
#include <filesystem>
#include <fstream>

extern std::string g_documentRoot;

TEST(RouterTest, URLDecodeNormal) {
    std::string decoded;
    bool success = Router::urlDecode("hello%20world", decoded);
    EXPECT_TRUE(success);
    EXPECT_EQ(decoded, "hello world");
}

TEST(RouterTest, URLDecodeMalformed) {
    std::string decoded;
    bool success = Router::urlDecode("hello%2", decoded); // truncated hex
    EXPECT_FALSE(success);

    success = Router::urlDecode("hello%2g", decoded); // invalid hex char
    EXPECT_FALSE(success);
}

TEST(RouterTest, GetMimeTypes) {
    EXPECT_EQ(Router::getMimeType("index.html"), "text/html");
    EXPECT_EQ(Router::getMimeType("style.CSS"), "text/css"); // case-insensitive extension check
    EXPECT_EQ(Router::getMimeType("script.js"), "application/javascript");
    EXPECT_EQ(Router::getMimeType("data.json"), "application/json");
    EXPECT_EQ(Router::getMimeType("image.png"), "image/png");
    EXPECT_EQ(Router::getMimeType("image.JPG"), "image/jpeg");
    EXPECT_EQ(Router::getMimeType("unknown.xyz"), "application/octet-stream");
}

TEST(RouterTest, PathTraversalBlocked) {
    std::string oldRoot = g_documentRoot;
    g_documentRoot = "test_public";

    // Setup temporary public folder
    std::filesystem::create_directory("test_public");
    std::filesystem::create_directory("test_public/subdir");
    {
        std::ofstream f("test_public/index.html");
        f << "index";
    }
    {
        std::ofstream f("test_public/subdir/file.txt");
        f << "secret";
    }

    // Normal resolves
    std::string resolved = Router::getFilePath("/");
    EXPECT_FALSE(resolved.empty());
    
    resolved = Router::getFilePath("/subdir/file.txt");
    EXPECT_FALSE(resolved.empty());

    // Traversal attempts
    resolved = Router::getFilePath("/../CMakeLists.txt");
    EXPECT_TRUE(resolved.empty());

    resolved = Router::getFilePath("/subdir/%2e%2e/%2e%2e/CMakeLists.txt");
    EXPECT_TRUE(resolved.empty());

    // Windows specific separators
    resolved = Router::getFilePath("/..\\..\\CMakeLists.txt");
    EXPECT_TRUE(resolved.empty());

    // Hidden files check
    {
        std::ofstream f("test_public/.env");
        f << "secret";
    }
    resolved = Router::getFilePath("/.env");
    EXPECT_TRUE(resolved.empty());

    // Cleanup
    std::filesystem::remove_all("test_public");
    g_documentRoot = oldRoot;
}

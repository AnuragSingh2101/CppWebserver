#include <gtest/gtest.h>
#include "../src/http_request.h"

TEST(HttpRequestTest, ValidParsing) {
    std::string reqStr = 
        "GET /index.html?name=John&age=30 HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "User-Agent: Mozilla\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "hello";

    HttpRequest request(reqStr);
    EXPECT_TRUE(request.isValid());
    EXPECT_EQ(request.getMethod(), "GET");
    EXPECT_EQ(request.getPath(), "/index.html");
    EXPECT_EQ(request.getQueryString(), "name=John&age=30");
    EXPECT_EQ(request.getQueryParameter("name"), "John");
    EXPECT_EQ(request.getQueryParameter("age"), "30");
    EXPECT_EQ(request.getHeader("Host"), "localhost");
    EXPECT_EQ(request.getHeader("User-Agent"), "Mozilla");
    EXPECT_EQ(request.getBody(), "hello");
}

TEST(HttpRequestTest, MissingHostHeader) {
    std::string reqStr = 
        "GET /index.html HTTP/1.1\r\n"
        "User-Agent: Mozilla\r\n"
        "\r\n";

    HttpRequest request(reqStr);
    EXPECT_FALSE(request.isValid());
    EXPECT_EQ(request.getErrorCode(), 400);
}

TEST(HttpRequestTest, SpaceInHeaderName) {
    std::string reqStr = 
        "GET /index.html HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Bad Header: value\r\n"
        "\r\n";

    HttpRequest request(reqStr);
    EXPECT_FALSE(request.isValid());
    EXPECT_EQ(request.getErrorCode(), 400);
}

TEST(HttpRequestTest, DuplicateHostHeader) {
    std::string reqStr = 
        "GET /index.html HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Host: otherhost\r\n"
        "\r\n";

    HttpRequest request(reqStr);
    EXPECT_FALSE(request.isValid());
    EXPECT_EQ(request.getErrorCode(), 400);
}

TEST(HttpRequestTest, EmptyRequest) {
    HttpRequest request("");
    EXPECT_FALSE(request.isValid());
    EXPECT_EQ(request.getErrorCode(), 400);
}

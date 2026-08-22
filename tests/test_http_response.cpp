#include <gtest/gtest.h>
#include "../src/http_response.h"

TEST(HttpResponseTest, SimpleResponse) {
    HttpResponse response(200, "text/plain", "Hello World");
    std::string str = response.toString();
    
    EXPECT_NE(str.find("HTTP/1.1 200 OK"), std::string::npos);
    EXPECT_NE(str.find("Content-Type: text/plain"), std::string::npos);
    EXPECT_NE(str.find("Content-Length: 11"), std::string::npos);
    EXPECT_NE(str.find("\r\n\r\nHello World"), std::string::npos);
}

TEST(HttpResponseTest, CustomHeaders) {
    HttpResponse response(302, "text/html", "");
    response.setHeader("Location", "http://example.com");
    std::string str = response.toString();
    
    EXPECT_NE(str.find("HTTP/1.1 302 Found"), std::string::npos);
    EXPECT_NE(str.find("Location: http://example.com"), std::string::npos);
}

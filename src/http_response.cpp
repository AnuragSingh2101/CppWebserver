#include "http_response.h"

HttpResponse::HttpResponse(int statusCode, const std::string& statusMessage, const std::string& contentType, const std::string& body)
    : statusCode(statusCode), statusMessage(statusMessage), contentType(contentType), body(body) {}

std::string HttpResponse::toString() const {
    return "HTTP/1.1 " + std::to_string(statusCode) + " " + statusMessage + "\r\n" +
           "Content-Type: " + contentType + "\r\n" +
           "Content-Length: " + std::to_string(body.size()) + "\r\n" +
           "Connection: close\r\n" +
           "\r\n" +
           body;
}
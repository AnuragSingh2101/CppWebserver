#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include <string>

class HttpResponse {
private:
    int statusCode;
    std::string statusMessage;
    std::string contentType;
    std::string body;

public:
    HttpResponse(int statusCode, const std::string& statusMessage, const std::string& contentType, const std::string& body);
    std::string toString() const;
};

#endif // HTTP_RESPONSE_H

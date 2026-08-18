#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include <string>
#include <map>

class HttpResponse
{
public:

    HttpResponse(
        int statusCode,
        const std::string& statusMessage,
        const std::string& contentType,
        const std::string& body
    );

    void setHeader(
        const std::string& name,
        const std::string& value
    );

    std::string toString() const;

private:
    int statusCode;
    std::string statusMessage;
    std::string contentType;
    std::string body;
    std::map<std::string, std::string> headers;
};

#endif
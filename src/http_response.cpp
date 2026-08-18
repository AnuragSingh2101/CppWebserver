#include "http_response.h"

#include <string>

HttpResponse::HttpResponse(
    int statusCode,
    const std::string& statusMessage,
    const std::string& contentType,
    const std::string& body)
    : statusCode(statusCode),
      statusMessage(statusMessage),
      contentType(contentType),
      body(body)
{
}

void HttpResponse::setHeader(
    const std::string& name,
    const std::string& value)
{
    headers[name] = value;
}

std::string HttpResponse::toString() const
{
    std::string response =
        "HTTP/1.1 " +
        std::to_string(statusCode) +
        " " +
        statusMessage +
        "\r\n";

    response +=
        "Content-Type: " +
        contentType +
        "\r\n";

    response +=
        "Content-Length: " +
        std::to_string(body.size()) +
        "\r\n";

    for (const auto& header : headers)
    {
        response +=
            header.first +
            ": " +
            header.second +
            "\r\n";
    }

    response += "Connection: close\r\n";

    response += "\r\n";
    response += body;
    return response;
}
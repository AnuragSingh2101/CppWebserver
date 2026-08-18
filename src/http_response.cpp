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
      body(body),
      sendBody(true)
{
}

HttpResponse::HttpResponse(
    int statusCode,
    const std::string& contentType,
    const std::string& body)
    : statusCode(statusCode),
      contentType(contentType),
      body(body),
      sendBody(true)
{
    switch (statusCode)
    {
        case 200: statusMessage = "OK"; break;
        case 201: statusMessage = "Created"; break;
        case 204: statusMessage = "No Content"; break;
        case 400: statusMessage = "Bad Request"; break;
        case 403: statusMessage = "Forbidden"; break;
        case 404: statusMessage = "Not Found"; break;
        case 405: statusMessage = "Method Not Allowed"; break;
        case 413: statusMessage = "Payload Too Large"; break;
        case 415: statusMessage = "Unsupported Media Type"; break;
        case 500: statusMessage = "Internal Server Error"; break;
        case 501: statusMessage = "Not Implemented"; break;
        default:  statusMessage = "OK"; break;
    }
}

void HttpResponse::setHeader(
    const std::string& name,
    const std::string& value)
{
    headers[name] = value;
}

void HttpResponse::setSendBody(bool send)
{
    sendBody = send;
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
    if (sendBody)
    {
        response += body;
    }
    return response;
}
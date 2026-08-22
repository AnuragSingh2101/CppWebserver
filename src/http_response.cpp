#include "http_response.h"
#include "http_status.h"

#include <string>
#include <algorithm>

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
    statusMessage = getHttpStatusReason(statusCode);
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

int HttpResponse::getStatusCode() const
{
    return statusCode;
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

    bool hasConnectionHeader = false;
    for (const auto& header : headers)
    {
        response +=
            header.first +
            ": " +
            header.second +
            "\r\n";

        std::string lowerKey = header.first;
        std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), ::tolower);
        if (lowerKey == "connection")
        {
            hasConnectionHeader = true;
        }
    }

    if (!hasConnectionHeader)
    {
        response += "Connection: close\r\n";
    }

    response += "\r\n";
    if (sendBody)
    {
        response += body;
    }
    return response;
}
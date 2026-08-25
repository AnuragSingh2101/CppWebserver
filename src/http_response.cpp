#include "http_response.h"
#include "http_status.h"

#include <string>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

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
    bool hasDateHeader = false;
    bool hasServerHeader = false;

    for (const auto& header : headers)
    {
        std::string lowerKey = header.first;
        std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), ::tolower);
        if (lowerKey == "connection")
        {
            hasConnectionHeader = true;
        }
        else if (lowerKey == "date")
        {
            hasDateHeader = true;
        }
        else if (lowerKey == "server")
        {
            hasServerHeader = true;
        }

        response +=
            header.first +
            ": " +
            header.second +
            "\r\n";
    }

    if (!hasConnectionHeader)
    {
        response += "Connection: close\r\n";
    }

    if (!hasDateHeader)
    {
        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        struct tm gmtTime;
#ifdef _WIN32
        if (gmtime_s(&gmtTime, &now_time) == 0) {
#else
        if (gmtime_r(&now_time, &gmtTime) != nullptr) {
#endif
            char dateBuf[100];
            if (strftime(dateBuf, sizeof(dateBuf), "%a, %d %b %Y %H:%M:%S GMT", &gmtTime) > 0) {
                response += "Date: " + std::string(dateBuf) + "\r\n";
            }
        }
    }

    if (!hasServerHeader)
    {
        response += "Server: Antigravity-C++/1.1\r\n";
    }

    response += "\r\n";
    if (sendBody)
    {
        response += body;
    }
    return response;
}
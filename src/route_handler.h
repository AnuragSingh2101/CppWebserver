#ifndef ROUTE_HANDLER_H
#define ROUTE_HANDLER_H

#include <string>

#include "http_request.h"
#include "http_response.h"

class RouteHandler{
public:

    static HttpResponse handleRequest(
        const HttpRequest& request
    );

private:

    static HttpResponse badRequest(
        const std::string& message
    );

    static HttpResponse notFound(
        const std::string& message
    );

    static HttpResponse forbidden(
        const std::string& message
    );

    static HttpResponse methodNotAllowed(
        const std::string& allowedMethods
    );

    static HttpResponse payloadTooLarge(
        const std::string& message
    );

    static HttpResponse jsonError(
        int statusCode,
        const std::string& code,
        const std::string& message
    );

    static std::string hello(
        const std::string& name,
        const std::string& age
    );

    static std::string about();
    static std::string contact();
};

#endif
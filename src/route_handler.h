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

    static HttpResponse methodNotAllowed(
        const std::string& allowedMethod
    );

private:

    static std::string hello(
        const std::string& name,
        const std::string& age
    );

    static std::string about();
    static std::string contact();
};

#endif
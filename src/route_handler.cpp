#include "route_handler.h"

#include "file_handler.h"
#include "router.h"

using namespace std;

// Handle Incoming Request
HttpResponse RouteHandler::handleRequest(const HttpRequest& request){
    const string path = request.getPath();

    // Dynamic Route : /hello
    if (path == "/hello"){
        string name = request.getQueryParameter("name");
        string age = request.getQueryParameter("age");
        return HttpResponse(
            200,
            "OK",
            "text/plain",
            hello(name, age)
        );
    }

    // Dynamic Route : /about
    if (path == "/about"){
        return HttpResponse(
            200,
            "OK",
            "text/html",
            about()
        );
    }

    // Dynamic Route : /contact
    if (path == "/contact"){
        return HttpResponse(
            200,
            "OK",
            "text/html",
            contact()
        );
    }

    // Directory Traversal Protection
    if (path.find("..") != string::npos){
        return HttpResponse(
            403,
            "Forbidden",
            "text/plain",
            "403 Forbidden"
        );
    }

    // Static File Routing
    const string filePath = Router::getFilePath(path);

    if (filePath.empty()){
        return HttpResponse(
            404,
            "Not Found",
            "text/plain",
            "404 Not Found"
        );
    }

    // Read File
    string fileContent;

    if (!FileHandler::readFile(filePath, fileContent)){
        return HttpResponse(
            404,
            "Not Found",
            "text/plain",
            "404 Not Found"
        );
    }

    // Return File
    return HttpResponse(
        200,
        "OK",
        Router::getMimeType(filePath),
        fileContent
    );
}

// Dynamic Route
string RouteHandler::hello(const string& name, const string& age){
    if (name.empty()) {
        return "Hello from Dynamic Route!";
    }
    if (age.empty()) {
        return "Hello, " + name + "!";
    }
    return "Hello, " + name + "! You are " + age + " years old.";
}

string RouteHandler::about(){
    return R"(
<!DOCTYPE html>
<html>
<head>
    <title>About</title>
</head>
<body>
    <h1>About Page</h1>
    <p>This page is generated dynamically using C++ for about section.</p>
</body>
</html>
)";
}


std::string RouteHandler::contact(){
    return R"(
<!DOCTYPE html>
<html>
<head>
    <title>Contact</title>
</head>
<body>
    <h1>Contact Us</h1>
    <p>Email: demo@example.com</p>
</body>
</html>
)";
}
#include "route_handler.h"

#include "file_handler.h"
#include "router.h"
#include "user_store.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <cctype>
#include <iostream>

using namespace std;

namespace {
    // Helper function to extract a string value for a given key from a JSON-like string
    bool extractJsonString(const string& json, const string& key, string& value) {
        string quotedKey = "\"" + key + "\"";
        size_t pos = 0;
        while ((pos = json.find(quotedKey, pos)) != string::npos) {
            // Verify this is a key, not a substring in a value.
            // A key must be preceded by '{', ',', or whitespace.
            bool isValidKey = false;
            size_t temp = pos;
            if (temp == 0) {
                isValidKey = true;
            } else {
                while (temp > 0) {
                    temp--;
                    if (!isspace(static_cast<unsigned char>(json[temp]))) {
                        if (json[temp] == '{' || json[temp] == ',') {
                            isValidKey = true;
                        }
                        break;
                    }
                }
            }

            if (!isValidKey) {
                pos += quotedKey.length();
                continue;
            }

            // Now find the colon after the key
            size_t colonPos = json.find(':', pos + quotedKey.length());
            if (colonPos == string::npos) {
                pos += quotedKey.length();
                continue;
            }

            // Verify there is only whitespace between key and colon
            bool onlyWhitespaceBeforeColon = true;
            for (size_t i = pos + quotedKey.length(); i < colonPos; ++i) {
                if (!isspace(static_cast<unsigned char>(json[i]))) {
                    onlyWhitespaceBeforeColon = false;
                    break;
                }
            }
            if (!onlyWhitespaceBeforeColon) {
                pos += quotedKey.length();
                continue;
            }

            // Now find the opening quote of the value
            size_t valueStartQuote = json.find('"', colonPos + 1);
            if (valueStartQuote == string::npos) {
                return false; // Malformed JSON (missing value quote)
            }

            // Verify there is only whitespace between colon and opening quote
            bool onlyWhitespaceBeforeValue = true;
            for (size_t i = colonPos + 1; i < valueStartQuote; ++i) {
                if (!isspace(static_cast<unsigned char>(json[i]))) {
                    onlyWhitespaceBeforeValue = false;
                    break;
                }
            }
            if (!onlyWhitespaceBeforeValue) {
                pos += quotedKey.length();
                continue;
            }

            // Find the closing quote of the value
            size_t valueEndQuote = json.find('"', valueStartQuote + 1);
            if (valueEndQuote == string::npos) {
                return false; // Malformed JSON (missing closing quote)
            }

            // Extract the value
            value = json.substr(valueStartQuote + 1, valueEndQuote - valueStartQuote - 1);
            return true;
        }
        return false; // Key not found
    }

    // Helper function to check if a key exists in a JSON-like string
    bool hasJsonKey(const string& json, const string& key) {
        string quotedKey = "\"" + key + "\"";
        size_t pos = 0;
        while ((pos = json.find(quotedKey, pos)) != string::npos) {
            // Verify this is a key, not a substring in a value.
            bool isValidKey = false;
            size_t temp = pos;
            if (temp == 0) {
                isValidKey = true;
            } else {
                while (temp > 0) {
                    temp--;
                    if (!isspace(static_cast<unsigned char>(json[temp]))) {
                        if (json[temp] == '{' || json[temp] == ',') {
                            isValidKey = true;
                        }
                        break;
                    }
                }
            }

            if (!isValidKey) {
                pos += quotedKey.length();
                continue;
            }

            // Now find the colon after the key
            size_t colonPos = json.find(':', pos + quotedKey.length());
            if (colonPos == string::npos) {
                pos += quotedKey.length();
                continue;
            }

            // Verify there is only whitespace between key and colon
            bool onlyWhitespaceBeforeColon = true;
            for (size_t i = pos + quotedKey.length(); i < colonPos; ++i) {
                if (!isspace(static_cast<unsigned char>(json[i]))) {
                    onlyWhitespaceBeforeColon = false;
                    break;
                }
            }
            if (onlyWhitespaceBeforeColon) {
                return true;
            }
            pos += quotedKey.length();
        }
        return false;
    }

    // Helper function to validate basic email format
    bool isValidEmail(const string& email) {
        size_t atPos = email.find('@');
        if (atPos == string::npos || atPos == 0 || atPos == email.length() - 1) {
            return false;
        }
        // Verify there is only one '@'
        if (email.find('@', atPos + 1) != string::npos) {
            return false;
        }
        size_t dotPos = email.find('.', atPos + 1);
        if (dotPos == string::npos || dotPos == atPos + 1 || dotPos == email.length() - 1) {
            return false;
        }
        return true;
    }

    // Helper function to escape JSON output strings
    string escapeJsonString(const string& input) {
        ostringstream ss;
        for (char c : input) {
            switch (c) {
                case '"':  ss << "\\\""; break;
                case '\\': ss << "\\\\"; break;
                case '\n': ss << "\\n"; break;
                case '\r': ss << "\\r"; break;
                case '\t': ss << "\\t"; break;
                default:
                    if (static_cast<unsigned char>(c) < 32) {
                        ss << "\\u" << setfill('0') << setw(4) << hex << static_cast<int>(c);
                    } else {
                        ss << c;
                    }
                    break;
            }
        }
        return ss.str();
    }
}

// Global user store
UserStore userStore;


// ============================================================
// Handle Incoming Request
// ============================================================

HttpResponse RouteHandler::handleRequest(
    const HttpRequest& request)
{
    const string rawMethod = request.getMethod();
    const string path = request.getPath();
    const string version = request.getVersion();

    // ========================================================
    // Request Validation
    // ========================================================
    if (rawMethod.empty())
    {
        return badRequest("Missing HTTP method");
    }
    if (path.empty())
    {
        return badRequest("Missing request path");
    }
    if (version.empty())
    {
        return badRequest("Missing HTTP version");
    }

    // ========================================================
    // HTTP Method Validation
    // ========================================================
    if (rawMethod != "GET" && rawMethod != "POST" && rawMethod != "PUT" &&
        rawMethod != "DELETE" && rawMethod != "PATCH" && rawMethod != "HEAD" &&
        rawMethod != "OPTIONS")
    {
        return HttpResponse(501, "text/plain", "Not Implemented");
    }

    // ========================================================
    // HTTP Version Validation
    // ========================================================
    if (version != "HTTP/1.1")
    {
        return HttpResponse(
            505,
            "HTTP Version Not Supported",
            "text/plain",
            "HTTP Version Not Supported"
        );
    }

    // ========================================================
    // OPTIONS Support
    // ========================================================
    if (rawMethod == "OPTIONS")
    {
        if (path == "/hello")
        {
            HttpResponse response(204, "text/plain", "");
            response.setHeader("Allow", "GET, POST, OPTIONS");
            return response;
        }
        if (path == "/api/users")
        {
            HttpResponse response(204, "text/plain", "");
            response.setHeader("Allow", "GET, POST, PUT, PATCH, DELETE, OPTIONS");
            return response;
        }
        if (path == "/api/time" || path == "/api/info" || path == "/about" || path == "/contact")
        {
            HttpResponse response(204, "text/plain", "");
            response.setHeader("Allow", "GET, OPTIONS");
            return response;
        }
        // Fallback for static files
        const string filePath = Router::getFilePath(path);
        if (!filePath.empty()) {
            HttpResponse response(204, "text/plain", "");
            response.setHeader("Allow", "GET, OPTIONS");
            return response;
        }
        return notFound("404 Not Found");
    }

    // Map HEAD to GET internally for routing logic below
    const string method = (rawMethod == "HEAD") ? "GET" : rawMethod;

    // ========================================================
    // Security: Prevent Directory Traversal
    // ========================================================

    if (path.find("..") != string::npos)
    {
        return forbidden(
            "Directory traversal is not allowed"
        );
    }


    // ========================================================
    // GET /hello
    // ========================================================

    if (path == "/hello" && method == "GET")
    {
        string name =
            request.getQueryParameter("name");

        string age =
            request.getQueryParameter("age");


        if (name.empty())
        {
            return HttpResponse(
                200,
                "OK",
                "text/plain",
                hello("", "")
            );
        }


        if (age.empty())
        {
            return HttpResponse(
                200,
                "OK",
                "text/plain",
                "Hello, " + name + "!"
            );
        }


        return HttpResponse(
            200,
            "OK",
            "text/plain",
            "Hello, " +
            name +
            "! You are " +
            age +
            " years old."
        );
    }


    // ========================================================
    // POST /hello
    // ========================================================

    if (path == "/hello" && method == "POST")
    {
        string body =
            request.getBody();

        cout << "POST BODY: ["
             << body
             << "]"
             << endl;


        // Simple JSON extraction for:
        // {"name":"Anurag"}

        size_t nameStart =
            body.find("\"name\"");


        if (nameStart == string::npos)
        {
            return HttpResponse(
                400,
                "Bad Request",
                "text/plain",
                "Missing name"
            );
        }


        size_t colon =
            body.find(':', nameStart);


        if (colon == string::npos)
        {
            return HttpResponse(
                400,
                "Bad Request",
                "text/plain",
                "Invalid JSON"
            );
        }


        size_t firstQuote =
            body.find('"', colon);


        if (firstQuote == string::npos)
        {
            return HttpResponse(
                400,
                "Bad Request",
                "text/plain",
                "Invalid JSON"
            );
        }


        size_t secondQuote =
            body.find(
                '"',
                firstQuote + 1
            );


        if (secondQuote == string::npos)
        {
            return HttpResponse(
                400,
                "Bad Request",
                "text/plain",
                "Invalid JSON"
            );
        }


        string name =
            body.substr(
                firstQuote + 1,
                secondQuote - firstQuote - 1
            );


        return HttpResponse(
            200,
            "OK",
            "text/plain",
            "Hello, " +
            name +
            "!"
        );
    }


    // ========================================================
    // /hello - Other HTTP Methods
    // ========================================================

    if (path == "/hello")
    {
        return methodNotAllowed(
            "GET, POST"
        );
    }


    // ========================================================
    // GET /api/time
    // ========================================================

    if (path == "/api/time" &&
        method == "GET")
    {
        auto now =
            chrono::system_clock::now();


        time_t currentTime =
            chrono::system_clock::to_time_t(now);


        tm localTime{};


#ifdef _WIN32

        localtime_s(
            &localTime,
            &currentTime
        );

#else

        localtime_r(
            &currentTime,
            &localTime
        );

#endif


        ostringstream timeStream;


        timeStream << put_time(
            &localTime,
            "%H:%M:%S"
        );


        string json =
            "{"
            "\"status\":\"success\","
            "\"time\":\"" +
            timeStream.str() +
            "\""
            "}";


        return HttpResponse(
            200,
            "OK",
            "application/json",
            json
        );
    }


    // ========================================================
    // GET /api/info
    // ========================================================

    if (path == "/api/info" &&
        method == "GET")
    {
        string json =
            "{"
            "\"name\":\"C++ HTTP Server\","
            "\"version\":\"1.0.0\","
            "\"language\":\"C++\","
            "\"protocol\":\"HTTP/1.1\","
            "\"port\":8080"
            "}";


        return HttpResponse(
            200,
            "OK",
            "application/json",
            json
        );
    }


    // ========================================================
    // GET /api/users
    // ========================================================

    if (path == "/api/users" &&
        method == "GET")
    {
        string idString =
            request.getQueryParameter("id");


        // ----------------------------------------------------
        // GET /api/users?id=N
        // ----------------------------------------------------

        if (!idString.empty())
        {
            // Validate ID
            for (char c : idString)
            {
                if (!isdigit(
                        static_cast<unsigned char>(c)))
                {
                    return badRequest("Invalid user id");
                }
            }


            int id =
                stoi(idString);


            auto userOpt =
                userStore.getUserById(id);


            if (!userOpt.has_value())
            {
                return notFound("User not found");
            }

            const User& user = userOpt.value();


            string json =
                "{"
                "\"id\":" +
                to_string(user.id) +
                ","
                "\"name\":\"" +
                escapeJsonString(user.name) +
                "\","
                "\"email\":\"" +
                escapeJsonString(user.email) +
                "\""
                "}";


            return HttpResponse(
                200,
                "OK",
                "application/json",
                json
            );
        }


        // ----------------------------------------------------
        // GET /api/users
        // ----------------------------------------------------

        vector<User> users =
            userStore.getAllUsers();


        string json = "[";


        for (size_t i = 0;
             i < users.size();
             i++)
        {
            json +=
                "{"
                "\"id\":" +
                to_string(users[i].id) +
                ","
                "\"name\":\"" +
                escapeJsonString(users[i].name) +
                "\","
                "\"email\":\"" +
                escapeJsonString(users[i].email) +
                "\""
                "}";


            if (i + 1 < users.size())
            {
                json += ",";
            }
        }


        json += "]";


        return HttpResponse(
            200,
            "OK",
            "application/json",
            json
        );
    }


    // ========================================================
    // POST /api/users
    // ========================================================

    if (path == "/api/users" &&
        method == "POST")
    {
        // Content-Type validation
        string contentType = request.getHeader("Content-Type");
        if (contentType.find("application/json") == string::npos)
        {
            return HttpResponse(415, "Unsupported Media Type", "text/plain", "Unsupported Media Type");
        }

        string body =
            request.getBody();


        cout << "POST /api/users BODY: ["
             << body
             << "]"
             << endl;


        cout << "BODY LENGTH: "
             << body.size()
             << endl;


        // ----------------------------------------------------
        // Basic JSON validation
        // ----------------------------------------------------
        size_t firstNonSpace = body.find_first_not_of(" \t\r\n");
        size_t lastNonSpace = body.find_last_not_of(" \t\r\n");
        if (firstNonSpace == string::npos || lastNonSpace == string::npos ||
            body[firstNonSpace] != '{' || body[lastNonSpace] != '}')
        {
            return badRequest("Invalid JSON");
        }

        string name;
        string email;

        // Try extracting name
        if (!hasJsonKey(body, "name"))
        {
            return badRequest("Missing name");
        }
        if (!extractJsonString(body, "name", name))
        {
            return badRequest("Invalid JSON");
        }
        if (name.empty())
        {
            return badRequest("Missing name");
        }

        // Try extracting email
        if (!hasJsonKey(body, "email"))
        {
            return badRequest("Missing email");
        }
        if (!extractJsonString(body, "email", email))
        {
            return badRequest("Invalid JSON");
        }
        if (email.empty())
        {
            return badRequest("Missing email");
        }
        if (!isValidEmail(email))
        {
            return badRequest("Invalid email");
        }


        // ----------------------------------------------------
        // Create user
        // ----------------------------------------------------

        User newUser =
            userStore.addUser(
                name,
                email
            );


        string json =
            "{"
            "\"id\":" +
            to_string(newUser.id) +
            ","
            "\"name\":\"" +
            escapeJsonString(newUser.name) +
            "\","
            "\"email\":\"" +
            escapeJsonString(newUser.email) +
            "\""
            "}";


        return HttpResponse(
            201,
            "Created",
            "application/json",
            json
        );
    }


    // ========================================================
    // PUT /api/users?id=N
    // ========================================================

    if (path == "/api/users" &&
        method == "PUT")
    {
        // Content-Type validation
        string contentType = request.getHeader("Content-Type");
        if (contentType.find("application/json") == string::npos)
        {
            return HttpResponse(415, "Unsupported Media Type", "text/plain", "Unsupported Media Type");
        }

        string idString = request.getQueryParameter("id");
        if (idString.empty())
        {
            return badRequest("Missing user id");
        }
        for (char c : idString)
        {
            if (!isdigit(static_cast<unsigned char>(c)))
            {
                return badRequest("Invalid user id");
            }
        }
        int id = stoi(idString);

        string body = request.getBody();

        // Basic JSON validation
        size_t firstNonSpace = body.find_first_not_of(" \t\r\n");
        size_t lastNonSpace = body.find_last_not_of(" \t\r\n");
        if (firstNonSpace == string::npos || lastNonSpace == string::npos ||
            body[firstNonSpace] != '{' || body[lastNonSpace] != '}')
        {
            return badRequest("Invalid JSON");
        }

        string name;
        string email;

        // Try extracting name
        if (!hasJsonKey(body, "name"))
        {
            return badRequest("Missing name");
        }
        if (!extractJsonString(body, "name", name))
        {
            return badRequest("Invalid JSON");
        }
        if (name.empty())
        {
            return badRequest("Missing name");
        }

        // Try extracting email
        if (!hasJsonKey(body, "email"))
        {
            return badRequest("Missing email");
        }
        if (!extractJsonString(body, "email", email))
        {
            return badRequest("Invalid JSON");
        }
        if (email.empty())
        {
            return badRequest("Missing email");
        }
        if (!isValidEmail(email))
        {
            return badRequest("Invalid email");
        }

        auto updatedOpt = userStore.updateUser(id, name, email);
        if (!updatedOpt.has_value())
        {
            return notFound("User not found");
        }
        const User& updated = updatedOpt.value();

        string json =
            "{"
            "\"id\":" + to_string(updated.id) + ","
            "\"name\":\"" + escapeJsonString(updated.name) + "\","
            "\"email\":\"" + escapeJsonString(updated.email) + "\""
            "}";

        return HttpResponse(200, "application/json", json);
    }

    // ========================================================
    // PATCH /api/users?id=N
    // ========================================================

    if (path == "/api/users" &&
        method == "PATCH")
    {
        // Content-Type validation
        string contentType = request.getHeader("Content-Type");
        if (contentType.find("application/json") == string::npos)
        {
            return HttpResponse(415, "Unsupported Media Type", "text/plain", "Unsupported Media Type");
        }

        string idString = request.getQueryParameter("id");
        if (idString.empty())
        {
            return badRequest("Missing user id");
        }
        for (char c : idString)
        {
            if (!isdigit(static_cast<unsigned char>(c)))
            {
                return badRequest("Invalid user id");
            }
        }
        int id = stoi(idString);

        string body = request.getBody();

        // Basic JSON validation
        size_t firstNonSpace = body.find_first_not_of(" \t\r\n");
        size_t lastNonSpace = body.find_last_not_of(" \t\r\n");
        if (firstNonSpace == string::npos || lastNonSpace == string::npos ||
            body[firstNonSpace] != '{' || body[lastNonSpace] != '}')
        {
            return badRequest("Invalid JSON");
        }

        bool hasName = hasJsonKey(body, "name");
        bool hasEmail = hasJsonKey(body, "email");

        if (!hasName && !hasEmail)
        {
            return badRequest("Invalid JSON");
        }

        string name = "";
        string email = "";

        if (hasName)
        {
            if (!extractJsonString(body, "name", name))
            {
                return badRequest("Invalid JSON");
            }
            if (name.empty())
            {
                return badRequest("Missing name");
            }
        }

        if (hasEmail)
        {
            if (!extractJsonString(body, "email", email))
            {
                return badRequest("Invalid JSON");
            }
            if (email.empty())
            {
                return badRequest("Missing email");
            }
            if (!isValidEmail(email))
            {
                return badRequest("Invalid email");
            }
        }

        auto updatedOpt = userStore.patchUser(id, name, email);
        if (!updatedOpt.has_value())
        {
            return notFound("User not found");
        }
        const User& updated = updatedOpt.value();

        string json =
            "{"
            "\"id\":" + to_string(updated.id) + ","
            "\"name\":\"" + escapeJsonString(updated.name) + "\","
            "\"email\":\"" + escapeJsonString(updated.email) + "\""
            "}";

        return HttpResponse(200, "application/json", json);
    }


    // ========================================================
    // DELETE /api/users?id=N
    // ========================================================

    if (path == "/api/users" &&
        method == "DELETE")
    {
        string idString =
            request.getQueryParameter("id");


        // ----------------------------------------------------
        // Missing ID
        // ----------------------------------------------------

        if (idString.empty())
        {
            return badRequest("Missing user id");
        }


        // ----------------------------------------------------
        // Validate ID
        // ----------------------------------------------------

        for (char c : idString)
        {
            if (!isdigit(
                    static_cast<unsigned char>(c)))
            {
                return badRequest("Invalid user id");
            }
        }


        int id =
            stoi(idString);


        // ----------------------------------------------------
        // Delete user
        // ----------------------------------------------------

        bool deleted =
            userStore.removeUser(id);


        if (!deleted)
        {
            return notFound("User not found");
        }


        // ----------------------------------------------------
        // Successful deletion
        // ----------------------------------------------------

        return HttpResponse(
            204,
            "No Content",
            "text/plain",
            ""
        );
    }


    // ========================================================
    // /api/users - Unsupported Methods
    // ========================================================

    if (path == "/api/users")
    {
        return methodNotAllowed(
            "GET, POST, DELETE"
        );
    }


    // ========================================================
    // GET /about
    // ========================================================

    if (path == "/about" &&
        method == "GET")
    {
        return HttpResponse(
            200,
            "OK",
            "text/html",
            about()
        );
    }


    // ========================================================
    // GET /contact
    // ========================================================

    if (path == "/contact" &&
        method == "GET")
    {
        return HttpResponse(
            200,
            "OK",
            "text/html",
            contact()
        );
    }


    // ========================================================
    // Unsupported methods for /about
    // ========================================================

    if (path == "/about")
    {
        return methodNotAllowed(
            "GET"
        );
    }


    // ========================================================
    // Unsupported methods for /contact
    // ========================================================

    if (path == "/contact")
    {
        return methodNotAllowed(
            "GET"
        );
    }


    // ========================================================
    // Static Files / Other Routes
    // ========================================================

    const string filePath =
        Router::getFilePath(path);


    if (filePath.empty())
    {
        return notFound("404 Not Found");
    }


    // ========================================================
    // Read Static File
    // ========================================================

    string fileContent;


    if (!FileHandler::readFile(
            filePath,
            fileContent))
    {
        return notFound("404 Not Found");
    }


    // ========================================================
    // Return Static File
    // ========================================================

    return HttpResponse(
        200,
        "OK",
        Router::getMimeType(filePath),
        fileContent
    );
}


// ============================================================
// Dynamic Route: /hello
// ============================================================

string RouteHandler::hello(
    const string& name,
    const string& age)
{
    if (name.empty())
    {
        return "Hello from Dynamic Route!";
    }


    if (age.empty())
    {
        return "Hello, " +
               name +
               "!";
    }


    return "Hello, " +
           name +
           "! You are " +
           age +
           " years old.";
}


// ============================================================
// Dynamic Route: /about
// ============================================================

string RouteHandler::about()
{
    return R"(
<!DOCTYPE html>
<html>
<head>
    <title>About</title>
</head>
<body>
    <h1>About Page</h1>
    <p>
        This page is generated dynamically
        using C++ for about section.
    </p>
</body>
</html>
)";
}


// ============================================================
// Dynamic Route: /contact
// ============================================================

string RouteHandler::contact()
{
    return R"(
<!DOCTYPE html>
<html>
<head>
    <title>Contact Us</title>
</head>
<body>
    <h1>Contact Us</h1>
    <p>Email: demo@example.com</p>
</body>
</html>
)";
}


// ============================================================
// Centralized Error Response Helpers
// ============================================================

HttpResponse RouteHandler::badRequest(
    const string& message)
{
    return HttpResponse(
        400,
        "Bad Request",
        "text/plain",
        message
    );
}

HttpResponse RouteHandler::notFound(
    const string& message)
{
    return HttpResponse(
        404,
        "Not Found",
        "text/plain",
        message
    );
}

HttpResponse RouteHandler::forbidden(
    const string& message)
{
    return HttpResponse(
        403,
        "Forbidden",
        "text/plain",
        message
    );
}

HttpResponse RouteHandler::methodNotAllowed(
    const string& allowedMethods)
{
    HttpResponse response(
        405,
        "Method Not Allowed",
        "text/plain",
        "Method Not Allowed"
    );

    response.setHeader(
        "Allow",
        allowedMethods
    );
    return response;
}

HttpResponse RouteHandler::payloadTooLarge(
    const string& message)
{
    return HttpResponse(
        413,
        "Payload Too Large",
        "text/plain",
        message
    );
}
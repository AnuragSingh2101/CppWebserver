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
}

// Global user store
UserStore userStore;


// ============================================================
// Handle Incoming Request
// ============================================================

HttpResponse RouteHandler::handleRequest(
    const HttpRequest& request)
{
    const string method = request.getMethod();
    const string path = request.getPath();


    // ========================================================
    // Security: Prevent Directory Traversal
    // ========================================================

    if (path.find("..") != string::npos)
    {
        return HttpResponse(
            403,
            "Forbidden",
            "text/plain",
            "403 Forbidden"
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
                    return HttpResponse(
                        400,
                        "Bad Request",
                        "text/plain",
                        "Invalid user id"
                    );
                }
            }


            int id =
                stoi(idString);


            User* user =
                userStore.getUserById(id);


            if (user == nullptr)
            {
                return HttpResponse(
                    404,
                    "Not Found",
                    "text/plain",
                    "User not found"
                );
            }


            string json =
                "{"
                "\"id\":" +
                to_string(user->id) +
                ","
                "\"name\":\"" +
                user->name +
                "\","
                "\"email\":\"" +
                user->email +
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
                users[i].name +
                "\","
                "\"email\":\"" +
                users[i].email +
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
            return HttpResponse(
                400,
                "Bad Request",
                "text/plain",
                "Invalid JSON"
            );
        }

        string name;
        string email;

        // Try extracting name
        if (!hasJsonKey(body, "name"))
        {
            return HttpResponse(
                400,
                "Bad Request",
                "text/plain",
                "Missing name"
            );
        }
        if (!extractJsonString(body, "name", name))
        {
            return HttpResponse(
                400,
                "Bad Request",
                "text/plain",
                "Invalid JSON"
            );
        }

        // Try extracting email
        if (!hasJsonKey(body, "email"))
        {
            return HttpResponse(
                400,
                "Bad Request",
                "text/plain",
                "Missing email"
            );
        }
        if (!extractJsonString(body, "email", email))
        {
            return HttpResponse(
                400,
                "Bad Request",
                "text/plain",
                "Invalid JSON"
            );
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
            newUser.name +
            "\","
            "\"email\":\"" +
            newUser.email +
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
            return HttpResponse(
                400,
                "Bad Request",
                "text/plain",
                "Missing user id"
            );
        }


        // ----------------------------------------------------
        // Validate ID
        // ----------------------------------------------------

        for (char c : idString)
        {
            if (!isdigit(
                    static_cast<unsigned char>(c)))
            {
                return HttpResponse(
                    400,
                    "Bad Request",
                    "text/plain",
                    "Invalid user id"
                );
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
            return HttpResponse(
                404,
                "Not Found",
                "text/plain",
                "User not found"
            );
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
        return HttpResponse(
            404,
            "Not Found",
            "text/plain",
            "404 Not Found"
        );
    }


    // ========================================================
    // Read Static File
    // ========================================================

    string fileContent;


    if (!FileHandler::readFile(
            filePath,
            fileContent))
    {
        return HttpResponse(
            404,
            "Not Found",
            "text/plain",
            "404 Not Found"
        );
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
// Method Not Allowed
// ============================================================

HttpResponse RouteHandler::methodNotAllowed(
    const string& allowedMethod)
{
    HttpResponse response(
        405,
        "Method Not Allowed",
        "text/plain",
        "Method Not Allowed"
    );


    response.setHeader(
        "Allow",
        allowedMethod
    );
    return response;
}
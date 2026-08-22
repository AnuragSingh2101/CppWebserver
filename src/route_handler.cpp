#include "route_handler.h"

#include "file_handler.h"
#include "router.h"
#include "user_store.h"
#include "metrics.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <cctype>
#include <iostream>
#include <nlohmann/json.hpp>

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

    // Parse path parameters for /api/users/{id}
    bool isUserPathWithId = false;
    int pathId = -1;
    if (path.rfind("/api/users/", 0) == 0) {
        string idStr = path.substr(11);
        bool allDigits = !idStr.empty();
        for (char c : idStr) {
            if (!isdigit(static_cast<unsigned char>(c))) {
                allDigits = false;
                break;
            }
        }
        if (allDigits) {
            isUserPathWithId = true;
            try {
                pathId = stoi(idStr);
            } catch (...) {
                isUserPathWithId = false;
            }
        }
    }

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
        if (path == "/api/users" || isUserPathWithId)
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
        nlohmann::json res;
        res["name"] = "C++ HTTP Server";
        res["version"] = "1.0.0";
        res["language"] = "C++";
        res["protocol"] = "HTTP/1.1";

        auto& m = ServerMetrics::getInstance();
        nlohmann::json metricsJson;
        metricsJson["total_requests"] = m.getTotalRequests();
        metricsJson["successful_requests"] = m.getSuccessfulRequests();
        metricsJson["client_errors"] = m.getClientErrors();
        metricsJson["server_errors"] = m.getServerErrors();
        metricsJson["active_connections"] = m.getActiveConnections();
        metricsJson["peak_connections"] = m.getPeakConnections();
        metricsJson["bytes_received"] = m.getBytesReceived();
        metricsJson["bytes_sent"] = m.getBytesSent();
        res["metrics"] = metricsJson;

        return HttpResponse(
            200,
            "OK",
            "application/json",
            res.dump()
        );
    }


    // ========================================================
    // GET, POST, PUT, PATCH, DELETE /api/users
    // ========================================================

    if (path == "/api/users" || isUserPathWithId)
    {
        int id = -1;
        bool hasId = false;

        if (isUserPathWithId) {
            id = pathId;
            hasId = true;
        } else {
            string idString = request.getQueryParameter("id");
            if (!idString.empty()) {
                bool allDigits = true;
                for (char c : idString) {
                    if (!isdigit(static_cast<unsigned char>(c))) {
                        allDigits = false;
                        break;
                    }
                }
                if (!allDigits) {
                    return jsonError(400, "INVALID_ID", "User ID must be a positive integer");
                }
                try {
                    id = stoi(idString);
                    hasId = true;
                } catch (...) {
                    return jsonError(400, "INVALID_ID", "User ID is out of range");
                }
            }
        }

        if (id <= 0 && hasId) {
            return jsonError(400, "INVALID_ID", "User ID must be a positive integer");
        }

        // --- GET Method ---
        if (method == "GET") {
            if (hasId) {
                auto userOpt = UserStore::getUserById(id);
                if (!userOpt.has_value()) {
                    return jsonError(404, "USER_NOT_FOUND", "User with id " + to_string(id) + " was not found");
                }
                const User& user = userOpt.value();
                nlohmann::json resJson;
                resJson["id"] = user.id;
                resJson["name"] = user.name;
                resJson["email"] = user.email;
                return HttpResponse(200, "application/json", resJson.dump());
            } else {
                auto users = UserStore::getAllUsers();
                nlohmann::json usersArr = nlohmann::json::array();
                for (const auto& user : users) {
                    nlohmann::json uJson;
                    uJson["id"] = user.id;
                    uJson["name"] = user.name;
                    uJson["email"] = user.email;
                    usersArr.push_back(uJson);
                }
                return HttpResponse(200, "application/json", usersArr.dump());
            }
        }

        // --- POST Method ---
        else if (method == "POST") {
            if (hasId) {
                return jsonError(400, "INVALID_ID", "Cannot provide an ID when creating a user");
            }
            string contentType = request.getHeader("Content-Type");
            if (contentType.find("application/json") == string::npos) {
                return jsonError(415, "UNSUPPORTED_MEDIA_TYPE", "Content-Type must be application/json");
            }

            nlohmann::json bodyJson;
            try {
                bodyJson = nlohmann::json::parse(request.getBody());
            } catch (...) {
                return jsonError(400, "INVALID_JSON", "Malformed JSON payload");
            }

            if (!bodyJson.is_object()) {
                return jsonError(400, "INVALID_JSON", "JSON body must be an object");
            }

            if (!bodyJson.contains("name")) {
                return jsonError(400, "MISSING_FIELD", "Missing required field: name");
            }
            if (!bodyJson.contains("email")) {
                return jsonError(400, "MISSING_FIELD", "Missing required field: email");
            }

            if (!bodyJson["name"].is_string()) {
                return jsonError(400, "INVALID_FIELD_TYPE", "Field 'name' must be a string");
            }
            if (!bodyJson["email"].is_string()) {
                return jsonError(400, "INVALID_FIELD_TYPE", "Field 'email' must be a string");
            }

            string name = bodyJson["name"].get<string>();
            string email = bodyJson["email"].get<string>();

            // Trim
            while (!name.empty() && isspace(static_cast<unsigned char>(name.front()))) name.erase(name.begin());
            while (!name.empty() && isspace(static_cast<unsigned char>(name.back()))) name.pop_back();
            while (!email.empty() && isspace(static_cast<unsigned char>(email.front()))) email.erase(email.begin());
            while (!email.empty() && isspace(static_cast<unsigned char>(email.back()))) email.pop_back();

            if (name.empty()) {
                return jsonError(400, "MISSING_FIELD", "Name field cannot be empty");
            }
            if (name.size() > 100) {
                return jsonError(400, "INVALID_EMAIL", "Name exceeds maximum length of 100 characters");
            }
            if (email.empty()) {
                return jsonError(400, "MISSING_FIELD", "Email field cannot be empty");
            }
            if (email.size() > 255) {
                return jsonError(400, "INVALID_EMAIL", "Email exceeds maximum length of 255 characters");
            }
            if (!isValidEmail(email)) {
                return jsonError(400, "INVALID_EMAIL", "Invalid email format");
            }

            auto addResult = UserStore::addUser(name, email);
            if (addResult.first == StoreResult::DUPLICATE_EMAIL) {
                return jsonError(409, "DUPLICATE_EMAIL", "Email address already in use: " + email);
            }

            const User& newUser = addResult.second.value();
            nlohmann::json resJson;
            resJson["id"] = newUser.id;
            resJson["name"] = newUser.name;
            resJson["email"] = newUser.email;
            return HttpResponse(201, "application/json", resJson.dump());
        }

        // --- PUT Method ---
        else if (method == "PUT") {
            if (!hasId) {
                return jsonError(400, "MISSING_FIELD", "Missing required query parameter or path parameter: id");
            }
            string contentType = request.getHeader("Content-Type");
            if (contentType.find("application/json") == string::npos) {
                return jsonError(415, "UNSUPPORTED_MEDIA_TYPE", "Content-Type must be application/json");
            }

            nlohmann::json bodyJson;
            try {
                bodyJson = nlohmann::json::parse(request.getBody());
            } catch (...) {
                return jsonError(400, "INVALID_JSON", "Malformed JSON payload");
            }

            if (!bodyJson.is_object()) {
                return jsonError(400, "INVALID_JSON", "JSON body must be an object");
            }

            if (!bodyJson.contains("name")) {
                return jsonError(400, "MISSING_FIELD", "Missing required field: name");
            }
            if (!bodyJson.contains("email")) {
                return jsonError(400, "MISSING_FIELD", "Missing required field: email");
            }

            if (!bodyJson["name"].is_string()) {
                return jsonError(400, "INVALID_FIELD_TYPE", "Field 'name' must be a string");
            }
            if (!bodyJson["email"].is_string()) {
                return jsonError(400, "INVALID_FIELD_TYPE", "Field 'email' must be a string");
            }

            string name = bodyJson["name"].get<string>();
            string email = bodyJson["email"].get<string>();

            // Trim
            while (!name.empty() && isspace(static_cast<unsigned char>(name.front()))) name.erase(name.begin());
            while (!name.empty() && isspace(static_cast<unsigned char>(name.back()))) name.pop_back();
            while (!email.empty() && isspace(static_cast<unsigned char>(email.front()))) email.erase(email.begin());
            while (!email.empty() && isspace(static_cast<unsigned char>(email.back()))) email.pop_back();

            if (name.empty()) {
                return jsonError(400, "MISSING_FIELD", "Name field cannot be empty");
            }
            if (name.size() > 100) {
                return jsonError(400, "INVALID_EMAIL", "Name exceeds maximum length of 100 characters");
            }
            if (email.empty()) {
                return jsonError(400, "MISSING_FIELD", "Email field cannot be empty");
            }
            if (email.size() > 255) {
                return jsonError(400, "INVALID_EMAIL", "Email exceeds maximum length of 255 characters");
            }
            if (!isValidEmail(email)) {
                return jsonError(400, "INVALID_EMAIL", "Invalid email format");
            }

            auto updateResult = UserStore::updateUser(id, name, email);
            if (updateResult.first == StoreResult::NOT_FOUND) {
                return jsonError(404, "USER_NOT_FOUND", "User with id " + to_string(id) + " was not found");
            }
            if (updateResult.first == StoreResult::DUPLICATE_EMAIL) {
                return jsonError(409, "DUPLICATE_EMAIL", "Email address already in use by another user: " + email);
            }

            const User& updatedUser = updateResult.second.value();
            nlohmann::json resJson;
            resJson["id"] = updatedUser.id;
            resJson["name"] = updatedUser.name;
            resJson["email"] = updatedUser.email;
            return HttpResponse(200, "application/json", resJson.dump());
        }

        // --- PATCH Method ---
        else if (method == "PATCH") {
            if (!hasId) {
                return jsonError(400, "MISSING_FIELD", "Missing required query parameter or path parameter: id");
            }
            string contentType = request.getHeader("Content-Type");
            if (contentType.find("application/json") == string::npos) {
                return jsonError(415, "UNSUPPORTED_MEDIA_TYPE", "Content-Type must be application/json");
            }

            nlohmann::json bodyJson;
            try {
                bodyJson = nlohmann::json::parse(request.getBody());
            } catch (...) {
                return jsonError(400, "INVALID_JSON", "Malformed JSON payload");
            }

            if (!bodyJson.is_object()) {
                return jsonError(400, "INVALID_JSON", "JSON body must be an object");
            }

            bool hasName = bodyJson.contains("name");
            bool hasEmail = bodyJson.contains("email");

            if (!hasName && !hasEmail) {
                return jsonError(400, "MISSING_FIELD", "Must provide name or email field");
            }

            string name = "";
            string email = "";

            if (hasName) {
                if (!bodyJson["name"].is_string()) {
                    return jsonError(400, "INVALID_FIELD_TYPE", "Field 'name' must be a string");
                }
                name = bodyJson["name"].get<string>();
                while (!name.empty() && isspace(static_cast<unsigned char>(name.front()))) name.erase(name.begin());
                while (!name.empty() && isspace(static_cast<unsigned char>(name.back()))) name.pop_back();
                if (name.empty()) {
                    return jsonError(400, "MISSING_FIELD", "Name field cannot be empty");
                }
                if (name.size() > 100) {
                    return jsonError(400, "INVALID_EMAIL", "Name exceeds maximum length of 100 characters");
                }
            }

            if (hasEmail) {
                if (!bodyJson["email"].is_string()) {
                    return jsonError(400, "INVALID_FIELD_TYPE", "Field 'email' must be a string");
                }
                email = bodyJson["email"].get<string>();
                while (!email.empty() && isspace(static_cast<unsigned char>(email.front()))) email.erase(email.begin());
                while (!email.empty() && isspace(static_cast<unsigned char>(email.back()))) email.pop_back();
                if (email.empty()) {
                    return jsonError(400, "MISSING_FIELD", "Email field cannot be empty");
                }
                if (email.size() > 255) {
                    return jsonError(400, "INVALID_EMAIL", "Email exceeds maximum length of 255 characters");
                }
                if (!isValidEmail(email)) {
                    return jsonError(400, "INVALID_EMAIL", "Invalid email format");
                }
            }

            auto patchResult = UserStore::patchUser(id, name, email);
            if (patchResult.first == StoreResult::NOT_FOUND) {
                return jsonError(404, "USER_NOT_FOUND", "User with id " + to_string(id) + " was not found");
            }
            if (patchResult.first == StoreResult::DUPLICATE_EMAIL) {
                return jsonError(409, "DUPLICATE_EMAIL", "Email address already in use by another user: " + email);
            }

            const User& updatedUser = patchResult.second.value();
            nlohmann::json resJson;
            resJson["id"] = updatedUser.id;
            resJson["name"] = updatedUser.name;
            resJson["email"] = updatedUser.email;
            return HttpResponse(200, "application/json", resJson.dump());
        }

        // --- DELETE Method ---
        else if (method == "DELETE") {
            if (!hasId) {
                return jsonError(400, "MISSING_FIELD", "Missing required query parameter or path parameter: id");
            }
            bool deleted = UserStore::removeUser(id);
            if (!deleted) {
                return jsonError(404, "USER_NOT_FOUND", "User with id " + to_string(id) + " was not found");
            }
            return HttpResponse(204, "application/json", "");
        }

        // --- Unsupported Method ---
        else {
            HttpResponse response = jsonError(405, "METHOD_NOT_ALLOWED", "Method not allowed. Allowed methods: GET, POST, PUT, PATCH, DELETE, OPTIONS");
            response.setHeader("Allow", "GET, POST, PUT, PATCH, DELETE, OPTIONS");
            return response;
        }
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

HttpResponse RouteHandler::jsonError(
    int statusCode,
    const string& code,
    const string& message)
{
    nlohmann::json err;
    err["error"]["code"] = code;
    err["error"]["message"] = message;
    return HttpResponse(statusCode, "application/json", err.dump());
}
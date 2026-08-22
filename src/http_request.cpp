#include <string>
#include <sstream>
#include <algorithm>

#include "http_request.h"
using namespace std;

HttpRequest::HttpRequest(const string& rawRequest){
    parse(rawRequest);
}

void HttpRequest::parse(const string& request){
    // Reset previous state
    method.clear();
    path.clear();
    version.clear();
    queryString.clear();
    body.clear();
    headers.clear();
    queryParams.clear();
    valid = true;
    errorCode = 200;
    errorMessage.clear();

    // Find end of HTTP headers
    size_t headerEnd =
        request.find("\r\n\r\n");

    string headerSection;

    if (headerEnd != string::npos){
        headerSection =
            request.substr(0, headerEnd);

        // Everything after \r\n\r\n is the body.
        body = request.substr(headerEnd + 4);
    }
    else{
        headerSection = request;
    }

    // Parse request line
    stringstream stream(headerSection);
    string requestLine;
    if (!getline(stream, requestLine)){
        valid = false;
        errorCode = 400;
        errorMessage = "Empty request line";
        return;
    }

    if (!requestLine.empty() &&
        requestLine.back() == '\r'){
        requestLine.pop_back();
    }

    stringstream lineStream(requestLine);
    lineStream >> method >> path >> version;

    if (method.empty() || path.empty() || version.empty()) {
        valid = false;
        errorCode = 400;
        errorMessage = "Malformed request line";
        return;
    }

    // Separate query string
    size_t questionMark =
        path.find('?');

    if (questionMark != string::npos){
        queryString =
            path.substr(
                questionMark + 1
            );

        path =
            path.substr(
                0,
                questionMark
            );

        parseQueryParams();
    }

    // Parse headers
    string headerLine;
    int hostHeaderCount = 0;
    int headerCount = 0;
    while (getline(stream, headerLine)){
        if (!headerLine.empty() &&
            headerLine.back() == '\r'){
            headerLine.pop_back();
        }

        if (headerLine.empty()) {
            continue;
        }

        headerCount++;
        if (headerCount > 100) {
            valid = false;
            errorCode = 400;
            errorMessage = "Too many headers";
            return;
        }

        size_t colon =
            headerLine.find(':');

        if (colon == string::npos){
            valid = false;
            errorCode = 400;
            errorMessage = "Malformed header line: missing colon";
            return;
        }

        string name =
            headerLine.substr(
                0,
                colon
            );

        string value =
            headerLine.substr(
                colon + 1
            );

        // Trim leading and trailing whitespace from name
        while (!name.empty() && (name.front() == ' ' || name.front() == '\t')) {
            name.erase(name.begin());
        }
        while (!name.empty() && (name.back() == ' ' || name.back() == '\t')) {
            name.pop_back();
        }

        if (name.empty()) {
            valid = false;
            errorCode = 400;
            errorMessage = "Empty header name";
            return;
        }

        // Validate spaces are not present in header name (RFC 7230 section 3.2.4)
        if (name.find(' ') != string::npos || name.find('\t') != string::npos) {
            valid = false;
            errorCode = 400;
            errorMessage = "Header name contains invalid whitespace";
            return;
        }

        // Trim leading and trailing whitespace from value
        while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
            value.erase(value.begin());
        }
        while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r')) {
            value.pop_back();
        }

        // Store header name in lowercase for case-insensitive lookup
        string lowerName = name;
        transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

        if (lowerName == "host") {
            hostHeaderCount++;
        }

        if (headers.count(lowerName) > 0) {
            headers[lowerName] = headers[lowerName] + ", " + value;
        } else {
            headers[lowerName] = value;
        }
    }

    // Host header validation for HTTP/1.1
    if (version == "HTTP/1.1") {
        if (hostHeaderCount == 0) {
            valid = false;
            errorCode = 400;
            errorMessage = "Host header is missing";
            return;
        }
        if (hostHeaderCount > 1) {
            valid = false;
            errorCode = 400;
            errorMessage = "Duplicate Host headers";
            return;
        }
    }

    // Truncate body to Content-Length if specified
    string contentLengthStr = getHeader("Content-Length");
    if (!contentLengthStr.empty()) {
        try {
            size_t contentLength = stoul(contentLengthStr);
            if (body.size() > contentLength) {
                body = body.substr(0, contentLength);
            }
        } catch (...) {
            // Ignore parsing errors
        }
    }
}

// Get Method
string HttpRequest::getMethod() const{
    return method;
}

// Get Path
string HttpRequest::getPath() const{
    return path;
}

// Get HTTP Version
string HttpRequest::getVersion() const{
    return version;
}

// Get Query String
string HttpRequest::getQueryString() const{
    return queryString;
}

// Get Query Parameter
string HttpRequest::getQueryParameter(
    const string& key) const
{
    auto it = queryParams.find(key);
    if (it != queryParams.end()){
        return it->second;
    }
    return "";
}

// Get Header
string HttpRequest::getHeader(
    const string& name) const{
    string lowerName = name;
    transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
    auto it =
        headers.find(lowerName);

    if (it == headers.end()){
        return "";
    }
    return it->second;
}

// Get Body
string HttpRequest::getBody() const{
    return body;
}


// Parse Query Parameters
void HttpRequest::parseQueryParams(){
    stringstream ss(queryString);

    string pair;

    while (getline(ss, pair, '&')){
        size_t equalSign =
            pair.find('=');

        if (equalSign != string::npos){
            string key =
                pair.substr(
                    0,
                    equalSign
                );

            string value =
                pair.substr(
                    equalSign + 1
                );

            queryParams[key] = value;
        }
        else if (!pair.empty()){
            queryParams[pair] = "";
        }
    }
}

bool HttpRequest::isValid() const {
    return valid;
}

int HttpRequest::getErrorCode() const {
    return errorCode;
}

string HttpRequest::getErrorMessage() const {
    return errorMessage;
}
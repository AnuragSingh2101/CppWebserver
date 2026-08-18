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
        return;
    }

    if (!requestLine.empty() &&
        requestLine.back() == '\r'){
        requestLine.pop_back();
    }

    stringstream lineStream(requestLine);
    lineStream >> method >> path >> version;

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
    while (getline(stream, headerLine)){
        if (!headerLine.empty() &&
            headerLine.back() == '\r'){
            headerLine.pop_back();
        }

        size_t colon =
            headerLine.find(':');

        if (colon == string::npos){
            continue;
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

        // Remove leading whitespace
        while (!value.empty() &&
               (value.front() == ' ' ||
                value.front() == '\t'))
        {
            value.erase(
                value.begin()
            );
        }

        headers[name] = value;
    }

    // Truncate body to Content-Length if specified
    string contentLengthStr = "";
    for (const auto& pair : headers) {
        string lowerKey = pair.first;
        transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), ::tolower);
        if (lowerKey == "content-length") {
            contentLengthStr = pair.second;
            break;
        }
    }
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
    auto it =
        headers.find(name);

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
#include <string>
#include <sstream>
#include <iostream>

#include "http_request.h"

using namespace std;

HttpRequest::HttpRequest(const std::string& rawRequest){
    stringstream requestStream(rawRequest);
    string requestLine;
    getline(requestStream, requestLine);
    stringstream lineStream(requestLine);

    lineStream >> method >> path >> version;

    size_t questionMark = path.find('?');

    if (questionMark != string::npos){
        queryString = path.substr(questionMark + 1);
        path = path.substr(0, questionMark);
        parseQueryParams();
    }

}

string HttpRequest::getMethod() const{
    return method;
}

string HttpRequest::getPath() const{
    return path;
}

string HttpRequest::getVersion() const{
    return version;
}

string HttpRequest::getQueryString() const{
    return queryString;
}

string HttpRequest::getQueryParameter(const string& key) const {
    auto it = queryParams.find(key);
    if (it != queryParams.end()) {
        return it->second;
    }
    return "";
}

void HttpRequest::parseQueryParams() {
    stringstream ss(queryString);
    string pair;
    while (getline(ss, pair, '&')) {
        size_t equalSign = pair.find('=');
        if (equalSign != string::npos) {
            string key = pair.substr(0, equalSign);
            string value = pair.substr(equalSign + 1);
            queryParams[key] = value;
        } else if (!pair.empty()) {
            queryParams[pair] = "";
        }
    }
}
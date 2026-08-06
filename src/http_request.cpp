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

    lineStream >> method;
    lineStream >> path;
    lineStream >> version;
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
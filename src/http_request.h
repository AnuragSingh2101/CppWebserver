#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <string>
using namespace std;
#include <unordered_map>

class HttpRequest{
private:
    string method;
    string path;
    string version;
    string queryString;
    unordered_map<string, string> queryParams;

    void parseQueryParams();
    
    unordered_map<string, string> headers;
    string body;
    void parse(const string& request);

public:
    HttpRequest(const string& rawRequest);
    string getMethod() const;
    string getPath() const;
    string getVersion() const;
    string getQueryString() const;
    string getQueryParameter(const string& key) const;

    string getHeader(const string& name) const;
    string getBody() const;

};

#endif
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

public:
    HttpRequest(const string& rawRequest);
    string getMethod() const;
    string getPath() const;
    string getVersion() const;
    string getQueryString() const;
    string getQueryParameter(const string& key) const;
};

#endif
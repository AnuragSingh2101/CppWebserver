#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <string>
using namespace std;

class HttpRequest{
private:
    string method;
    string path;
    string version;

public:
    HttpRequest(const string& rawRequest);
    string getMethod() const;
    string getPath() const;
    string getVersion() const;
};

#endif
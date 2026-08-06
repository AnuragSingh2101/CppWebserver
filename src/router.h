#ifndef ROUTER_H
#define ROUTER_H

#include <string>
#include <iostream>
using namespace std;

class Router{
public:
    static string getFilePath(const string& path);
    static string getMimeType(const string& filePath);
};

#endif
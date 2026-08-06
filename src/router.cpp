#include "router.h"
#include <string>

using namespace std;

string Router::getFilePath(const string& path){
    if (path == "/" || path == "/index.html"){
    return "public/index.html";
    }

    if (path == "/style.css"){
        return "public/style.css";
    }

    if (path == "/script.js"){
        return "public/script.js";
    }

    return "";
}


string Router::getMimeType(const string& filePath){
    if (filePath.find(".html") != string::npos){
        return "text/html";
    }

    if (filePath.find(".css") != string::npos){
        return "text/css";
    }

    if (filePath.find(".js") != string::npos){
        return "application/javascript";
    }
    return "text/plain";
}
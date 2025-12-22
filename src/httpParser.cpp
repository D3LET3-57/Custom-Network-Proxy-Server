#include <iostream>
#include <string>
#include <sstream>

struct HttpRequest
{
    std::string method;
    std::string host;
    std::string path;
    std::string httpVersion;
    int port;
};

HttpRequest parseHttpRequest(const std::string &request)
{
    HttpRequest httpreq;
    std::istringstream requestStream(request);
    std::string line;

    if (std::getline(requestStream, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        std::istringstream lineStream(line);
        std::string url;
        lineStream >> httpreq.method >> url >> httpreq.httpVersion;

        if (url.substr(0, 7) == "http://")
        {
            url = url.substr(7);
        }
        size_t pathPos = url.find('/');
        if (pathPos != std::string::npos)
        {
            httpreq.host = url.substr(0, pathPos);
            httpreq.path = url.substr(pathPos);
        }
        else
        {
            httpreq.host = url;
            httpreq.path = "/";
        }
    }

    while (std::getline(requestStream, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        if (line.substr(0, 6) == "Host: ")
        {
            if (httpreq.host.empty())
            {
                httpreq.host = line.substr(6);
            }
        }
    }

    return httpreq;
}

int main()
{
    std::string httpRequestStr = "GET http://www.example.com/index.html HTTP/1.1\r\nHost: www.example.com\r\n\r\n";
    HttpRequest request = parseHttpRequest(httpRequestStr);

    std::cout << "Method: " << request.method << std::endl;
    std::cout << "Host: " << request.host << std::endl;
    std::cout << "Path: " << request.path << std::endl;
    std::cout << "HTTP Version: " << request.httpVersion << std::endl;

    return 0;
}
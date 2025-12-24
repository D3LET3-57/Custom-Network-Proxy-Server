#include <iostream>
#include <string>
#include <sstream>

struct HttpRequest
{
    std::string method;
    std::string host;
    std::string path;
    std::string httpVersion;
    std::string ContentType;
    std::string body;
    int ContentLength;
    int port;
};

HttpRequest parseHttpRequest(const std::string &request)
{
    HttpRequest httpreq;
    httpreq.ContentLength = 0;
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
        else if (url.substr(0, 8) == "https://")
        {
            url = url.substr(8);
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

        if (line.empty())
        {
            break;
        }

        if (line.substr(0, 6) == "Host: ")
        {
            if (httpreq.host.empty())
            {
                httpreq.host = line.substr(6);
            }
        }
        else if (line.substr(0, 14) == "Content-Type: ")
        {
            httpreq.ContentType = line.substr(14);
        }
        else if (line.substr(0, 16) == "Content-Length: ")
        {
            httpreq.ContentLength = std::stoi(line.substr(16));
        }
    }

    // Parse Port from Host
    size_t colonPos = httpreq.host.find(':');
    if (colonPos != std::string::npos)
    {
        httpreq.port = std::stoi(httpreq.host.substr(colonPos + 1));
        httpreq.host = httpreq.host.substr(0, colonPos);
    }
    else
    {
        httpreq.port = 80; // Default to 80
    }

    if (httpreq.ContentLength > 0)
    {
        httpreq.body.resize(httpreq.ContentLength);
        requestStream.read(&httpreq.body[0], httpreq.ContentLength);
    }

    return httpreq;
}

int main()
{
    std::string httpRequestStr = R"(POST /create-user HTTP/1.1
Host: www.example.com
Content-Type: application/json
Content-Length: 81

{
  "Id": 78912,
  "Customer": "Jason Sweet",
  "Quantity": 1,
  "Price": 18.00
})";
    HttpRequest request = parseHttpRequest(httpRequestStr);

    std::cout << "Method: " << request.method << std::endl;
    std::cout << "Host: " << request.host << std::endl;
    std::cout << "Path: " << request.path << std::endl;
    std::cout << "HTTP Version: " << request.httpVersion << std::endl;
    std::cout << "Content-Type: " << request.ContentType << std::endl;
    std::cout << "Content-Length: " << request.ContentLength << std::endl;
    std::cout << "Body: " << request.body << std::endl;

    return 0;
}
#ifndef HTTPREQ_H
#define HTTPREQ_H

#include <string>

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

HttpRequest parseHttpRequest(const std::string &request);
bool forwardRequest(int client_socket, const HttpRequest &request);

#endif // HTTPREQ_H

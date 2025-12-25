#include "../include/httpReq.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>

int openConnection(const HttpRequest &request)
{
    int server_socket;
    struct sockaddr_in server_addr;

    // Create a TCP socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0)
    {
        std::cerr << "[-] Socket creation failed\n";
        return -1;
    }

    // Resolve hostname to IP address
    struct hostent *host = gethostbyname(request.host.c_str());
    if (host == nullptr)
    {
        std::cerr << "[-] Hostname resolution failed\n";
        close(server_socket);
        return -1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(request.port > 0 ? request.port : 80);
    memcpy(&server_addr.sin_addr, host->h_addr, host->h_length);

    // Connect to the server
    if (connect(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        std::cerr << "[-] Connection to server failed\n";
        close(server_socket);
        return -1;
    }

    return server_socket;
}

bool forwardRequest(int client_socket, const HttpRequest &request)
{
    int server_socket = openConnection(request);
    if (server_socket < 0)
    {
        return false;
    }

    std::string httpRequestStr = request.method + " " + request.path + " " + request.httpVersion + "\r\n" +
                                 "Host: " + request.host + "\r\n" +
                                 "Content-Type: " + request.ContentType + "\r\n" +
                                 "Content-Length: " + std::to_string(request.ContentLength) + "\r\n" +
                                 "\r\n" +
                                 request.body;

    // Forward the client's request bytes
    size_t totalSent = 0;
    size_t toSend = httpRequestStr.size();
    const char *dataPtr = httpRequestStr.c_str();

    while (totalSent < toSend)
    {
        ssize_t bytesSent = send(server_socket, dataPtr + totalSent, toSend - totalSent, 0);
        if (bytesSent < 0)
        {
            std::cerr << "[-] Sending request to server failed\n";
            close(server_socket);
            return false;
        }
        totalSent += bytesSent;
    }

    // Relay server responses back to the client in a streaming fashion
    char buffer[4096];
    ssize_t bytesRead;
    while ((bytesRead = recv(server_socket, buffer, sizeof(buffer), 0)) > 0)
    {
        ssize_t totalWritten = 0;
        while (totalWritten < bytesRead)
        {
            ssize_t bytesWritten = send(client_socket, buffer + totalWritten, bytesRead - totalWritten, 0);
            if (bytesWritten < 0)
            {
                std::cerr << "[-] Sending response to client failed\n";
                close(server_socket);
                return false;
            }
            totalWritten += bytesWritten;
        }
    }

    if (bytesRead < 0)
    {
        std::cerr << "[-] Receiving response from server failed\n";
        close(server_socket);
        return false;
    }

    close(server_socket);
    return true;
}
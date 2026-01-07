#include "../include/httpReq.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include "../include/filter.h"

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

ForwardResult forwardRequest(int client_socket, const HttpRequest &request)
{
    ForwardResult result = {false, 0, 0, "UNKNOWN"};

    if (isBlocked(request.host))
    {
        std::cerr << "[-] Request to blocked host: " << request.host << "\n";
        result.action = "BLOCKED";
        result.statusCode = 403;

        std::string forbidden = "HTTP/1.1 403 Forbidden\r\nConnection: close\r\n\r\n";
        send(client_socket, forbidden.c_str(), forbidden.size(), 0);

        return result;
    }

    result.action = "ALLOWED";
    int server_socket = openConnection(request);
    if (server_socket < 0)
    {
        result.statusCode = 502;
        return result;
    }

    // Set receive timeout on server socket
    struct timeval timeout;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    setsockopt(server_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    std::string httpRequestStr = request.method + " " + request.path + " " + request.httpVersion + "\r\n" +
                                 "Host: " + request.host + "\r\n" +
                                 "Content-Type: " + request.ContentType + "\r\n" +
                                 "Content-Length: " + std::to_string(request.ContentLength) + "\r\n" +
                                 "Connection: close\r\n" +
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
            result.statusCode = 500;
            return result;
        }
        totalSent += bytesSent;
    }

    // Relay server responses back to the client in a streaming fashion
    char buffer[4096];
    ssize_t bytesRead;
    bool firstChunk = true;

    while ((bytesRead = recv(server_socket, buffer, sizeof(buffer), 0)) > 0)
    {
        if (firstChunk)
        {
            // Try to parse HTTP status code from "HTTP/1.1 XXX "
            if (bytesRead > 12)
            {
                std::string header(buffer, bytesRead);
                size_t firstSpace = header.find(' ');
                if (firstSpace != std::string::npos && firstSpace + 4 < header.length())
                {
                    try
                    {
                        result.statusCode = std::stoi(header.substr(firstSpace + 1, 3));
                    }
                    catch (...)
                    {
                        result.statusCode = 0;
                    }
                }
            }
            firstChunk = false;
        }

        ssize_t totalWritten = 0;
        while (totalWritten < bytesRead)
        {
            ssize_t bytesWritten = send(client_socket, buffer + totalWritten, bytesRead - totalWritten, 0);
            if (bytesWritten < 0)
            {
                std::cerr << "[-] Sending response to client failed\n";
                close(server_socket);
                return result; // Partial success maybe? But failed transmission.
            }
            totalWritten += bytesWritten;
        }
        result.bytesTransferred += bytesRead;
    }

    if (bytesRead < 0)
    {
        std::cerr << "[-] Receiving response from server failed\n";
        close(server_socket);
        // result.success is false by default
        return result;
    }

    close(server_socket);
    result.success = true;
    return result;
}
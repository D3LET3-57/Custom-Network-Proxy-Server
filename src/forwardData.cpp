#include "../include/httpReq.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/select.h>
#include <fcntl.h>
#include <errno.h>
#include "../include/filter.h"

#define TUNNEL_BUFFER_SIZE 8192
#define TUNNEL_TIMEOUT_SEC 60

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

// Set socket to non-blocking mode
static bool setNonBlocking(int socket)
{
    int flags = fcntl(socket, F_GETFL, 0);
    if (flags == -1)
        return false;
    return fcntl(socket, F_SETFL, flags | O_NONBLOCK) != -1;
}

// Handle HTTPS CONNECT tunneling
ForwardResult handleConnect(int client_socket, const HttpRequest &request)
{
    ForwardResult result = {false, 0, 0, "UNKNOWN"};

    // Check if the host is blocked
    if (isBlocked(request.host))
    {
        std::cerr << "[-] CONNECT to blocked host: " << request.host << "\n";
        result.action = "BLOCKED";
        result.statusCode = 403;

        std::string forbidden = "HTTP/1.1 403 Forbidden\r\nConnection: close\r\n\r\n";
        send(client_socket, forbidden.c_str(), forbidden.size(), 0);
        return result;
    }

    result.action = "TUNNEL";

    // Establish connection to the target server
    int server_socket = openConnection(request);
    if (server_socket < 0)
    {
        std::cerr << "[-] Failed to connect to " << request.host << ":" << request.port << "\n";
        result.statusCode = 502;

        std::string badGateway = "HTTP/1.1 502 Bad Gateway\r\nConnection: close\r\n\r\n";
        send(client_socket, badGateway.c_str(), badGateway.size(), 0);
        return result;
    }

    // Send 200 Connection Established response to client
    std::string established = "HTTP/1.1 200 Connection Established\r\n\r\n";
    if (send(client_socket, established.c_str(), established.size(), 0) < 0)
    {
        std::cerr << "[-] Failed to send connection established response\n";
        close(server_socket);
        result.statusCode = 500;
        return result;
    }

    result.statusCode = 200;

    // Set both sockets to non-blocking for bidirectional forwarding
    if (!setNonBlocking(client_socket) || !setNonBlocking(server_socket))
    {
        std::cerr << "[-] Failed to set non-blocking mode\n";
        close(server_socket);
        return result;
    }

    // Bidirectional data forwarding using select()
    char buffer[TUNNEL_BUFFER_SIZE];
    fd_set readfds;
    int maxfd = std::max(client_socket, server_socket) + 1;

    while (true)
    {
        FD_ZERO(&readfds);
        FD_SET(client_socket, &readfds);
        FD_SET(server_socket, &readfds);

        struct timeval timeout;
        timeout.tv_sec = TUNNEL_TIMEOUT_SEC;
        timeout.tv_usec = 0;

        int activity = select(maxfd, &readfds, nullptr, nullptr, &timeout);

        if (activity < 0)
        {
            if (errno == EINTR)
                continue; // Interrupted, retry
            std::cerr << "[-] Select error in tunnel\n";
            break;
        }
        else if (activity == 0)
        {
            // Timeout - no activity for TUNNEL_TIMEOUT_SEC seconds
            std::cerr << "[-] Tunnel timeout\n";
            break;
        }

        // Forward data from client to server
        if (FD_ISSET(client_socket, &readfds))
        {
            ssize_t bytesRead = recv(client_socket, buffer, TUNNEL_BUFFER_SIZE, 0);
            if (bytesRead <= 0)
            {
                if (bytesRead == 0)
                {
                    // Client closed connection
                    break;
                }
                if (errno != EWOULDBLOCK && errno != EAGAIN)
                {
                    break;
                }
            }
            else
            {
                // Send all data to server
                ssize_t totalSent = 0;
                while (totalSent < bytesRead)
                {
                    ssize_t sent = send(server_socket, buffer + totalSent, bytesRead - totalSent, 0);
                    if (sent < 0)
                    {
                        if (errno == EWOULDBLOCK || errno == EAGAIN)
                        {
                            // Would block, wait a bit
                            usleep(1000);
                            continue;
                        }
                        break;
                    }
                    totalSent += sent;
                }
                if (totalSent < bytesRead)
                    break;
                result.bytesTransferred += bytesRead;
            }
        }

        // Forward data from server to client
        if (FD_ISSET(server_socket, &readfds))
        {
            ssize_t bytesRead = recv(server_socket, buffer, TUNNEL_BUFFER_SIZE, 0);
            if (bytesRead <= 0)
            {
                if (bytesRead == 0)
                {
                    // Server closed connection
                    break;
                }
                if (errno != EWOULDBLOCK && errno != EAGAIN)
                {
                    break;
                }
            }
            else
            {
                // Send all data to client
                ssize_t totalSent = 0;
                while (totalSent < bytesRead)
                {
                    ssize_t sent = send(client_socket, buffer + totalSent, bytesRead - totalSent, 0);
                    if (sent < 0)
                    {
                        if (errno == EWOULDBLOCK || errno == EAGAIN)
                        {
                            // Would block, wait a bit
                            usleep(1000);
                            continue;
                        }
                        break;
                    }
                    totalSent += sent;
                }
                if (totalSent < bytesRead)
                    break;
                result.bytesTransferred += bytesRead;
            }
        }
    }

    close(server_socket);
    result.success = true;
    return result;
}
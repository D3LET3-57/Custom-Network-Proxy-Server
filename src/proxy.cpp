#include <iostream>
#include <string>
#include <thread>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "../include/httpReq.h"
#include "../include/filter.h"
#include "../include/logger.h"

#define PORT 8080
#define BUFFER_SIZE 1024
#define BINDADDR "127.0.0.1"
#define BACKLOG 10

void handle_client(int client_socket)
{
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    getpeername(client_socket, (struct sockaddr *)&addr, &len);
    char clientIP[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(addr.sin_addr), clientIP, INET_ADDRSTRLEN);
    int clientPort = ntohs(addr.sin_port);

    // Set a timeout for receiving data
    struct timeval timeout;
    timeout.tv_sec = 5;  // 5 seconds timeout
    timeout.tv_usec = 0; // 0 microseconds

    if (setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout)) < 0)
    {
        std::cerr << "[-] Failed to set socket options\n";
        close(client_socket);
        return;
    }
    std::string totalRequest = "";
    char buffer[BUFFER_SIZE];

    // Read data from the client
    while (true)
    {
        int bytesRead = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytesRead > 0)
        {
            buffer[bytesRead] = '\0';
            totalRequest += std::string(buffer);
            // Check if we have received the end of the HTTP headers
            if (totalRequest.find("\r\n\r\n") != std::string::npos)
            {
                break;
            }
        }
        else if (bytesRead == 0)
        {
            // Connection closed by client
            break;
        }
        else
        {
            // Error or timeout
            if (errno == EWOULDBLOCK || errno == EAGAIN)
            {
                std::cerr << "[-] Receive timeout\n";
            }
            else
            {
                std::cerr << "[-] Receive error\n";
            }
            close(client_socket);
            return;
        }
    }
    std::cout << "[*] Received request:\n"
              << totalRequest << '\n';

    HttpRequest request = parseHttpRequest(totalRequest);
    ForwardResult result = forwardRequest(client_socket, request);

    if (!result.success && result.action == "ALLOWED")
    {
        std::cerr << "[-] Failed to forward request\n";
    }

    std::string requestLine = totalRequest.substr(0, totalRequest.find('\r'));
    if (requestLine.empty())
        requestLine = "UNKNOWN";

    Logger::getInstance().log(clientIP, clientPort,
                              request.host, request.port > 0 ? request.port : 80,
                              requestLine,
                              result.action,
                              result.statusCode,
                              result.bytesTransferred);

    close(client_socket);
}

int main()
{
    // Initialize logger before anything else
    Logger::getInstance();

    loadBlockedHosts();
    int server_socket;
    struct sockaddr_in server_addr;

    // Creating a TCP socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0)
    {
        std::cerr << "[-] Socket creation failed\n";
        return -1;
    }

    // Setting socket options to reuse address and port immediately after program termination
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, BINDADDR, &server_addr.sin_addr);

    // Binding the socket to the specified IP and port
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        std::cerr << "[-] Bind failed\n";
        close(server_socket);
        return -1;
    }

    // Listening for incoming connections
    if (listen(server_socket, BACKLOG) < 0)
    {
        std::cerr << "[-] Listen failed\n";
        close(server_socket);
        return -1;
    }

    std::cout << "[*] Proxy server listening on " << BINDADDR << ":" << PORT << '\n';

    while (true)
    {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        // Accepting incoming client connections
        int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket < 0)
        {
            std::cerr << "[-] Accept failed\n";
            continue;
        }

        // Handle each client connection in a separate thread
        std::thread(handle_client, client_socket).detach();
    }
    close(server_socket);
    return 0;
}
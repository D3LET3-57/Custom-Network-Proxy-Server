#include <iostream>
#include <string>
#include <thread>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define BINDADDR "127.0.0.1"
#define BACKLOG 10

void handle_client(int client_socket)
{
    char buffer[BUFFER_SIZE] = {0};
    int bytes_read = recv(client_socket, buffer, BUFFER_SIZE, 0);

    if (bytes_read > 0)
    {
        buffer[bytes_read] = '\0';
        std::cout << "[*] Received: \n"
                  << buffer << '\n';
    }

    close(client_socket);
}

int main()
{
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
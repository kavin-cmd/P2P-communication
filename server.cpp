#include <iostream>
#include <string>
#include <cstring>
#include <thread>
#include <mutex>
#include <chrono>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 4400
#define MAX_CONNECTIONS 50
#define RETRY_INTERVAL 600 // in seconds (10 minutes)
#define MAX_RETRIES 5

std::vector<int> clients;
std::mutex clients_mutex;

void handle_client(int client_socket) {
    std::cout << "New client connected with socket " << client_socket << std::endl;

    // Add the new client to the clients vector
    clients_mutex.lock();
    clients.push_back(client_socket);
    clients_mutex.unlock();

    // Do some work with the client...
    // For example, receive and send messages using the client socket

    // Remove the client from the clients vector
    clients_mutex.lock();
    clients.erase(std::remove(clients.begin(), clients.end(), client_socket), clients.end());
    clients_mutex.unlock();

    std::cout << "Client with socket " << client_socket << " disconnected" << std::endl;

    close(client_socket);
}
void tcp_server() {
    // Create the TCP socket
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        std::cerr << "Failed to create TCP socket" << std::endl;
        exit(1);
    }

    // Set socket options to reuse the address and port
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        std::cerr << "Failed to set socket options" << std::endl;
        exit(1);
    }

    // Bind the socket to the address and port
    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    if (bind(server_socket, (sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Failed to bind TCP socket" << std::endl;
        exit(1);
    }

    // Listen for client connections
    if (listen(server_socket, MAX_CLIENTS) < 0) {
        std::cerr << "Failed to listen for client connections" << std::endl;
        exit(1);
    }

    while (true) {
        // Accept a client connection
        int client_socket = accept(server_socket, nullptr, nullptr);
        if (client_socket < 0) {
            std::cerr << "Failed to accept client connection" << std::endl;
            continue;
        }

        // Add the client socket to the list of clients
        clients_mutex.lock();
        if (clients.size() < MAX_CLIENTS) {
            clients.push_back(client_socket);
        } else {
            // If there are already MAX_CLIENTS clients, reject the connection
            std::cerr << "Rejecting client connection - too many clients already connected" << std::endl;
            close(client_socket);

            // Allow the client to retry up to MAX_RETRIES times
            int num_retries = 0;
            while (num_retries < MAX_RETRIES) {
                std::this_thread::sleep_for(std::chrono::minutes(1)); // Wait 1 minute before retrying
                client_socket = accept(server_socket, nullptr, nullptr);
                if (client_socket >= 0) {
                    std::cerr << "Accepted retrying client connection" << std::endl;
                    clients.push_back(client_socket);
                    break;
                } else {
                    std::cerr << "Failed to accept retrying client connection" << std::endl;
                    num_retries++;
                }
            }

            if (num_retries == MAX_RETRIES) {
                std::cerr << "Giving up on retrying client connection" << std::endl;
            }
        }
        clients_mutex.unlock();
    }
}

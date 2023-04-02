
#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <mysql/mysql.h>
#include <thread>
#include <vector>
#include <ctime>

using namespace std;

#define PORT 4400
#define MAX_CONNECTIONS 50
#define MAX_TRIES 3
#define TIMEOUT 600 // in seconds
#define BROADCAST_PORT 4401
#define MAX_BROADCAST_MESSAGES 1000
#define MAX_BROADCAST_TIME 1800 // in seconds
#define MAX_USERNAME_LENGTH 20
#define MAX_PASSWORD_LENGTH 20

// server state
struct server_state {
    int tcp_sockfd;
    int udp_sockfd;
    int broadcast_sockfd;
    int num_connections;
    vector<string> active_clients;
    vector<time_t> active_clients_timestamps;
    vector<string> broadcast_messages;
    vector<time_t> broadcast_messages_timestamps;
    MYSQL *db_conn;
    bool is_running;
};

// client state
struct client_state {
    int sockfd;
    string username;
    string ip_address;
};

// function prototypes
void error(const char *msg);
void handle_client(int sockfd, server_state *server);
void handle_broadcast(server_state *server);
void add_active_client(client_state *client, server_state *server);
void remove_active_client(int idx, server_state *server);
void handle_list_active_clients(client_state *client, server_state *server);
bool authenticate_client(client_state *client, server_state *server);
void handle_client_request(client_state *client, server_state *server);
void handle_client_signup(client_state *client, server_state *server);
void handle_client_login(client_state *client, server_state *server);
void handle_client_broadcast(client_state *client, server_state *server);
void broadcast_message(string message, server_state *server);
void add_broadcast_message(string message, server_state *server);
void remove_old_broadcast_messages(server_state *server);

int main(int argc, char *argv[]) {
    // initialize server state
    server_state server;
    server.tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    server.udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    server.broadcast_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    server.num_connections = 0;
    server.db_conn = mysql_init(NULL);
    server.is_running = true;

    // check if socket creation succeeded
    if (server.tcp_sockfd < 0 || server.udp_sockfd < 0 || server.broadcast_sockfd < 0) {
        error("ERROR opening socket");
    }

    // set socket options
    int optval = 1;
    setsockopt(server.tcp_sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));
    setsockopt(server.udp_sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));
    setsockopt(server.broadcast_sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));

    // bind sockets
    struct sockaddr_in tcp_addr, udp_addr, broadcast_addr;
    bzero((char *)&tcp_addr, sizeof(tcp_addr));
    tcp_addr.sin_family = AF_INET;
    tcp_addr.sin_addr.s_addr = INADDR_ANY;
    tcp_addr.sin_port = htons(PORT);
    if (bind(server.tcp_sockfd, (struct sockaddr *)&tcp_addr, sizeof(tcp_addr)) < 0) {
        error("ERROR on binding");
    }
    bzero((char *)&udp_addr, sizeof(udp_addr));
    udp_addr.sin_family = AF_INET;
    udp_addr.sin_addr.s_addr = INADDR_ANY;
    udp_addr.sin_port = htons(PORT);
    if (bind(server.udp_sockfd, (struct sockaddr *)&udp_addr, sizeof(udp_addr)) < 0) {
        error("ERROR on binding");
    }
    bzero((char *)&broadcast_addr, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_addr.s_addr = INADDR_ANY;
    broadcast_addr.sin_port = htons(BROADCAST_PORT);
    if (bind(server.broadcast_sockfd, (struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr)) < 0) {
        error("ERROR on binding");
    }

    // listen for incoming connections
    listen(server.tcp_sockfd, MAX_CONNECTIONS);

    // connect to database
    if (!mysql_real_connect(server.db_conn, "localhost", "root", "", "chat_app", 0, NULL, 0)) {
        error("ERROR connecting to database");
    }

    // start broadcast thread
    thread broadcast_thread(handle_broadcast, &server);

    // accept incoming connections
    while (server.is_running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_sockfd = accept(server.tcp_sockfd, (struct sockaddr *)&client_addr, &client_len);
        if (client_sockfd < 0) {
            error("ERROR on accept");
        }
        if (server.num_connections >= MAX_CONNECTIONS) {
            cout << "Reached maximum number of connections. Please retry later." << endl;
            close(client_sockfd);
            continue;
        }
        client_state *client = new client_state;
        client->sockfd = client_sockfd;
        client->ip_address = inet_ntoa(client_addr.sin_addr);
        thread new_client_thread(handle_client, client_sockfd, &server);
        new_client_thread.detach();
    }

    // clean up and exit
    broadcast_thread.join();
    mysql_close(server.db_conn);
    close(server.tcp_sockfd);
    close(server.udp_sockfd);
    close(server.broadcast_sockfd);
    return 0;
}

void error(const char *msg) {
    perror(msg);
    exit(1);
}

void handle_client(int sockfd, server_state *server) {
    // initialize client state
    client_state client;
    client.sockfd = sockfd;

    // add client to list of active clients
    add_active_client(&client, server);

    // handle client requests
    while (true) {
        // receive client request
        char buffer[256];
        bzero(buffer, 256);
        int n = read(sockfd, buffer, 255);
        if (n < 0) {
            error("ERROR reading from socket");
        }
        if (n == 0) {
            cout << "Connection closed by client " << client.ip_address << endl;
            remove_active_client(-1, server);
            break;
        }

        // handle client request
        client.username = "";
        if (!authenticate_client(&client, server)) {
            cout << "Authentication failed for client " << client.ip_address << endl;
            if (MAX_TRIES > 1) {
                for (int i = 0; i < MAX_TRIES - 1; i++) {
                    char retries_left[10];
                    sprintf(retries_left, "%d", MAX_TRIES - i - 1);
                    string message = "Authentication failed. Please try again. You have " + string(retries_left) + " tries left.";
                    write(sockfd, message.c_str(), message.size());
                    bzero(buffer, 256);
                    n = read(sockfd, buffer, 255);
                    if (n < 0) {
                        error("ERROR reading from socket");
                    }
                    if (n == 0) {
                        cout << "Connection closed by client " << client.ip_address << endl;
                        remove_active_client(-1, server);
                        return;
                    }
                    if (authenticate_client(&client, server)) {
                        break;
                    }
                }
            }
            if (!authenticate_client(&client, server)) {
                cout << "Authentication failed for client " << client.ip_address << endl;
                remove_active_client(-1, server);
                return;
            }
        }
        handle_client_request(&client, server);
    }
}

void handle_broadcast(server_state *server) {
    while (server->is_running) {
        // receive broadcast message
        char buffer[256];
        bzero(buffer, 256);
        struct sockaddr_in sender_addr;
        socklen_t sender_len = sizeof(sender_addr);
        int n = recvfrom(server->broadcast_sockfd, buffer, 255, 0, (struct sockaddr *)&sender_addr, &sender_len);
        if (n < 0) {
            error("ERROR receiving broadcast message");
        }

        // add broadcast message to list
        string sender_ip_address = inet_ntoa(sender_addr.sin_addr);
        string message(buffer);
        message = sender_ip_address + ": " + message;
        add_broadcast_message(message, server);
    }
}

void add_active_client(client_state *client, server_state *server) {
    server->active_clients.push_back(client->username);
    server->active_clients_timestamps.push_back(time(NULL));
    server->num_connections++;
}

void remove_active_client(int idx, server_state *server) {
    if (idx < 0) {
        for (int i = 0; i < server->active_clients.size(); i++) {
            if (server->active_clients[i] == client.username) {
                idx = i;
                break;
            }
        }
    }
    if (idx >= 0) {
        server->active_clients.erase(server->active_clients.begin() + idx);
        server->active_clients_timestamps.erase(server->active_clients_timestamps.begin() + idx);
        server->num_connections--;
    }
}

void handle_list_active_clients(client_state *client, server_state *server) {
    string message ="Active clients:\n";
    for (int i = 0; i < server->active_clients.size(); i++) {
        // only show client username and IP address
        message += server->active_clients[i] + " (" + client->ip_address + ")\n";
    }
    send_message(message.c_str(), client->sockfd);
}

void handle_peer_to_peer(client_state *client, server_state *server, string peer_ip_address) {
    // check if peer IP address is valid
    bool is_valid_peer = false;
    for (int i = 0; i < server->active_clients.size(); i++) {
        if (server->active_clients[i] == peer_ip_address) {
            is_valid_peer = true;
            break;
        }
    }

    // if peer IP address is valid, open peer-to-peer connection
    if (is_valid_peer) {
        int peer_sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (peer_sockfd < 0) {
            error("ERROR opening peer-to-peer socket");
        }

        struct sockaddr_in peer_addr;
        bzero((char *)&peer_addr, sizeof(peer_addr));
        peer_addr.sin_family = AF_INET;
        peer_addr.sin_addr.s_addr = inet_addr(peer_ip_address.c_str());
        peer_addr.sin_port = htons(4400);

        if (connect(peer_sockfd, (struct sockaddr *)&peer_addr, sizeof(peer_addr)) < 0) {
            error("ERROR connecting to peer");
        }

        send_message("Peer-to-peer connection established.", client->sockfd);

        // forward messages between clients
        char buffer[256];
        bzero(buffer, 256);
        int n;
        while (true) {
            n = recv(client->sockfd, buffer, 255, 0);
            if (n < 0) {
                error("ERROR receiving message from client");
            }
            if (n == 0) {
                break;
            }
            send_message(buffer, peer_sockfd);

            bzero(buffer, 256);
            n = recv(peer_sockfd, buffer, 255, 0);
            if (n < 0) {
                error("ERROR receiving message from peer");
            }
            if (n == 0) {
                break;
            }
            send_message(buffer, client->sockfd);
        }

        close(peer_sockfd);
    } else {
        send_message("Invalid peer IP address.", client->sockfd);
    }
}

void handle_broadcast_message(client_state *client, server_state *server) {
    send_message("Enter broadcast message:", client->sockfd);
    char buffer[256];
    bzero(buffer, 256);
    int n = recv(client->sockfd, buffer, 255, 0);
    if (n < 0) {
        error("ERROR receiving broadcast message");
    }
    string message(buffer);
    message = client->username + ": " + message;

    // broadcast message to all clients
    struct sockaddr_in client_addr;
    bzero((char *)&client_addr, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = INADDR_BROADCAST;
    client_addr.sin_port = htons(4400);

    n = sendto(server->broadcast_sockfd, message.c_str(), message.length(), 0, (struct sockaddr *)&client_addr, sizeof(client_addr));
    if (n < 0) {
        error("ERROR broadcasting message");
    }

    // add broadcast message to list
    add_broadcast_message(message, server);

    send_message("Broadcast message sent.", client->sockfd);
}

void add_broadcast_message(string message, server_state *server) {
    time_t current_time = time(NULL);
    server->broadcast_messages.push(make_pair(message, current_time));
    while (!server->broadcast_messages.empty() && difftime(current_time, server->broadcast_messages.front().second) > 1800) {
        server->broadcast_messages.pop();
    }
}

void handle_client_request(client_state *client, server_state *server) {
    send_message("Enter command (list/broadcast/peer):", client->sockfd);
    char buffer[256];
    bzero(buffer, 256);
    int n = recv(client->sockfd, buffer, 255, 0);
    if (n < 0) {
        error("ERROR receiving client request");
    }
    string request(buffer);
    request.erase(remove(request.begin(), request.end(), '\n'), request.end());

    if (request == "list") {
        handle_list_active_clients(client, server);
    } else if (request == "broadcast") {
        handle_broadcast_message(client, server);
    } else if (request == "peer") {
        send_message("Enter peer IP address:", client->sockfd);
        bzero(buffer, 256);
        n = recv(client->sockfd, buffer, 255, 0);
        if (n < 0) {
            error("ERROR receiving peer IP address");
        }
        string peer_ip_address(buffer);
        peer_ip_address.erase(remove(peer_ip_address.begin(), peer_ip_address.end(), '\n'), peer_ip_address.end());
        handle_peer_to_peer(client, server, peer_ip_address);
    } else {
        send_message("Invalid command.", client->sockfd);
    }

    close(client->sockfd);
    remove_active_client(-1, server);
}

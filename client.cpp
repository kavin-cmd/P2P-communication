#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define port 4400

int main()
{
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0)
    {
        std::cerr << "Error creating client socket" << std::endl;
        return 1;
    }

    struct sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    serverAddress.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(clientSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
    {
        std::cerr << "Error connecting to server" << std::endl;
        return 1;
    }

    // Read the initial message from the server
    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    int bytesRead = read(clientSocket, buffer, sizeof(buffer));
    if (bytesRead < 0)
    {
        std::cerr << "Error receiving message" << std::endl;
        return 1;
    }
    std::cout << "Received: " << buffer << std::endl;

    // Enter the message loop
    std::string message;
    while (true)
    {
        std::getline(std::cin, message);
        if (message == "quit")
        {
            break;
        }
        int bytesSent = send(clientSocket, message.c_str(), message.size(), 0);
        if (bytesSent < 0)
        {
            std::cerr << "Error sending message" << std::endl;
            return 1;
        }
        memset(buffer, 0, sizeof(buffer));
        bytesRead = read(clientSocket, buffer, sizeof(buffer));
        if (bytesRead < 0)
        {
            std::cerr << "Error receiving message" << std::endl;
            return 1;
        }
        std::cout << "Received: " << buffer << std::endl;
    }
    close(clientSocket);
    return 0;
}
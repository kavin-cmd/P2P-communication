Chat App Server

The Chat App Server is a TCP/UDP server that provides a chat service to clients. Clients can connect to the server, authenticate themselves, and then send and receive messages to and from other clients. The server also supports broadcast messages that can be sent to all connected clients.

The Chat App Server is written in C++ and uses the following libraries:

iostream for input/output

string and cstring for strings and character arrays

cstdlib for standard library functions

cstdio for C-style input/output

sys/types.h, sys/socket.h, netinet/in.h, arpa/inet.h, and unistd.h for socket programming

mysql/mysql.h for database connectivity

thread and vector for multi-threading and data structures

ctime for time-related functions

Installation

To compile and run the Chat App Server, you need to have a C++ compiler and the above libraries installed on your system.

Clone the repository: git clone https://github.com/kavin-cmd/P2P-communication.git

Navigate to the project directory: cd P2P-communication

Compile the server: g++ server.cpp -o server -lmysqlclient -lpthread

Start the server: ./server

Usage

The Chat App Server listens on TCP port 4400 and UDP port 4400 for client connections, and UDP port 4401 for broadcast messages.

When a client connects to the server, the server prompts the client to enter a username and password. The server then checks the username and password against the database, and if they match, adds the client to the list of active clients. The client can then send and receive messages to and from other clients, or send broadcast messages to all connected clients.

Clients can also request a list of all active clients by sending the message "list" to the server.

The server keeps track of the last 1000 broadcast messages sent, and deletes messages that are older than 30 minutes.

Database

The Chat App Server uses a MySQL database to store client usernames and passwords. The database should have a users table with the following schema:

CREATE TABLE users (
  id INT(11) NOT NULL AUTO_INCREMENT,
  username VARCHAR(20) NOT NULL,
  password VARCHAR(20) NOT NULL,
  PRIMARY KEY (id),
  UNIQUE KEY username (username)
);

The server expects the database to be running on localhost with the username root and no password, and the database name chat_app. You can modify these settings in the main function of server.cpp.

License

The Chat App Server is licensed under the MIT License. See LICENSE for more information.

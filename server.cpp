#include<iostream>
#include<cstring>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<thread>
#include<vector>
#include<mysql/mysql.h>
#include<string>
#include<unordered_map>
#include<algorithm>
using namespace std;

#define max_connections 50
#define port 4400
MYSQL *conn = mysql_init(NULL);
MYSQL_RES *res = NULL;
MYSQL_ROW row;

void sendInitialMenu(int clientSocket)
{
    const char* menumessage = "please choose an option:\n Login \n Signup";
    send(clientSocket, menumessage, strlen(menumessage), 0);
}

void tcpHandler(int clientSocket)
{

    sendInitialMenu(clientSocket);
    char buffer[2048];
    while (true)
    {
        memset(buffer, 0, sizeof(buffer));
        int bytesRead = read(clientSocket, buffer, sizeof(buffer));
        if (bytesRead <= 0)
        {
            break;
        }
        std::cout << "Received: " << buffer << std::endl;
        write(clientSocket, buffer, bytesRead);
        std::string message = buffer;
        if(message == "login" || message == "Login")
        {
            std::string username, password;
            write(clientSocket, "Enter username:", strlen("Enter username:"));
            memset(buffer, 0, sizeof(buffer));
            bytesRead = read(clientSocket, buffer, sizeof(buffer));
            username = std::string(buffer);
            username.erase(std::remove_if(username.begin(), username.end(), [](char c){ return c == '\n'; }), username.end());
            write(clientSocket, "Enter password:", strlen("Enter password:"));
            memset(buffer, 0, sizeof(buffer));
            bytesRead = read(clientSocket, buffer, sizeof(buffer));
            password = std::string(buffer);
            password.erase(std::remove(password.begin(), password.end(), '\n'), password.end());

            //check if the username and password match
            std::string query = "SELECT * FROM clients WHERE username='" + username + "' AND password='" + password + "'";
            if(mysql_query(conn, query.c_str()))
            {
                std::cerr<<"error querying mysql database:"<<mysql_error(conn)<<std::endl;
                write(clientSocket, "Error querying database", strlen("Error querying database"));
                continue;
            }
            MYSQL_RES *result = mysql_store_result(conn);
            if(result == NULL)
            {
                std::cerr<<"error querying mysql query results:"<<mysql_error(conn)<<std::endl;
                write(clientSocket, "Error querying database", strlen("Error querying database"));
                continue;
            }
            int num_rows = mysql_num_rows(result);
            mysql_free_result(result);
            if(num_rows == 1)
            {
                write(clientSocket, "Login Successful \n", strlen("Login successful \n"));     
            }
            else 
            {
                write(clientSocket, "Invalid username or password \n", strlen("Invalid username or password"));
            }
        }
        else if (message == "signup" || message == "Signup") 
        {
            std::string username, password, confirm_password;
            write(clientSocket, "Enter username", strlen("Enter username"));
            cout<<"1"<<endl;
            memset(buffer, 0, sizeof(buffer));
            bytesRead = read(clientSocket, buffer, sizeof(buffer));
            cout<<"2"<<endl;
            username = std::string(buffer);
            cout<<username<<endl;
            username.erase(std::remove(username.begin(), username.end(), '\n'), username.end());
            std::cout << "Username received: " << username << std::endl;
            write(clientSocket, "Enter password", strlen("Enter password"));
            cout<<"3"<<endl;
            memset(buffer, 0, sizeof(buffer));
            bytesRead = read(clientSocket, buffer, sizeof(buffer));
            cout<<"4"<<endl;
            password = std::string(buffer);
            cout<<password<<endl;
            password.erase(std::remove(password.begin(), password.end(), '\n'), password.end());
            std::cout << "Password received: " << password << std::endl;
            write(clientSocket, "Confirm password", strlen("Confirm password"));
            cout<<"5"<<endl;
            memset(buffer, 0, sizeof(buffer));
            bytesRead = read(clientSocket, buffer, sizeof(buffer));
            cout<<"6"<<endl;
            confirm_password = std::string(buffer);
            cout<<confirm_password<<endl;
            confirm_password.erase(std::remove(confirm_password.begin(), confirm_password.end(), '\n'), confirm_password.end());
            std::cout << "confirm password received: " << confirm_password << std::endl;
            //check if the password and the confirm password match
            if(password != confirm_password)
            {
                write(clientSocket, "Invalid password", strlen("Invalid password"));
                //works
            }
            else
            {
                //check if the username is already taken
                cout<<"7"<<endl;
                std::string query = "SELECT * FROM clients WHERE username='" + username + "' AND password='" + password + "'";
                mysql_query(conn, query.c_str());
                res = mysql_store_result(conn);
                cout<<res<<endl;
                // if(res == NULL)
                // {
                //     write(clientSocket, "Error querying database", strlen("Error querying database"));
                //     cout<<"7_1"<<endl;
                //     continue;
                // }
                int num_rows_1 = mysql_num_rows(res);
                mysql_free_result(res);
                cout<<num_rows_1<<endl;
                cout<<"8"<<endl;
                if(num_rows_1 == 0)
                {
                    //username is not taken
                    std::string query = "INSERT INTO users (username, password) VALUES ('" + username + "', '" + password + "')";
                    cout<<"8.1"<<endl;
                    if(mysql_query(conn, query.c_str()))
                    {
                        write(clientSocket, "Error inserting into database", strlen("Error inserting into database"));
                        cout<<"8.2"<<endl;
                    }
                    else
                    {
                        write(clientSocket, "Successfully created user", strlen("Successfully created user"));
                        cout<<"8.3"<<endl;
                    }
                }
                else
                {
                    write(clientSocket, "Username already taken", strlen("Username already taken"));
                    cout<<"8.4"<<endl;
                }
            }
        }  
    }
    close(clientSocket);
}

void udpHandler(int udpSocket)
{
    struct sockaddr_in broadcastAddress;
    memset(&broadcastAddress, 0, sizeof(broadcastAddress));
    broadcastAddress.sin_family = AF_INET;
    broadcastAddress.sin_port = htons(port+1);
    broadcastAddress.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    while (true)
    {
        std::string message;
        std::cin >> message;
        if (message.length() < 0)
        {
            break;
        }
        int bytesSent = sendto(udpSocket, message.c_str(), message.size(), 0, (struct sockaddr *)&broadcastAddress, sizeof(broadcastAddress));
    }
    close(udpSocket);
}
int main()
{
    int tcpSocket, udpSocket;
    struct sockaddr_in tcpAddress, udpAddress;

        // create a MySQL connection
    MYSQL *conn;
    conn = mysql_init(NULL);
    if (conn == NULL)
    {
        std::cerr << "Error initializing MySQL connection" << std::endl;
        return 1;
    }
    // connect to MySQL server 
    if (!mysql_real_connect(conn, "127.0.0.1", "root", "root", NULL, 0, NULL, 0))
    {
        std::cerr << "Error connecting to MySQL server" << std::endl;
        return 1;
    }
    else
    {
        std::cout << "Connected to MySQL server successfully" << std::endl;
    }

    // create a database
    if (mysql_query(conn, "CREATE DATABASE IF NOT EXISTS my_database"))
    {
        std::cerr << "Error creating database: " << mysql_error(conn) << std::endl;
        return 1;
    }

    // select the database
    if (mysql_select_db(conn, "my_database"))
    {
        std::cerr << "Error selecting database: " << mysql_error(conn) << std::endl;
        return 1;
    }

    // create a table to store client authentication information
    if (mysql_query(conn, "CREATE TABLE IF NOT EXISTS clients (username VARCHAR(255), password VARCHAR(255))"))
    {
        std::cerr << "Error creating table: " << mysql_error(conn) << std::endl;
        return 1;
    }

    //create a tcp socket
    tcpSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (tcpSocket < 0)
    {
        std::cerr << "Error creating tcp socket" << std::endl;
        return 1;
    }

    //set tcp socket options
    int opt = 1;
    if(setsockopt(tcpSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))<0)
    {
        std::cerr << "Error setting tcp socket options" << std::endl;
        return 1;
    }

    //bind tcp socket to port 4400
   memset(&tcpAddress, 0, sizeof(tcpAddress));
    tcpAddress.sin_family = AF_INET;
    tcpAddress.sin_addr.s_addr = INADDR_ANY;
    tcpAddress.sin_port = htons(port);
    if (bind(tcpSocket, (struct sockaddr *)&tcpAddress, sizeof(tcpAddress)) < 0)
    {
        std::cerr << "Error binding tcp socket: " << strerror(errno) << std::endl;
        close(tcpSocket);
        return 1;
    }
    //listen for incoming tcp connections
    if (listen(tcpSocket, 50) < 0)
    {
        std::cerr << "Error listening for tcp connections" << std::endl;
        return 1;
    }

    //create a udp socket
    udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpSocket < 0)
    {
        std::cerr << "Error creating udp socket" << std::endl;
        return 1;
    }

    //set udp socket options
    opt = 1;
    if(setsockopt(udpSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))<0)
    {
        std::cerr << "Error setting udp socket options" << std::endl;
        return 1;
    }

    //bind udp socket to port 4401
    memset(&udpAddress, 0, sizeof(udpAddress));
    udpAddress.sin_family = AF_INET;
    udpAddress.sin_addr.s_addr = INADDR_ANY;
    udpAddress.sin_port = htons(port+1);
    if (bind(udpSocket, (struct sockaddr *)&udpAddress, sizeof(udpAddress)) < 0)
    {
        std::cerr << "Error binding udp socket" << std::endl;
        return 1;
    }

    // listen for incoming udp connections
    // if (listen(udpSocket, 50) < 0)
    // {
    //     std::cerr << "Error listening for udp connections" << std::endl;
    //     return 1;
    // }

    // //create a signal handler
    // signal(SIGINT, signal_handler);

    //create a thread to listen for tcp connections
    std::vector<std::thread >tcpThreads;
    while(true)
    {
        if(tcpThreads.size()>=max_connections)
        {
            //Ignore new tcp connections if max_connections reached
            std::this_thread::sleep_for(std::chrono::minutes(10));
            continue;
        }
        struct sockaddr_in clientAddress;
        socklen_t clientAddressSize = sizeof(clientAddress);
        int clientSocket = accept(tcpSocket, (struct sockaddr*)&clientAddress, &clientAddressSize);
        if (clientSocket < 0)
        {
            std::cerr << "Error accepting tcp connection" << std::endl;
            return 1;
        }
        std::thread tcpThread(tcpHandler, clientSocket);
        tcpThreads.push_back(std::move(tcpThread));
    }

//start udp handler thread
std::thread udpThread(udpHandler, udpSocket);

//wait for all threads to finish
for(auto& thread : tcpThreads)
{
    thread.join();
}
return 0;
}
# Acknowledgement

The file 'collections/list.h' was obtained from the Linux source code, because I think it is evry convenient to use. Also, I made some modification to make it more suitable to the project. Except the file 'collections/list.h', I wrote the whole project (So sorry that I didn't use the template code from the lab).


# About the project

The project implements a simple chat system based on the TCP protocol. It contains a server and client application. The file name of the server application is IMServer (after being compiled), and the file name of the client application is IMClient. The applications only can be run on Linux. Because macOS doesn't provide the header <errno.h> I used, and I haven't tested it on windows. For now, I have tested the application on the virtual machine from the lab, my raspberry pi (Raspberry Pi OS, base on Debian 32-bit), and my personal computer (Fedora Linux 40 64-bit).

The applications don't print to much information on the terminal, but I did write a lot of logs in the program for debugging. If you want to check the log, just uncomment the defination of __DEBUG__ in common.h and rebuild the program.


# Build

Instructions to build the project:
1. Make sure the command gcc in this directory: /usr/bin/, if not, modify the first line of Makefile to change it;
2. To compile the server, use: `make server`;
3. To compile the client, use: `make client`;
4. To compile both, use: `make all`;
5. To clean up, use: `make clean`;


# Run

Instructions to run:
1. To run the server, use: ./IMServer ip port
    I. The {ip} is the ip address that the server listen on;
    II. If you want to accept clients from any host, use '*' to replace the ip (don't forgot the '', or the shell will expand the * to the files in this directory);
    III. The {port} is the port that the server listen on.
2. To run the client, use: ./IMClient ip port user_name
    I. The {ip} is server's ip address;
    II. Same as the {port}, the port of the server;
    III. The {user_name} is the user name that you want to login the IM system (so others can recognise you and send peer message to you).
3. To leave the client or server, use: `ctrl + c`


# Implementation
The client application is quite simple. After connect to the server, uses `poll` to observe if there is data in the standrad inout and the socket. If so, reads the data from standard input, and constructs a message then writes the message to the socket. If there is data in the socket, just reads it and output to the stanrdard output.

The server application is much more complicated. After create the listening socket, creates a new thread to accept all the new connections, wrap the socket with a ClientInfo structure, and then use a list to organise all the structures. If the reading and writing thread don't exist, creates them. 

The reading thread in the server application uses `poll` to observe all the sockets from client. Then read the data from sockets and store the data in the corresponding ClientInfo's reading buffer. After each round of reading, parse the newly read data, check if the data in the buffer contains a whole message. If so, reads the whole message from the reading buffer, checks the type of the message, then rebuilds the message based on its type and write it to related clients's writing buffer. At last, notify the writing thread to flush the messages. In addition, if there is some error in specified socket, marks the status on the ClientInfo, then notifies the writing thread to log it out.

The writing thread in the server application uses a loop to check all the ClientInfos if there are some messages in its writing buffer, if so, writing the data in its writing buffer into corresponding socket. If not, waits for the singal from reading thread to repeat the next round of checking. In addition, if the status of any ClientInfo is abnormal, removes it from list, and sends a message to all other clients that it has logged out.


# Comments on the source code
IMClient.c:     The entry code of the client application.
IMServer.c:     The entry code of the server application.
Makefile:       The makefile to compile the project.
io/cache.c:     The module to reuse the allocated memory.
io/buffer.c:    A module that can use multiple small chunks of memory to store big chunk of data.
net_util.c:     Some handy functions used to construct ip address or obtain information from socket.
entity/message.c:   A structure which wraps the data received or sent to client.
entity/client_info.c    A structure which wraps the socket and name, also provides buffer to read and write messages.

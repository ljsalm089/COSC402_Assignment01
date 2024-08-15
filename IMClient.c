#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/param.h>

#include <pthread.h>

#include "common.h"
#include "collections/list.h"

#define TAG "IMClient"

typedef struct sockaddr ip;
typedef struct sockaddr_in ipv4;
typedef struct sockaddr_in6 ipv6;

int main(int argc, char **argv)
{
    if (argc < 4) 
    {
        err_sys(TAG, "usage: %s {server address} {aserver port} {client name}");
    }
    int sever_port = atoi(argv[2]);
    D(TAG, "Server ip address is %s", argv[1]);
    D(TAG, "Server port is %d", sever_port);
    D(TAG, "Client name: %s", argv[3]);

    ip * server_addr;
    int ip_size;

    struct in_addr addr4;
    struct in6_addr addr6;
    if (inet_pton(AF_INET, argv[1], &addr4) == 1)
    {
        D(TAG, "%s is an ipv4 address", argv[1]);
        ip_size = sizeof(ipv4);
        ipv4 * addr = (ipv4 *) malloc(ip_size);
        bzero(addr, ip_size);
        addr->sin_port = htons(sever_port);
        addr->sin_addr = addr4;
        addr->sin_family = AF_INET;
        server_addr = (struct sockaddr *) addr;
    }
    else if (inet_pton(AF_INET6, argv[1], &addr6) == 1) 
    {
        D(TAG, "%s is an ipv6 address", argv[1]);
        ip_size = sizeof(ipv6);
        struct sockaddr_in6 * addr = (ipv6 *) malloc(ip_size);
        bzero(addr, ip_size);
        addr->sin6_addr = addr6;
        addr->sin6_port = htons(sever_port);
        addr->sin6_family = AF_INET6;
        server_addr = (struct sockaddr *) addr;
    }
    else 
    {
        err_sys(TAG, "Invalid server address: %s\0", argv[1]);
    }

    // create socket
    int s_fd = socket(sizeof(ipv4) == ip_size ? AF_INET : AF_INET6, SOCK_STREAM, 0);
    if (s_fd < 0) 
    {
        err_sys(TAG, "Unable to create socket");
    }

    // connect to server
    if (connect(s_fd, server_addr, ip_size) < 0) 
    {
        close(s_fd);
        err_sys(TAG, "Unable to connect to server");
    }

    char * name_line = malloc(strlen(argv[3]) + 2);
    sprintf(name_line, "%s\n", argv[3]);
    ssize_t size_send = send(s_fd, name_line, strlen(argv[3]) + 1, 0);
    

    return 0;
}

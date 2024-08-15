#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/param.h>
#include <string.h>

#include <pthread.h>

#include "common.h"
#include "collections/list.h"

#define TAG "IMServer"
#define S_NAME "Server"

#define TYPE_C 1
#define TYPE_S 2

typedef struct sockaddr ip;
typedef struct sockaddr_in ipv4;
typedef struct sockaddr_in6 ipv6;

typedef struct client_info 
{
    struct list_head node;
    int socket_fp;
    char client_name[30];
    time_t login_time;
    unsigned int type;
} * Client_Info;

static pthread_mutex_t global_lock = PTHREAD_MUTEX_INITIALIZER;
static Client_Info client_list;

int main(int argc, char** argv)
{
    I(TAG, "Starting IM server!");
    if (argc < 3) 
    {
        err_sys(TAG, "usage: %s {server address} {aserver port}");
    }
    int sever_port = atoi(argv[2]);
    D(TAG, "Server ip address is %s", argv[1]);
    D(TAG, "Server port is %d", sever_port);

    ip * server_addr;
    int ip_size;

    struct in_addr addr4;
    struct in6_addr addr6;
    if (!strcmp("*", argv[1]) || inet_pton(AF_INET, argv[1], &addr4) == 1)
    {
        D(TAG, "%s is an ipv4 address", argv[1]);
        ipv4 * addr = (ipv4 *) malloc(ip_size);
        if (!strcmp("*", argv[1]))
            addr->sin_addr.s_addr = htonl(INADDR_ANY);
        else
        {
            ip_size = sizeof(ipv4);
            bzero(addr, ip_size);
            addr->sin_addr = addr4;
        }
        addr->sin_port = htons(sever_port);
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
    int listen_fd = socket(ip_size == sizeof(ipv4) ? AF_INET : AF_INET6, SOCK_STREAM, 0);
    if (listen_fd < 0) 
    {
        err_sys(TAG, "unable to create socket.");
    }
    D(TAG, "create socket succefully.");

    int ret = bind(listen_fd, (struct sockaddr *)server_addr, ip_size);
    if (ret < 0) 
    {
        err_sys(TAG, "unable to bind the socket to specified address and port.");
    }
    D(TAG, "bind socket to specified address and port successfully.");

    // listen and waiting for connect
    ret = listen(listen_fd, 150);
    if (ret < 0) 
    {
        close(listen_fd);
        err_sys(TAG, "unable to listen in specified port.");
    }
    D(TAG, "listen to specified port successfully.");

    client_list = (Client_Info) malloc(sizeof(struct client_info));
    INIT_LIST_HEAD(&client_list->node);
    client_list->socket_fp = listen_fd;
    strcpy(client_list->client_name, S_NAME);
    time(&(client_list->login_time));
    client_list->type = TYPE_S;
    D(TAG, "Initialise the server info successfully.");

    socklen_t conn_addr_size = MAX(sizeof(struct sockaddr_in), sizeof(struct sockaddr_in6));
    size_t addr_max_size = conn_addr_size;
    void * conn_addr = malloc (addr_max_size);
    char ip_address[INET6_ADDRSTRLEN + 6];

    for(;;)
    {
        memset(conn_addr, 0, addr_max_size);
        int client_fd = accept(listen_fd, conn_addr, &conn_addr_size);
        if (client_fd < 0) 
        {
            E(TAG, "unable to accept connection from the client.");
            continue;
        }
        D(TAG, "received an new connection.");

        sa_family_t family = * (sa_family_t *) conn_addr;
        if (AF_INET == family || AF_INET6 == family) 
        {
            if (inet_ntop(family, conn_addr, ip_address, 
                    AF_INET == family ? INET_ADDRSTRLEN : INET6_ADDRSTRLEN) == NULL) 
            {
                E(TAG, "unable to convert to the ipv4 or ipv6 address");
                close(client_fd);
                continue;
            }
            // for debugging, just print out the remote address and port of the socket
            const char * addr_str;
            int port;
            if (AF_INET == family) 
            {
                ipv4 * client_ip = (ipv4 *) conn_addr;
                addr_str = inet_ntop(family, &client_ip->sin_addr, ip_address, INET6_ADDRSTRLEN + 6);
                port = ntohs(client_ip->sin_port);
            }
            else 
            {
                ipv6 * client_ip = (ipv6 *) conn_addr;
                addr_str = inet_ntop(family, &client_ip->sin6_addr, ip_address, INET6_ADDRSTRLEN + 6);
                port = ntohs(client_ip->sin6_port);
            }
            D(TAG, "the remote ip and port of the connecttion is: %s:%d", addr_str, port);
        }
        else 
        {
            E(TAG, "unknown the address type of this connection, just ignore it.");
            close(client_fd);
            continue;
        }
    }

    // relase the socket
    close(listen_fd);

    return 0;
}

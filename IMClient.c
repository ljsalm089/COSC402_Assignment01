#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/param.h>
#include <poll.h>
#include <errno.h>

#include <pthread.h>

#include "common.h"
#include "collections/list.h"
#include "entity/message.h"

#define TAG "IMClient"

#define CACHE_SIZE 1024

typedef struct sockaddr ip;
typedef struct sockaddr_in ipv4;
typedef struct sockaddr_in6 ipv6;

int main(int argc, char **argv)
{
    if (argc < 4) {
        err_sys(TAG, "usage: %s {server address} {aserver port} {client name}");
    }
    int sever_port = atoi(argv[2]);
    D(TAG, "Server ip address is %s", argv[1]);
    D(TAG, "Server port is %d", sever_port);
    D(TAG, "Client name: %s", argv[3]);

    ip * server_addr;
    int ip_size;

    int ret = 0;
    extern int errno;

    struct in_addr addr4;
    struct in6_addr addr6;
    if (inet_pton(AF_INET, argv[1], &addr4) == 1){
        D(TAG, "%s is an ipv4 address", argv[1]);
        ip_size = sizeof(ipv4);
        ipv4 * addr = (ipv4 *) malloc(ip_size);
        bzero(addr, ip_size);
        addr->sin_port = htons(sever_port);
        addr->sin_addr = addr4;
        addr->sin_family = AF_INET;
        server_addr = (struct sockaddr *) addr;
    } else if (inet_pton(AF_INET6, argv[1], &addr6) == 1) {
        D(TAG, "%s is an ipv6 address", argv[1]);
        ip_size = sizeof(ipv6);
        struct sockaddr_in6 * addr = (ipv6 *) malloc(ip_size);
        bzero(addr, ip_size);
        addr->sin6_addr = addr6;
        addr->sin6_port = htons(sever_port);
        addr->sin6_family = AF_INET6;
        server_addr = (struct sockaddr *) addr;
    } else {
        err_sys(TAG, "Invalid server address: %s\0", argv[1]);
    }

    // create socket
    int sock_id = socket(sizeof(ipv4) == ip_size ? AF_INET : AF_INET6, SOCK_STREAM, 0);
    if (sock_id < 0) {
        err_sys(TAG, "Unable to create socket: %s", strerror(errno));
    }

    // connect to server
    ret = connect(sock_id, server_addr, ip_size);
    if (ret < 0) {
        E(TAG, "Unable to connect to server: %s", strerror(errno));
        goto main_socket_error;
    }

    size_t login_cmd_size = strlen(argv[3]) + strlen(CMD_LOGIN) + 1;
    char * name_line = malloc(login_cmd_size);
    if (NULL == name_line) {
        E(TAG, "Unable to allocate memory for registration");
        goto main_socket_error;
    }

    memset(name_line, 0, login_cmd_size);
    sprintf(name_line, CMD_LOGIN "%s\n", argv[3]);
    ssize_t send_size = send(sock_id, name_line, login_cmd_size, 0);
    if (send_size != login_cmd_size) {
        E(TAG, "Unable to send client name to the server to register");
        goto main_register_error;
    }
    D(TAG, "Write %d bytes into socket(%d)", send_size, sock_id);
    free(name_line);
    name_line = NULL;

    struct pollfd * p_poll_ids = (struct pollfd *) malloc(2 * sizeof(struct pollfd));
    nfds_t poll_size = 2;
    if (NULL == p_poll_ids) {
        E(TAG, "Unable to allocate memory for poll id list");
        goto main_register_error;
    }
    memset(p_poll_ids, 0, 2 * sizeof(struct pollfd));
    p_poll_ids[0].fd = 0;
    p_poll_ids[1].fd = sock_id;
    p_poll_ids[1].events = p_poll_ids[0].events = POLLIN;

    int is_online = 1;
    int read_size = 0;
    int write_size = 0;

    char * buff = malloc(CACHE_SIZE * sizeof(char));
    if (NULL == buff) {
        E(TAG, "Unable to allocate cache for reading: %s", strerror(errno));
        goto main_register_error;
    }

    while (is_online) {
        ret = poll(p_poll_ids, poll_size, -1);
        if (-1 == ret) {
            E(TAG, "Unable to poll: %s", strerror(errno));
            break;
        }

        int cmd_len = strlen(CMD_TO_ALL);
        for (int index = 0; index < poll_size; index++) {
            struct pollfd * p_fd = p_poll_ids + index;
            memset(buff, 0, CACHE_SIZE);

            if (0 == p_fd->fd && (p_fd->revents & POLLIN)) {
                // read data from the standard input and handle it
                read_size = read(p_fd->fd, buff + cmd_len, CACHE_SIZE - cmd_len);
                strncpy(buff, CMD_TO_ALL, cmd_len);
                D(TAG, "Read %d bytes data from stdin: %s", read_size, buff);
                write_size = write(sock_id, buff, strlen(buff));
                if (-1 == write_size) {
                    E(TAG, "Fail to write data to socket: %s", strerror(errno));
                    is_online = 0;
                } else {
                    D(TAG, "Write %d bytes to socket", write_size);
                }
            } else if (sock_id == p_fd->fd && (p_fd->revents & POLLIN)) {
                // read data from socket and handle it
                read_size = read(p_fd->fd, buff, CACHE_SIZE);
                if (-1 == read_size) {
                    // read data from socket error
                    E(TAG, "Fail to read data from socket: %s", strerror(errno));
                    continue;
                } else if (0 == read_size) {
                    is_online = 0;
                    I(TAG, "Abnormal socket status, exiting...");
                    break;
                } else {
                    D(TAG, "Read %d bytes data from socket: %d", read_size, p_fd->fd);
                }
                // just write the data from buff to standard output
                write_size = write(1, buff, read_size);
            } else if (sock_id == p_fd->fd && p_fd->revents != 0) {
                // POLLERR | POLLHUP
                D(TAG, "Current event: %d, error event: %d, %d", p_fd->revents, POLLERR, POLLHUP);
                is_online = 0;
                I(TAG, "Some error in the socket, just exit");
            }
        }
    }
    free(buff);

main_register_error:
    if (NULL != name_line) free(name_line);

main_socket_error:
    close(sock_id);
    return 0;
}

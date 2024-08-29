# include <time.h>
# include <unistd.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <sys/param.h>
# include <poll.h>
# include <errno.h>
 
# include <pthread.h>

# include "common.h"
# include "collections/list.h"
# include "entity/message.h"
# include "net_util.h"

#define TAG "IMClient"

#define CACHE_SIZE 1024

int main(int argc, char **argv)
{
    if (argc < 4) {
        err_sys(TAG, "usage: %s {server address} {aserver port} {client name}", argv[0]);
    }
    int sever_port = atoi(argv[2]);
    D(TAG, "Server ip address is %s", argv[1]);
    D(TAG, "Server port is %d", sever_port);
    D(TAG, "Client name: %s", argv[3]);

    extern int errno;

    PIP server_addr;
    int ip_size;

    int ret = 0;
    if ((ret = get_addr_info_from_ip_and_port(argv[1], argv[2], &server_addr, 
                    &ip_size)) != 0) {
        E(TAG, "Unable to parse server address or port");
        return -1;
    }

    int s_socket = socket(server_addr->sa_family, SOCK_STREAM, 0);
    if (s_socket < 0) {
        err_sys(TAG, "Unable to create socket.");
    }
    D(TAG, "create socket successfully");

    // connect to server
    ret = connect(s_socket, server_addr, ip_size);
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
    // LOGIN: name\n
    sprintf(name_line, CMD_LOGIN "%s\n", argv[3]);
    ssize_t send_size = send(s_socket, name_line, login_cmd_size, 0);
    if (send_size != login_cmd_size) {
        E(TAG, "Unable to send client name to the server to register");
        goto main_register_error;
    }
    D(TAG, "Write %d bytes into socket(%d)", send_size, s_socket);
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
    p_poll_ids[1].fd = s_socket;
    p_poll_ids[1].events = p_poll_ids[0].events = POLLIN;

    int is_online = 1;
    int read_size = 0;
    int write_size = 0;

    char * buff = malloc(CACHE_SIZE * sizeof(char));
    if (NULL == buff) {
        E(TAG, "Unable to allocate cache for reading: %s", strerror(errno));
        goto main_register_error;
    }

    printf("You are online now!\nPlease input your message and press 'enter' to send:\n");

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
                char * start_read_pos = buff + cmd_len + 1;
                read_size = read(p_fd->fd, start_read_pos, CACHE_SIZE - cmd_len - 1);

                char * write_pos = buff;
                char * indicator_pos = strstr(start_read_pos, MSG_PREFIX_INDICATOR " ");
                if (start_read_pos == strstr(start_read_pos, CMD_TO_C) 
                        && NULL != indicator_pos) {
                    // contains to c message prefix TO-C- and indicator :
                    if (indicator_pos - start_read_pos <
                            strlen(CMD_TO_C) + CLIENT_NAME_MAXIMUM_SIZE) {
                        // valid user name, just send the reading part to socket
                        write_pos = start_read_pos;
                    }
                }

                if (write_pos == buff) {
                    // need to add TO_ALL: prefix to the buffer
                    strncpy(buff, CMD_TO_ALL " ", cmd_len + 1);
                    read_size += cmd_len + 1;
                }
                D(TAG, "Read %d bytes data from stdin: %s", read_size, write_pos);

                write_size = write(s_socket, write_pos, read_size);
                if (-1 == write_size) {
                    E(TAG, "Fail to write data to socket: %s", strerror(errno));
                    is_online = 0;
                } else {
                    D(TAG, "Write %d bytes to socket", write_size);
                }

                printf("Please input your message and press 'enter' to send:\n");
            } else if (s_socket == p_fd->fd && (p_fd->revents & POLLIN)) {
                // read data from socket and handle it
                read_size = read(p_fd->fd, buff, CACHE_SIZE);
                if (-1 == read_size) {
                    // read data from socket error
                    E(TAG, "Fail to read data from socket: %s", strerror(errno));
                    continue;
                } else if (0 == read_size) {
                    is_online = 0;
                    I(TAG, "The server is down, exiting...");
                    break;
                } else {
                    D(TAG, "Read %d bytes data from socket: %d", read_size, p_fd->fd);
                }
                // just write the data from buff to standard output
                write_size = write(1, buff, read_size);
            } else if (s_socket == p_fd->fd && p_fd->revents != 0) {
                // POLLERR | POLLHUP
                D(TAG, "Current event: %d, error event: %d, %d", p_fd->revents, POLLERR, POLLHUP);
                is_online = 0;
                I(TAG, "Some error in the connection, exiting....");
                break;
            }
        }
    }
    free(buff);

main_register_error:
    if (NULL != name_line) free(name_line);

main_socket_error:
    close(s_socket);
    return 0;
}

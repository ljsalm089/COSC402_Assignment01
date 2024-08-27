# include <time.h>
# include <unistd.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <sys/param.h>
# include <string.h>
# include <poll.h>
# include <pthread.h>

# include "common.h"
# include "collections/list.h"
# include "io/common_io.h"
# include "entity/client_info.h"
# include "entity/message.h"
# include "net_util.h"

#define TAG "IMServer"

#define __DEBUG__ 1
#ifdef __DEBUG__
#define POLL_TIMEOUT 5000

#define PAUSE() sleep(1)
#else
#define POLL_TIMEOUT 10
#define PAUSE() 
#endif

// link list node to link all the client info
typedef struct
{
    ListHead node;

    PClientInfo info;
} ClientNode;
typedef ClientNode * PClientNode;

// struct to save all the data need in the server
typedef struct
{
    // store all online client informations
    ListHead clients;
    // current online client number
    int client_count;
    // spin lock to protect the client list and client number
    pthread_mutex_t lock;

    // if the server is running
    int is_running;

    // save the server info, contains the socket
    PClientInfo info;
 } ServerInfo;
typedef ServerInfo * PServerInfo;

PServerInfo init_server_info(int socket_id)
{
    PClientInfo client_info = new_client_info(socket_id, TAG, C_TYPE_S);
    if (!client_info) {
        E(TAG, "Unable to create client info for server");
        return NULL;
    }

    PServerInfo server_info = (PServerInfo) malloc(sizeof(ServerInfo));
    if (!server_info) {
        E(TAG, "Unable to create server info");
        goto error_with_client_info;
    }

    INIT_LIST_HEAD(&server_info->clients);
    server_info->client_count = 0;

    int ret = pthread_mutex_init(&server_info->lock, NULL);
    if (SUCC != ret) {
        E(TAG, "Unable to initialise the spin lock to protect the clients list(%d)", ret);
        goto error_with_server_info;
    }
    server_info->info = client_info;
    server_info->is_running = 1;
    return server_info;

error_with_server_info:
    free(server_info);

error_with_client_info:
    release_client_info(client_info);

    return NULL;
}

void release_server_info(PServerInfo info)
{
    if (!info) return;

    info->is_running = 0;
    release_client_info(info->info);
    pthread_mutex_destroy(&info->lock);
    free(info);
}

PMsg user_login(PClientInfo info, size_t size, char * msg)
{
    int cmd_size = strlen(CMD_LOGIN);
    
    PMsg message = null;
    char * enter = strchr(msg, '\n');
    if (enter) {
        *enter = '\0';
    }
    update_client_name(info, msg + cmd_size);

    D(TAG, "Client: %s login to the system", info->name);

    // notify other clients that info has logined
    // message: FROM SYSTEM: {name} is online now
    int message_len = strlen(MSG_SYS_PREFIX) + strlen(info->name) + strlen(MSG_LOGIN_SUFFIX) + 3;
    message = create_new_msg(message_len);

    if (message) {
        sprintf(message->content, MSG_SYS_PREFIX " %s " MSG_LOGIN_SUFFIX, info->name);
        D(TAG, "Construct message to all user: %s", message->content);
        strcpy(message->target, CMD_TO_ALL);
        strcpy(message->exclude, info->name);
    }

    return message;
}

PMsg send_msg_to_all(PClientInfo info, size_t size, char * msg) 
{
    return NULL;
}

PMsg send_msg_to_c(PClientInfo info, size_t size, char * msg)
{
    return NULL;
}

PClientInfo * get_client_list(PServerInfo server_info, int * items)
{
    pthread_mutex_lock(&server_info->lock);
    PClientInfo * clients = (PClientInfo *) malloc(sizeof(PClientInfo) * server_info->client_count);
    if (!clients) {
        E(TAG, "Unable to allocate memory for client list");
        * items = 0;
    } else {
        * items = server_info->client_count;

        PListHead ptr;
        PClientNode curr;
        int index = 0;

        list_for_each(ptr, &server_info->clients) {
            curr = list_entry(ptr, ClientNode, node);

            clients[index ++] = curr->info;
        }
    }
    pthread_mutex_unlock(&server_info->lock);

    return clients;
}

void user_logout(PServerInfo server_info, PClientInfo info)
{
    pthread_mutex_lock(&server_info->lock);
    PListHead ptr;
    PClientNode curr;

    list_for_each(ptr, &server_info->clients) {
        curr = list_entry(ptr, ClientNode, node);

        if (curr->info->socket_id == info->socket_id) {
            list_del(&curr->node);
            free(curr);
            break;
        }
    }
    pthread_mutex_lock(&server_info->lock);

    // TODO create logout msg and send to other users

}

void try_to_handle_msg(PServerInfo server_info, PClientInfo info)
{
    char *msg = null;
    size_t msg_size = fetch_msg_from_client(info, &msg);

    int need_to_flush_msg = 0;
    while (msg_size > 0) {
        D(TAG, "Got message from %d: %s", info->socket_id, msg);
        // got a message, try to parse and handle it

        PMsg redirect_msg = null;
        if (msg == strstr(msg, CMD_LOGIN)) {
            // user login
            redirect_msg = user_login(info, msg_size, msg); 
        } else if (msg == strstr(msg, CMD_TO_ALL)) {
        } else if (msg == strstr(msg, CMD_TO_C)) {
        }

        if (redirect_msg) {
            // need to redirect the msg to the client(s)
            int size = 0;
            PClientInfo * clients = get_client_list(server_info, &size);

            int to_all = (SUCC == is_msg_to_all(redirect_msg)) ? 1 : 0;
            for (int index = 0; index < size; index ++) {
                PClientInfo info = clients[index];
                int skip = SUCC == filter_out(redirect_msg, info->name) ? 1 : 0;

                D(TAG, "Check if send to this client(%s): %d, is all: %d", 
                        info->name, skip, to_all);

                if (skip) continue;

                if (to_all || SUCC == is_target(redirect_msg, info->name)) {
                    send_msg_to_client(info, redirect_msg->content, redirect_msg->size);
                    need_to_flush_msg ++;
                }
            }
            
            release_msg(redirect_msg);
        }

        if (msg) free(msg);
        msg = null;
        msg_size = 0;

        msg_size = fetch_msg_from_client(info, &msg);
    }

    if (need_to_flush_msg) {
        // TODO notify the writing thread to flush messages to sockets
    }
}

void * get_polling_and_client_list(PServerInfo server_info, int * items)
{
    pthread_mutex_lock(&server_info->lock);
    int size = server_info->client_count * (sizeof(struct pollfd) + sizeof(PClientInfo));
    void * data = malloc(size);
    if (!data) {
        E(TAG, "Allocate memory for polling list and client infos fail");
        * items = 0;
    } else {
        * items = server_info->client_count;
        memset(data, 0, size);

        int index = 0;
        PListHead ptr;
        PClientNode curr;

        struct pollfd * polling = (struct pollfd *) data;
        PClientInfo * clients = data + server_info->client_count * sizeof(struct pollfd);

        list_for_each(ptr, &server_info->clients) {
            curr = list_entry(ptr, ClientNode, node);

            D(TAG, "Polling socket from list: %d", curr->info->socket_id);

            struct pollfd * tmp = polling + index;
            clients[index] = curr->info;
            tmp->fd = curr->info->socket_id;
            tmp->events = POLLIN;
            index ++;
        }
    }
    pthread_mutex_unlock(&server_info->lock);
    return data;
}

void * read_sockets(void * param) 
{
    PServerInfo server_info = (PServerInfo) param;

    pthread_t current_tid = pthread_self();
    D(TAG, "Start a new thread (%d) to handle client sockets.", current_tid);

    extern int errno;
    int ret;

    do {
        int items = 0;
        void * data = get_polling_and_client_list(server_info, &items);
        if (!data) {
            break;
        }
        struct pollfd * polling_fds = (struct pollfd *) data;
        PClientInfo * infos = (PClientInfo *) (data + items * sizeof(struct pollfd));
        
        DEBUG_BLOCK(
        for (int index = 0; index < items; index ++) {
            D(TAG, "socket polling list: %d", polling_fds[index].fd);
            D(TAG, "socket from infos: %d", infos[index]->socket_id);
        }
        )

        D(TAG, "Start polling events to read data from socket: %d", items);
        ret = poll(polling_fds, items, POLL_TIMEOUT);
        if (FAIL == ret) {
            E(TAG, "Failed to poll events from sockets: %s", strerror(errno));
            continue;
        } else if (SUCC == ret) {
            D(TAG, "Poll on thread(%d) timeout", current_tid);
            continue;
        } else {
            D(TAG, "Some events occur in some socket");
        }

        int has_read_from_socket = 0;
        for (int index = 0; index < items; index ++) {
            struct pollfd tmp = polling_fds[index];
            PClientInfo info = infos[index];

            if (tmp.fd != info->socket_id) {
                // wrong status, just ignore this item
                D(TAG, "socket id(%d) in pollfd does not equal to socket id(%d) in client infos,"
                        " this shouldn't happen", tmp.fd, info->socket_id);
                continue;
            }
            D(TAG, "Captured some event(%d) on socket(%d)", tmp.revents, info->socket_id);

            if (tmp.revents & POLLIN) {
                int fetch_count = fetch_from_client(info);
                D(TAG, "fetch %d bytes from socket: %d", fetch_count, info->socket_id);
                if (fetch_count) {
                    // has read sth, check if there is a command in the cache
                    try_to_handle_msg(server_info, info);
                } else {
                    E(TAG, "Captured POLLIN on socket(%d), but read nothing, "
                            "maybe the socket has been closed by the peer", info->socket_id);
                    // read nothing, but got the event, maybe the socket has been closed by peer
                    goto handle_socket_exception;
                }
            } else { // POLLERR | POLLHUP
                // handle the error situation
                goto handle_socket_exception;
            }

            continue;
handle_socket_exception:
            if (SUCC != is_client_connected(info)) {
                // the connection between client and server is invalid
                // logout this client and remove it from list
                user_logout(server_info, info);
            }
        }
        free(data);

        PAUSE();
    } while (server_info->is_running);

    D(TAG, "Exiting the thread: %d", current_tid);

    return NULL;
}

void * write_sockets(void *param)
{
    PServerInfo server_info = (PServerInfo) param;
    
    PClientInfo * clients = null;
    size_t current_size = 0, allocated_size = 10;

    do {
        int size = 0;
        PClientInfo * clients = get_client_list(server_info, &size);
        if (!clients) {
            break;
        }

        D(TAG, "Check %d client, if there are some msg to send", size);

        for (int index = 0; index < size; index ++) {
            PClientInfo info = clients[index];
            D(TAG, "Check any msg to send: %s", info->name);
            if (SUCC != is_client_connected(info)) {
                D(TAG, "Checking, socket (%d) is not connected???", info->socket_id);
                continue;
            }

            size_t flush_size  = flush_messages(info);
            if (flush_size > 0) {
                D(TAG, "Flush %d bytes data to the socket(%d)", flush_size, info->socket_id);
            }
        }
        PAUSE();
        free(clients);
    } while (server_info->is_running);
    return NULL;
}

void * handle_listen(void * param) 
{
    PServerInfo server_info = (PServerInfo) param;

    extern int errno;
    int ret = 0;

    pthread_t reading_thread = -1;
    pthread_t writing_thread = -1;

    while (server_info->is_running) {
        int client_id = accept(server_info->info->socket_id, NULL, NULL);
        if (client_id < 0) {
            E(TAG, "unable to accept connection from the client: %s", strerror(errno));
            continue;
        }

        PClientInfo new_client = new_client_info(client_id, UNKNOWN_CLIENT, C_TYPE_C);
        if (!new_client) {
            E(TAG, "Received new connection, but unable to create client info for it");
            continue;
        }
        D(TAG, "received an new connection: %s:%d", new_client->peer_info.ip, new_client->peer_info.port);

        PClientNode node = (PClientNode) malloc(sizeof(ClientNode));
        if (!node) {
            E(TAG, "Unable to create client node to store client info");
            release_client_info(new_client);
            continue;
        }
        node->info = new_client;

        pthread_mutex_lock(&server_info->lock);

        int is_empty = list_empty(&server_info->clients);
        list_add_tail(&node->node, &server_info->clients);
        server_info-> client_count ++;
        D(TAG, "Add new client info to clients list");

        pthread_mutex_unlock(&server_info->lock);

        if (is_empty) {
            // create new thread to accept message from all clients
            ret = pthread_create(&reading_thread, NULL, read_sockets, server_info);
            if (SUCC != ret) {
                E(TAG, "Unable to create new thread to read data from sockets: %d", ret);
                break;
            }

            ret = pthread_create(&writing_thread, NULL, write_sockets, server_info);
            if (SUCC != ret) {
                E(TAG, "Unable to create new thread to write data from sockets: %d", ret);
                break;
            }
        }
    }
    server_info->is_running = 0;

    if (reading_thread != -1) {
        ret = pthread_join(reading_thread, NULL);
        if (SUCC != ret) {
            E(TAG, "Unable to wait for reading thread to exit: %d", ret);
        }
    }

    if (writing_thread != -1) {
        ret = pthread_join(writing_thread, NULL);
        if (SUCC != ret) {
            E(TAG, "Unable to wait for writing thread to exit: %d", ret);
        }
    }

    return NULL;
}

int main(int argc, char** argv)
{
    I(TAG, "Starting IM server!");
    if (argc < 3) {
        err_sys(TAG, "usage: %s {server address} {aserver port}");
    }

    PIP server_addr;
    int ip_size;

    int ret = 0;
    if ((ret = get_addr_info_from_ip_and_port(argv[1], argv[2], &server_addr, 
                    &ip_size)) != 0) {
        E(TAG, "Unable to parse server address or port");
        return -1;
    }

    // create socket
    int listen_fd = socket(server_addr->sa_family, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        err_sys(TAG, "unable to create socket.");
    }
    D(TAG, "create socket succefully.");

    ret = bind(listen_fd, server_addr, ip_size);
    if (ret < 0) {
        err_sys(TAG, "unable to bind the socket to specified address and port.");
    }
    D(TAG, "bind socket to specified address and port successfully.");

    // listen and waiting for connect
    ret = listen(listen_fd, 150);
    if (ret < 0) {
        close(listen_fd);
        err_sys(TAG, "unable to listen in specified port.");
    }
    D(TAG, "listen to specified port successfully.");

    if (initialise_cache_module() != 0) {
        close(listen_fd);
        err_sys(TAG, "Initialise cache module error");
    }

    PServerInfo server_info = init_server_info(listen_fd);
    if (!server_info) {
        close(listen_fd);
        release_cache_module();
        err_sys(TAG, "Initialise sever info error");
    }

    D(TAG, "Initialise the server info successfully.");

    pthread_t listen_thread = -1;
    ret = pthread_create(&listen_thread, NULL, handle_listen, server_info);
    if (ret != 0) {
        E(TAG, "Fail to create a listening thread: %d", ret);
    } else {
        D(TAG, "Create listening thread successfully");
        // wait for the listening thread
        ret = pthread_join(listen_thread, NULL);
        if (!ret) {
            E(TAG, "Unable to wait for the listening thread: %d", ret);
        }
    }

    // release data
    release_server_info(server_info);
    
    // release cache module
    release_cache_module();
   
    // relase the socket
    close(listen_fd);

    return 0;
}

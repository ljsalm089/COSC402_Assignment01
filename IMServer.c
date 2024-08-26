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
# include "net_util.h"

#define TAG "IMServer"

#define UNKNOWN_CLIENT "unknown"

#ifdef __DEBUG__
#define POLL_TIMEOUT 500000
#else
#define POLL_TIMEOUT 10
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

    int ret = 0;
    if (!(ret = pthread_mutex_init(&server_info->lock, NULL))) {
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

void try_to_handle_msg(PClientInfo info)
{
}

int make_sure_array(void ** ptr, int expected_size)
{
    void * tmp = null;
    if (!*ptr) {
        tmp = malloc(expected_size);
        if (!tmp) {
            E(TAG, "Unable to allocate memory");
            return -1;
        }
        *ptr = tmp;
    } else {
        tmp = realloc(*ptr, expected_size);
        if (!tmp) {
            E(TAG, "Unable to reallocate memory");
            return -1;
        }
        *ptr = tmp;
    }
    return SUCC;
}

void * read_sockets(void * param) 
{
    PServerInfo server_info = (PServerInfo) param;

    pthread_t current_tid = pthread_self();
    D(TAG, "Start a new thread (%d) to handle client sockets.", current_tid);

    extern int errno;
    int ret;

    // the size of polling_fds should be max_size * (sizeof(struct pollfd) + sizeof (PClientInfo));
    // the prefix max_size * sizeof(struct pollfd) used to store pollfd list, 
    // the suffix max_size * sizeof(PClientInfo) used to store PClientInfo
    struct pollfd * polling_fds = null;
    nfds_t current_size = 0, allocated_size = 10;

    int mem_unit = sizeof(struct pollfd) + sizeof(PClientInfo);

    do {
        pthread_mutex_lock(&server_info->lock);

        int expected_size = allocated_size;
        if (allocated_size <= server_info->client_count) {
            expected_size = max(allocated_size * 2, server_info->client_count + 10);
        }

        if (!polling_fds || expected_size > allocated_size) {
            // if polling_fds is null or no enought space, then need to allocate or expand the size
            if (FAIL(make_sure_array(&(void *) polling_fds, expected_size * unit))) {
                E(TAG, "Unable to allocate or expand memory size for polling");
                if (!polling_fds) {
                    continue;
                }
            } else {
                D(TAG, "Allocate or expand memory size for polling success");
                allocated_size = expected_size;
                current_size = server_info->client_count;
            }
        } else {
            // no need to expand the memory size
            current_size = server_info->client_count;
        }

        // clear the content in this list
        memset(polling_fds, 0, allocated_size * mem_unit);

        // convert the tail info PClientInfo array
        PClientInfo * infos = (PClientInfo *) ((char *) polling_fds + allocated_size * sizeof(struct pollfd));

        int index = 0;
        PListHead ptr;
        PClientNode curr;

        list_for_each(ptr, &server_info->clients) {
            if (index >= current_size) break;

            curr = list_entry(ptr, ClientNode, node);

            struct pollfd tmp = polling_fds[index];
            infos[index] = curr->info; 

            tmp.fd = curr->info->socket_id;
            tmp.events = POLLIN;
        }

        pthread_mutex_unlock(&server_info->lock);

        ret = poll(polling_fds, current_size, POLL_TIMEOUT);
        if (-1 == ret) {
            E(TAG, "Failed to poll events from sockets: %s", strerror(errno));
            continue;
        } else if (0 == ret) {
            D(TAG, "Poll on thread(%d) timeout", current_tid);
            continue;
        } else {
            D(TAG, "Some events occur in some socket");
        }

        int has_read_from_socket = 0;
        for (index = 0; index < current_size; index ++) {
            struct pollfd tmp = polling_fds[index];
            PClientInfo info = infos[index];

            if (tmp.fd != info->socket_id) {
                // wrong status, just ignore this item
                continue;
            }
            D(TAG, "Captured some event(%d) on socket(%d)", tmp.revents, info->socket_id);

            if (tmp.revents & POLLIN) {
                if (fetch_from_client(info)) {
                    // has read sth, check if there is a command in the cache
                    try_to_handle_msg(info);
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
            if (FAIL(is_socket_valid(info->socket_id))) {
                // invalid socket, logout this client and remove it from list
            }
        }
    } while (server_info->is_running);

    D(TAG, "Exiting the thread: %d", current_tid);

handle_sockets_release_with_fds:
    if (polling_fds) free(polling_fds);

    return NULL;
}

void * write_sockets(void *param)
{
    PServerInfo server_info = (PServerInfo) param;
    
    PClientInfo * clients = null;
    size_t current_size = 0, allocated_size = 10;

    do {
        pthread_mutex_lock(&server_info->lock);

        size_t expected_size = allocated_size;
        if (allocated_size <= server_info->client_count) {
            expected_size = max(allocated_size * 2, server_info->client_count + 10);
        }

        if (!clients || expected_size > allocated_size) {
            // need to allocate or expand the memory size
            if (FAIL(make_sure_array(&(void *) clients, expected_size * sizeof(PClientInfo)))) {
                E(TAG, "Fail to allocated expand the memory size");
                if (!clients) {
                    continue;
                }
            } else {
                allocated_size = expected_size;
                current_size = server_info->client_count;
            }
        } else {
            current_size = server_info->client_count;
        }

        // clear the content
        memset(clients, 0, allocated_size * sizeof(PClientInfo));

        int index = 0;
        PListHead ptr;
        PClientNode curr;


        list_for_each(ptr, &server_info->clients) {
            curr = list_entry(ptr, ClientNode, node);

            clients[index] = curr->info;
        }

        pthread_mutex_unlock(&server_info->lock);


        for (index = 0; index < current_size; index ++) {
            PClientInfo info = clients[index];
            if (FAIL(is_client_connected(info))) {
                continue;
            }

            size_t flush_size  = flush_messages(info);
            D(TAG, "Flush %d bytes data to the socket(%d)", flush_size, info->socket_id);
        }
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
            if (FAIL(ret)) {
                E(TAG, "Unable to create new thread to read data from sockets: %d", ret);
                break;
            }

            ret = pthread_create(&writing_thread, NULL, write_sockets, server_info);
            if (FAIL(ret)) {
                E(TAG, "Unable to create new thread to write data from sockets: %d", ret);
                break;
            }
        }
    }
    server_info->is_running = 0;

    if (reading_thread != -1) {
        ret = pthread_join(reading_thread, NULL);
        if (FAIL(ret)) {
            E(TAG, "Unable to wait for reading thread to exit: %d", ret);
        }
    }

    if (writing_thread != -1) {
        ret = pthread_join(writing_thread, NULL);
        if (FAIL(ret)) {
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

#ifndef __CLIENT_INFO_H__
#define __CLIENT_INFO_H__

#include <time.h>
#include <stdlib.h>
#include <string.h>

#include "../collections/list.h"

#define CLIENT_NAME_MAXIMUM_SIZE 30

#define C_TYPE_C 1
#define C_TYPE_S 2

typedef struct {
    char * ip;
    int port;
} PeerInfo;

typedef PeerInfo * PPeerInfo;

typedef struct {
    // basic client info
    int socket_id;

    // peer info sock the socket
    PeerInfo peer_info;

    char name[CLIENT_NAME_MAXIMUM_SIZE];
    time_t login_time;
    unsigned short type;

    void * pub;
    void * priv; 
} ClientInfo;

typedef ClientInfo * PClientInfo;

/**
 * Create a new client info
 * return: null means error, otherwise return the newly created info
 */
PClientInfo new_client_info(int socket_id, char *client_name, int type);

/**
 * read data from client and cache it
 * return: how many bytes read from socket, -1 means some error, 0 means nothing
 */
size_t fetch_from_client(PClientInfo info);

/**
 * fetch a message from client
 * return: -1 means error, 0 means no message, an positive integer means the size of the message
 */
size_t fetch_msg_from_client(PClientInfo info, char **msg_buff);

/**
 * send a message to client
 * @return: none
 */
void send_msg_to_client(PClientInfo info, void * msg, size_t size);

/**
 * flush all the message in the buffer to the socket
 * @return: -1 means error, 0 means no message to flush, positive integer means the bytes that flushed to the socket
 */
size_t flush_messages(PClientInfo info);

/**
 * release all the resource related to the client info
 * return: 0 means success, -1 means error
 */
int release_client_info(PClientInfo info);

/**
 * check if the socket of the client is still connected
 * @return: 0 means success, otherwise fail
 */
int is_client_connected(PClientInfo info);
#endif // __CLIENT_INFO_H__

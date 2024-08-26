#ifndef __net_util_h__
#define __net_util_h__

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

typedef struct sockaddr IP;
typedef IP * PIP;
typedef struct sockaddr_in IPv4;
typedef IPv4 * PIPv4;
typedef struct sockaddr_in6 IPv6;
typedef IPv6 * PIPv6;

int get_addr_and_port(int sock_id, char ** addr, int *port);

int get_addr_info_from_ip_and_port(char * ip, char * port, PIP * addr, int * size);

int is_socket_valid(int socket_id);

#endif // __net_util_h__

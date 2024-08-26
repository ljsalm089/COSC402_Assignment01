#include <errno.h>

#include "net_util.h"
#include "common.h"

#define TAG "net_util"

#define ANY_HOST "*"

int get_addr_and_port(int sock_id, char ** addr, int * port) {
    D(TAG, "get address and port info from the socket: %d", sock_id);

    int ret = SUCC;
    extern int errno;

    size_t max_size = max(sizeof(IPv4), sizeof(IPv6));
    socklen_t addr_size = max_size;
    * addr = (char *) malloc (INET6_ADDRSTRLEN);
    void * addr_info = malloc (max_size);

    uint16_t uport = 0;

    ret = getpeername(sock_id, (PIP) addr_info, &addr_size);
    if (0 != ret) {
        D(TAG, "Fail to get peer name: %d", ret);
        goto error_handle;
    }

    if (sizeof(IPv6) == addr_size) {
        // parse as ipv6 address
        if (!inet_ntop(AF_INET6, &((PIPv6) addr_info)->sin6_addr, *addr, addr_size)) {
            E(TAG, "Unable to convert to the ipv6 address: %s", strerror(errno));
            goto error_handle;
        }
        uport = ntohs(((PIPv6) addr_info)->sin6_port);
    } else if (sizeof(IPv4) == addr_size) {
        // parse as ipv4 address
        if (!inet_ntop(AF_INET, &((PIPv4) addr_info)->sin_addr, *addr, addr_size)) {
            E(TAG, "Unable to convert to the ipv4 address: %s", strerror(errno));
            goto error_handle;
        }
        uport = ntohs(((PIPv4) addr_info)->sin_port);
    } else {
        E(TAG, "unknown address type, address size is %d", addr_size);
        ret = -1;
        goto error_handle;
    }
    *port = uport;
    return 0;

error_handle:
    free(addr_info);
    free(* addr);
    * addr = NULL;

    return ret;
}

int get_addr_info_from_ip_and_port(char * ip, char * port, PIP * addr, int * size) {
    int ret = SUCC;

    int porti = atoi(port);
    if (porti <= 0) {
        E(TAG, "Invalid port number or unable to convert the port: %s", port);
        return -1;
    }
    
    struct in_addr addr4;
    struct in6_addr addr6;

    if (!strcmp(ANY_HOST, ip) || inet_pton(AF_INET, ip, &addr4) == 1) {
        D(TAG, "%s is an IPv4 address.", ip);
        *size = sizeof(IPv4);
        PIPv4 ipv4 = (PIPv4) malloc(*size);
        bzero(ipv4, *size);
        if (!strcmp(ANY_HOST, ip)) {
            ipv4->sin_addr.s_addr = htonl(INADDR_ANY);
        } else {
            ipv4->sin_addr = addr4;
        }
        ipv4->sin_port = htons(porti);
        ipv4->sin_family = AF_INET;
        *addr = (PIP) ipv4;
    } else if (inet_pton(AF_INET6, ip, &addr6) == 1) {
        D(TAG, "%s is an IPv6 address.", ip);
        *size = sizeof(IPv6);
        PIPv6 ipv6 = (PIPv6) malloc(*size);
        bzero(ipv6, *size);
        ipv6->sin6_addr = addr6;
        ipv6->sin6_port = htons(porti);
        ipv6->sin6_family = AF_INET6;
        *addr = (PIP) ipv6;
    } else {
        D(TAG, "Invalid address: %s", ip);
        ret = -1;
    }

    return ret;
}

int is_socket_valid(int socket_id) 
{
    int error = 0;
    socklen_t len = sizeof(error);

    int ret = getsockopt(socket_id, SOL_SOCKET, SO_ERROR, &error, &len);

    if (ret == 0 && 0 == error) {
        return SUCC;
    } else {
        return -1;
    }
}

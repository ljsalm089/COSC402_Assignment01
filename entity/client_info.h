#ifndef __CLIENT_INFO_H__
#define __CLIENT_INFO_H__

#include <time.h>
#include <stdlib.h>
#include <string.h>

#include "../collections/list.h"

#define C_TYPE_C
#define C_TYPE_S

typedef struct client_info {
    // basic client info
    int socket_id;
    char name[30];
    time_t login_time;
    unsigned short type;

    
} ClientInfo;

typedef ClientInfo * PClientInfo;

PClientInfo new_client_info(char *name, unsigned short type) {
    PClientInfo info = (PClientInfo) malloc (sizeof(ClientInfo));

    int name_max_size = sizeof(info->name) - 1;
    if (strlen(name) >= name_max_size) {
        memcpy(info->name, name, name_max_size);
        info->name[name_max_size] = '\0';
    } else {
        strcpy(info->name, name);
    }
    time(&info->login_time);
    info->type = type;
    return info;
}

#endif // __CLIENT_INFO_H__
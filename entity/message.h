#ifndef __MESSAGE_H__
#define __MESSAGE_H__

# include "client_info.h"

#define CMD_LOGIN "LOGIN:"
#define CMD_TO_ALL "TO-ALL:"
#define CMD_TO_C "TO-C-"

#define MSG_SYS_PREFIX "FROM SYSTEM:"
#define MSG_LOGIN_SUFFIX "is online now"


#define UNKNOWN_CLIENT "unknown"


typedef struct
{
    char target[CLIENT_NAME_MAXIMUM_SIZE];
    char exclude[CLIENT_NAME_MAXIMUM_SIZE];
    size_t size;
    char * content;
} Msg;

typedef Msg * PMsg;

int is_msg_to_all(PMsg msg);

int filter_out(PMsg msg, char *target);

int is_target(PMsg msg, char *target);

PMsg create_new_msg(size_t size);

void release_msg(PMsg msg);

#endif // __MESSAGE_H__

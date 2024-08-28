#ifndef __MESSAGE_H__
#define __MESSAGE_H__

# include "client_info.h"

// LOGIN: name\n
#define CMD_LOGIN "LOGIN: "

// TO_ALL: hello world\n
#define CMD_TO_ALL "TO-ALL:" 

// TO-C-123: hello world\n
#define CMD_TO_C "TO-C-"

#define MSG_PREFIX_INDICATOR ":"
#define MSG_SYS_PREFIX "FROM SYSTEM:"
#define MSG_PEER_PREFIX "FROM"
#define MSG_LOGIN_SUFFIX "is online now"
#define MSG_LOGOUT_SUFFIX "has logged out"

#define MSG_EDGE_INDICATOR '\n'

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

PMsg create_p2p_msg_base_on_content(char *sender, char * target, char * s_content);

PMsg create_sys_msg_base_on_content(char * target, char * s_content);

#endif // __MESSAGE_H__

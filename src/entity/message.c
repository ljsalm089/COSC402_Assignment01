# include <stdlib.h>
# include <string.h>

# include "common.h"
# include "entity/message.h"

#define TAG "message"

int is_msg_to_all(PMsg msg)
{
    return strcmp(msg->target, CMD_TO_ALL);
}

int filter_out(PMsg msg, char * target) 
{
    return (SUCC == strcmp(msg->exclude, target) 
            || SUCC == strcmp(target, UNKNOWN_CLIENT)) ? SUCC : -1;
}

int is_target(PMsg msg, char * target)
{
    return strcmp(msg->target, target);
}

PMsg create_new_msg(size_t size)
{
    char * content = (char *) malloc (size * sizeof(char));
    if (!content) {
        E(TAG, "Unable to allocate memory for the content of a message");
        return NULL;
    }

    memset(content, 0, size * sizeof(char));
    PMsg msg = (PMsg) malloc(sizeof(Msg));
    if (!msg) {
        E(TAG, "Unable to allocate memory for a message");
        free(content);
        return NULL;
    }
    msg->content = content;
    msg->size = size;
    return msg;
}

void release_msg(PMsg msg)
{
    if (!msg) return;
    free(msg->content);
    free(msg);
}

PMsg create_p2p_msg_base_on_content(char *sender, char * target, char * s_content)
{
    char * enter = strchr(s_content, MSG_EDGE_INDICATOR);
    if (enter) {
        *enter = '\0';
    }

    // redirect message to specified client or all
    // message format: FROM {sender}: {msg_content}\n\0
    int message_len = strlen(MSG_PEER_PREFIX) + strlen(sender) + strlen(s_content) + 5;
    PMsg message = create_new_msg(message_len);

    if (message) {
        sprintf(message->content, MSG_PEER_PREFIX " %s" MSG_PREFIX_INDICATOR " %s\n", 
                sender, s_content);
        // appending a \0 is for output the string into stdin
        // while writing the content to the socket, the \0 should be skipped
        message->size --;
        if (target) {
            strcpy(message->target, target);
        }
        strcpy(message->exclude, sender);
        D(TAG, "Constructed a p2p message from %s to %s: %s", sender, message->target, 
                message->content);
    }
    return message;
}

PMsg create_sys_msg_base_on_content(char * target, char * s_content)
{
    char * enter = strchr(s_content, MSG_EDGE_INDICATOR);
    if (enter) {
        *enter = '\0';
    }

    // redirect message to specified client or all
    // message format: FROM SYSTEM: {s_content}\n\0
    int msg_len = strlen(MSG_SYS_PREFIX) + strlen(s_content) + 3;
    PMsg msg = create_new_msg(msg_len);

    if (msg) {
        sprintf(msg->content, MSG_SYS_PREFIX " %s\n", s_content);
        // appending a \0 is for output the string into stdin
        // while writing the content to the socket, the \0 should be skipped
        msg->size --;
        if (target) {
            strcpy(msg->target, target);
        }
        D(TAG, "Constructed a system message to %s: %s", msg->target, msg->content);
    }
    return msg;
}

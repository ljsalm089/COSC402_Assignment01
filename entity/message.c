# include <stdlib.h>
# include <string.h>

# include "../common.h"
# include "message.h"

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

# include <string.h>
# include <errno.h>
# include <sys/ioctl.h>
# include <pthread.h>

# include "../common.h"
# include "../collections/list.h"
# include "../net_util.h"
# include "../io/buffer.h"

# include "client_info.h"

#define TAG "ClientInfo"

# define CONVERT(data, info) PClientData data = _convert_client_info((info));

extern int errno;

typedef struct {
    // client info which can be accessed exnternally
    ClientInfo info;

    // reading buffer
    PBuffer reading_buffer;

    // writing buffer
    PBuffer writing_buffer;

    // lock to protect the reading_cache
    pthread_spinlock_t reading_lock;

    // lock to protect the writing_cache
    pthread_spinlock_t writing_lock;

    // save the position last time check the command, to avoid rechecking from the beginning
    size_t last_check_cmd_index;
} ClientData;

typedef ClientData * PClientData;

PClientData _convert_client_info(PClientInfo info)
{
    PClientData data = (PClientData)((char *) info - offsetof(ClientData, info));
    return data;
 }


PClientInfo new_client_info(int socket_id, char *client_name, int type) 
{
    int ret = SUCC;
    PClientData data = (PClientData) malloc (sizeof(ClientData));
    if (!data) {
        E(TAG, "Unable to allocate memory for client info(%d): %s", errno, strerror(errno));
        return null;
    }

    memset(data, 0, sizeof(ClientData));

    if (C_TYPE_C == type) {
        ret = get_addr_and_port(socket_id, &data->info.peer_info.ip, &data->info.peer_info.port);
        if (SUCC != ret) {
            data->info.peer_info.ip = null;
            data->info.peer_info.port = -1;
            E(TAG, "Unable to get address and port from socket");
        }
    }

    update_client_name(&data->info, client_name);

    
    data->info.type = type;
    data->info.socket_id = socket_id;
    time(&(data->info.login_time));

    data->reading_buffer = create_new_buffer();
    if (!data->reading_buffer) {
        E(TAG, "Create reading buffer failed");
        goto error_with_data;
    }
    
    data->writing_buffer = create_new_buffer();
    if (!data->writing_buffer) {
        E(TAG, "Create writing buffer failed");
        goto error_with_data;
    }

    ret = pthread_spin_init(&data->reading_lock, PTHREAD_PROCESS_PRIVATE);
    if (SUCC != ret) {
        E(TAG, "Unable to initialise the spin lock for reading in client info: %d", ret);
        goto error_with_data;
    }

    ret = pthread_spin_init(&data->writing_lock, PTHREAD_PROCESS_PRIVATE);
    if (SUCC != ret) {
        E(TAG, "Unable to initialise the spin lock for writing in client info: %d", ret);
        goto error_with_data;
    }
    return &data->info;

error_with_buffer:
    if (!data->reading_buffer) destroy_buffer(data->reading_buffer);
    if (!data->writing_buffer) destroy_buffer(data->writing_buffer);

error_with_data:
    free(data);

    if (data->info.peer_info.ip) free(data->info.peer_info.ip);

    return null;
}

void update_client_name(PClientInfo info, char * name)
{
    D(TAG, "Update client name from %s to %s", info->name, name);
    memset(info->name, 0, CLIENT_NAME_MAXIMUM_SIZE);
    if (null == name) {
        strcpy(info->name, "Unknown");
    } else {
        int name_length = min(strlen(name), CLIENT_NAME_MAXIMUM_SIZE - 1);
        strncpy(info->name, name, name_length);
    }
}

size_t fetch_from_client(PClientInfo info) 
{
    CONVERT(data, info);

    int byte_available;
    if (ioctl(info->socket_id, FIONREAD, &byte_available) < 0) {
        E(TAG, "Detect the readable size error(%d): %s", errno, strerror(errno));
        return -1;
    } else {
        D(TAG, "Try to read %d bytes from socket", byte_available);
    }
    
    return read_into_buffer(info->socket_id, data->reading_buffer, byte_available);  
}

size_t check_command_suffix(void * buff, int size, void * arg) 
{
    char * suffix = (char *) arg;

    char *target = index(buff, *suffix);
    if (!target) {
        return -1;
    } else if (target - (char *) buff < size) {
        return target - (char *) buff;
    }
    return -1;
}

size_t fetch_msg_from_client(PClientInfo info, char **msg_buff)
{
    CONVERT(data, info);

    char suffix = '\n';
    size_t cmd_suffix_index = find_in_buffer(data->reading_buffer, 
            data->last_check_cmd_index, check_command_suffix, &suffix);
    D(TAG, "fetch_msg_from_client, cmd_suffix_index: %d", cmd_suffix_index);
    if (cmd_suffix_index < 0) {
        data->last_check_cmd_index = buffer_size(data->reading_buffer);
        return -1;
    } else {
        // find the suffix of the command
        int read_size = read_from_buffer(data->reading_buffer, (void **)msg_buff, cmd_suffix_index + 1);
        data->last_check_cmd_index = 0;
        return read_size;
    }
}

size_t send_msg_to_client(PClientInfo info, void *msg, size_t size)
{
    CONVERT(data, info);

    pthread_spin_lock(&data->writing_lock);
    size_t ret = write_into_buffer(data->writing_buffer, msg, size);
    D(TAG, "After write %d bytes data to buffer, current buffer size: %d", 
            size, buffer_size(data->writing_buffer));
    pthread_spin_unlock(&data->writing_lock);

    return ret;
}

size_t flush_messages(PClientInfo info)
{
    size_t ret = 0;
    CONVERT(data, info);

    pthread_spin_lock(&data->writing_lock);

    size_t b_size = buffer_size(data->writing_buffer);
    D(TAG, "Current buffer size for writing: %d", b_size);
    if (b_size > 0) {
        ret = write_from_buffer(info->socket_id, data->writing_buffer, b_size);
    }

    pthread_spin_unlock(&data->writing_lock);

    return ret;
}

int release_client_info(PClientInfo info)
{
    int ret = SUCC;
    if (!info) return ret;

    CONVERT(data, info);

    if (data->reading_buffer) destroy_buffer(data->reading_buffer);
    if (data->writing_buffer) destroy_buffer(data->writing_buffer);

    // release the cache
    pthread_spin_destroy(&data->reading_lock);
    pthread_spin_destroy(&data->writing_lock);

    if (info->peer_info.ip) free(info->peer_info.ip);
    free(data);
    return ret;
}

int is_client_connected(PClientInfo info)
{
    return is_socket_valid(info->socket_id);
}

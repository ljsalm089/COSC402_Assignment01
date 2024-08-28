# include <stddef.h>
# include <errno.h>
# include <stdio.h>
# include <unistd.h>
# include <string.h>

# include "../common.h"
# include "../collections/list.h"
# include "common_io.h"

# include "buffer.h"

# define TAG "Buffer"

# define CONVERT(n, o) PInnerBuffer n = _convert_buffer((o))

# define CACHE_POS(node) (node)->cache->buff
# define NODE_SIZE(node) (node)->w_offset - (node)->r_offset

# define CACHE_SIZE 512

typedef struct 
{
    int w_offset;
    int r_offset;
    PCache cache;

    ListHead node;
} _InnerNode;

typedef _InnerNode * PInnerNode;

typedef struct 
{
    Buffer inner;

    ListHead cache_list; 
} _InnerBuffer;

typedef _InnerBuffer * PInnerBuffer;

extern int errno;

PInnerBuffer _convert_buffer(PBuffer buff)
{
    PInnerBuffer outer = (PInnerBuffer)((char *) buff - offsetof(_InnerBuffer, inner));
    return outer;
}

PBuffer create_new_buffer() 
{
    PInnerBuffer buff = (PInnerBuffer) malloc (sizeof(_InnerBuffer));
    if (!buff) {
        E(TAG, "Allocate memory for buffer fail(%d): %s", errno, strerror(errno));
        return null;
    }
    INIT_LIST_HEAD(&buff->cache_list);

    return &buff->inner;
}

int destroy_buffer(PBuffer buffer)
{
    int ret = SUCC;
    CONVERT(buff, buffer);
    if (!buff)
        return -1;

    while (list_empty(&buff->cache_list)) {
        PInnerNode tmp = list_last_entry(&buff->cache_list, _InnerNode, node);
        list_del(&tmp->node);
        recycle_cache(tmp->cache);

        free(tmp);
    }
    free(buff);

    return ret;
}

size_t read_into_buffer(int fd, PBuffer buffer, size_t size)
{
    D(TAG, "Try to read %d bytes from fd(%d)", size, fd);
    int ret = SUCC;
    CONVERT(buff, buffer);

    LIST_HEAD(tmp_list);
    size_t last_w_offset = -1;

    PInnerNode node = null;
    if (list_empty(&buff->cache_list) 
            || list_last_entry(&buff->cache_list, _InnerNode, node)->w_offset == CACHE_SIZE) {
        node = (PInnerNode) malloc (sizeof(_InnerNode));
        if (!node) {
            E(TAG, "Allocate _InnerNode failed(%d): %s", errno, strerror(errno));
            return -1;
        }
        memset(node, 0, sizeof(_InnerNode));
        node->cache = get_new_cache(CACHE_SIZE);
        if (!node->cache) {
            E(TAG, "Get cache failed");
            free(node);
            ret = -1;
            goto return_func;
        }
        D(TAG, "Allocate new node to hold to buffer: %p", CACHE_POS(node));

        // add the new node to the temporary list first
        list_add_tail(&node->node, &tmp_list);
    } else {
        node = list_last_entry(&buff->cache_list, _InnerNode, node);
        last_w_offset = node->w_offset;
        D(TAG, "Reuse the old node to hold to buffer: %p", CACHE_POS(node));
    }

    size_t already_read_size = 0;
    while (already_read_size < size) {
        size_t read_size = min(size - already_read_size, CACHE_SIZE - node->w_offset);

        D(TAG, "reading %d bytes from fd(%d)", read_size, fd);

        int tmp_read_size = read(fd, CACHE_POS(node) + node->w_offset, read_size);
        if (tmp_read_size < 0) {
            E(TAG, "Read from fd error(%d): %s", errno, strerror(errno));
            ret = tmp_read_size;
            goto error_with_read;
        }
        already_read_size += tmp_read_size;
        node->w_offset += tmp_read_size;
        if (already_read_size == size || tmp_read_size < read_size) {
            // already read enough data or no more data to read
            ret = already_read_size;
            break;
        }
        // expected read size == really read size
        // keep reading
        
        node = (PInnerNode) malloc(sizeof(_InnerNode));
        if (!node) {
            E(TAG, "Allocate _InnerNode failed(%d): %s", errno, strerror(errno));
            ret = already_read_size;
            break;
        }
        memset(node, 0, sizeof(_InnerNode));
        node->cache = get_new_cache(CACHE_SIZE);
        if (!node->cache) {
            E(TAG, "Get new cache failed");
            free(node);
            ret = already_read_size;
            break;
        }
        
        list_add_tail(&node->node, &tmp_list);
    }

    if (ret > 0 && !list_empty(&tmp_list)) {
        // read successfully, merge two list
        // add the temporary list to the end of the original list
        list_splice(&tmp_list, buff->cache_list.prev);
    }
    return ret;

error_with_read:
    if (last_w_offset >= 0) {
        // already read part of data into the last node, reset its w_offset
        list_last_entry(&buff->cache_list, _InnerNode, node)->w_offset = last_w_offset;
    }

    // already read part of data into temporary list, recycle the list
    while(!list_empty(&tmp_list)) {
        PInnerNode node = list_last_entry(&tmp_list, _InnerNode, node);
        list_del(&node->node);

        recycle_cache(node->cache);
        free(node);
    }

return_func:
    return ret;
}

size_t write_from_buffer(int fd, PBuffer buffer, size_t size)
{
    size_t already_write_size = 0;
    CONVERT(buff, buffer);

    if(list_empty(&buff->cache_list)) {
        return already_write_size;
    }

    while (already_write_size < size) {
        PInnerNode node = list_first_entry_or_null(&buff->cache_list, _InnerNode, node);
        if (!node) break;

        size_t write_size = min(NODE_SIZE(node), size - already_write_size);
        size_t tmp_write_size = write(fd, CACHE_POS(node) + node->r_offset, write_size);
        if (tmp_write_size < 0) {
            E(TAG, "Write to file/socket(%d) fail(%d): %s", fd, errno, strerror(errno));
            break;
        }

        already_write_size += tmp_write_size;
        node->r_offset += tmp_write_size;

        if (node->r_offset == node->w_offset) {
            // already finish reading this node
            // there is no spare space in this node, need to remove it
            list_del(&node->node);

            recycle_cache(node->cache);
            free(node);

            node = list_first_entry_or_null(&buff->cache_list, _InnerNode, node);
        }
    }

    return already_write_size;
}

size_t read_from_buffer(PBuffer buffer, void **target, size_t size)
{
    size_t expected_read_size = 0;
    CONVERT(buff, buffer);

    if (!list_empty(&buff->cache_list)) {
        ListHead * ptr;
        PInnerNode curr;

        list_for_each(ptr, &buff->cache_list) {
            curr = list_entry(ptr, _InnerNode, node);

            expected_read_size += NODE_SIZE(curr);

            if (expected_read_size >= size) {
                expected_read_size = size;
                break;
            }
        }
    }

    * target = malloc(expected_read_size);
    D(TAG, "try to read %d bytes from buffer", expected_read_size);

    size_t already_read_size = 0;
    while (already_read_size < expected_read_size) {
        PInnerNode node = list_first_entry_or_null(&buff->cache_list, _InnerNode, node);
        if (!node) break;

        int write_size = min(NODE_SIZE(node), expected_read_size - already_read_size);
        memcpy(*target + already_read_size, CACHE_POS(node) + node->r_offset, write_size);

        node->r_offset += write_size;
        already_read_size += write_size;

        if (node->r_offset == node->w_offset) {
            D(TAG, "Already exhause a node (%p), remove it from list", CACHE_POS(node));
            list_del(&node->node);
            recycle_cache(node->cache);
            free(node);
        }
    }

    return already_read_size;
}

size_t write_into_buffer(PBuffer buffer, void *src, size_t size)
{
    int ret = SUCC;
    CONVERT(buff, buffer);

    LIST_HEAD(tmp_list);
    size_t last_w_offset = -1;

    PInnerNode node = null;
    if (list_empty(&buff->cache_list) 
            || list_last_entry(&buff->cache_list, _InnerNode, node)->w_offset == CACHE_SIZE) {
        node = (PInnerNode) malloc (sizeof(_InnerNode));
        if (!node) {
            E(TAG, "Allocate _InnerNode failed(%d): %s", errno, strerror(errno));
            return -1;
        }
        memset(node, 0, sizeof(_InnerNode));
        node->cache = get_new_cache(CACHE_SIZE);
        if (!node->cache) {
            E(TAG, "Get cache failed");
            free(node);
            ret = -1;
            goto return_func;
        }

        // add the new node to the temporary list first
        list_add_tail(&node->node, &tmp_list);
        D(TAG, "Allocate a new node to cache: %p", CACHE_POS(node));
    } else {
        node = list_last_entry(&buff->cache_list, _InnerNode, node);
        D(TAG, "Reused the old node: %p", CACHE_POS(node));
        last_w_offset = node->w_offset;
    }

    size_t already_read_size = 0;
    while (already_read_size < size) {
        size_t read_size = min(size - already_read_size, CACHE_SIZE - node->w_offset);

        D(TAG, "Try to write %d bytes", read_size);
        D(TAG, "from %p", src + already_read_size);
        D(TAG, "into node: %p, w_offset: %d", CACHE_POS(node) + node->w_offset, node->w_offset);
        memcpy(CACHE_POS(node) + node->w_offset, src + already_read_size, read_size);
        int tmp_read_size = read_size;
        if (tmp_read_size < 0) {
            E(TAG, "Read from fd error: %d", tmp_read_size);
            ret = tmp_read_size;
            goto error_with_read;
        }
        already_read_size += tmp_read_size;
        node->w_offset += tmp_read_size;
        if (already_read_size == size || tmp_read_size < read_size) {
            // already read enough data or no more data to read
            ret = already_read_size;
            break;
        }
        // expected read size == really read size
        // keep reading
        
        node = (PInnerNode) malloc(sizeof(_InnerNode));
        if (!node) {
            E(TAG, "Allocate _InnerNode failed(%d): %s", errno, strerror(errno));
            ret = already_read_size;
            break;
        }
        memset(node, 0, sizeof(_InnerNode));
        node->cache = get_new_cache(CACHE_SIZE);
        if (!node->cache) {
            E(TAG, "Get new cache failed");
            free(node);
            ret = already_read_size;
            break;
        }
        
        list_add_tail(&node->node, &tmp_list);
    }

    if (ret > 0 && !list_empty(&tmp_list)) {
        // read successfully, merge two list
        // add the temporary list to the end of the original list
        list_splice(&tmp_list, buff->cache_list.prev);
    }
    return ret;

error_with_read:
    if (last_w_offset >= 0) {
        // already read part of data into the last node, reset its w_offset
        list_last_entry(&buff->cache_list, _InnerNode, node)->w_offset = last_w_offset;
    }

    // already read part of data into temporary list, recycle the list
    while(!list_empty(&tmp_list)) {
        PInnerNode node = list_last_entry(&tmp_list, _InnerNode, node);
        list_del(&node->node);

        recycle_cache(node->cache);
        free(node);
    }

return_func:
    return ret;
}

size_t simple_index_func(void * ptr, int size, void * arg) 
{
    char c = *((char *) arg);

    char * target = strchr(ptr, c);
    if (!target) {
        return -1;
    } else if (target - (char*) ptr >= size) {
        return target - (char *)ptr;
    }
    return -1;
}

size_t find_in_buffer(PBuffer buffer, size_t start_pos, size_t (*index)(void *, int, void *), void *arg)
{
    size_t node_first_pos = 0;
    size_t node_index_pos = -1;

    if (!index) index = simple_index_func;

    CONVERT(buff, buffer);
    
    PListHead ptr;
    PInnerNode cur;

    list_for_each(ptr, &buff->cache_list) {
        cur = list_entry(ptr, _InnerNode, node);

        void * start_ptr = CACHE_POS(cur);
        if (node_first_pos >= start_pos) {
            // first position of the node is already bigger than start_pos, 
            // need to find char from the node start position in this node
            node_index_pos = index(start_ptr, NODE_SIZE(cur), arg);
            if (node_index_pos >=0) {
                node_first_pos += node_index_pos;
                break;
            }
        } else if (node_first_pos + NODE_SIZE(cur) > start_pos) {
            // the start_pos is in this 
            int start_pos_in_node = start_pos - node_first_pos;
            node_index_pos = index(start_ptr + start_pos_in_node, NODE_SIZE(cur) - start_pos_in_node, arg);
            if (node_index_pos >= 0) {
                node_first_pos += start_pos_in_node + node_index_pos;
                break;
            }
        }

        node_first_pos += NODE_SIZE(cur);
    }

    return (node_index_pos >= 0) ? node_first_pos : -1;
}

size_t buffer_size(PBuffer buffer) 
{
    CONVERT(buff, buffer);
    size_t size = 0;

    if (!list_empty(&buff->cache_list)) {
        PListHead ptr;
        PInnerNode node;

        list_for_each(ptr, &buff->cache_list) {
            node = list_entry(ptr, _InnerNode, node);

            size += NODE_SIZE(node);
        }
    }
    return size;
}

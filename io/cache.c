#include <stdlib.h>
#include <string.h>
#include "common_io.h"

#include "../collections/list.h"

typedef struct cache_node {
    struct list_head node;

    PCache cache;
} CacheNode;

// 64, 128, 256, 512, 1024, 2048
typedef struct specified_size_cache_list {
    struct list_head node;

    int allocated_cache_size;
    struct list_head list;
} SpecifiedSizeCacheList;

typedef CacheNode * PCacheNode;
typedef SpecifiedSizeCacheList * PSpecifiedSizeCacheList;


static LIST_HEAD(all_node);
static LIST_HEAD(available_cache_node);
static int specified_cache_size[] = {64, 128, 256, 512, 1024, 2048};

void make_sure_cache_mm() {
    if (list_empty(&available_cache_node)) {
        for (int index = 0; index < sizeof(specified_cache_size); index ++) {
            PSpecifiedSizeCacheList list = (PSpecifiedSizeCacheList) malloc(sizeof(SpecifiedSizeCacheList));
            list->allocated_cache_size = specified_cache_size[index];
            INIT_LIST_HEAD(&list->list);

            list_add_tail(&list->node, &available_cache_node);
        }
    }
}

PCache get_new_cache(int expected_size) {
    make_sure_cache_mm();

    PSpecifiedSizeCacheList cache_list = NULL;

    struct list_head *ptr;
    PSpecifiedSizeCacheList curr;

    list_for_each(ptr, &available_cache_node) {
        curr = list_entry(ptr, SpecifiedSizeCacheList, node);
        if (curr->allocated_cache_size >= expected_size) {
            cache_list = curr;
            break;
        }
    }

    if (NULL == cache_list) {
        return NULL;
    }
    // return the first one from list
    PCacheNode cache_node = NULL;
    list_for_each(ptr, &cache_list->list) {
        cache_node = list_entry(ptr, CacheNode, node);
        break;
    }

    if (NULL == cache_node) {
        // generate a new one, add to all cache list and return
        cache_node = (PCacheNode) malloc(sizeof(CacheNode));
        list_add_tail(&all_node, &cache_node->node);
        cache_node->cache = (PCache) malloc(sizeof(Cache));
        cache_node->cache->allocated_size = cache_list->allocated_cache_size;
        cache_node->cache->cache_size = expected_size;
        cache_node->cache->buff = malloc(cache_node->cache->allocated_size);
        memset(cache_node->cache->buff, 0, cache_node->cache->allocated_size);
        return cache_node->cache;
    } else {
        // remove from the specified size cache list
        list_del(&cache_node->node);
        free(cache_node);
        cache_node->cache->cache_size = expected_size;
        return cache_node->cache;
    }
}

int recycle_cache(PCache cache) {
    make_sure_cache_mm();

    int allocated_size = cache->allocated_size;
    memset(cache->buff, 0, cache->allocated_size);
    cache->cache_size = 0;

    struct list_head *ptr;
    PSpecifiedSizeCacheList curr = NULL;
    list_for_each(ptr, &available_cache_node) {
        PSpecifiedSizeCacheList tmp = list_entry(ptr, SpecifiedSizeCacheList, node);
        if (curr->allocated_cache_size == allocated_size) {
            curr = tmp;
            break;
        }
    }

    if (NULL == curr) return 0;

    PCacheNode cache_node = (PCacheNode) malloc(sizeof(CacheNode));
    cache_node->cache = cache;
    list_add_tail(&curr->list, &cache_node->node);
    return 1;
}

void release_all_cache() {

}
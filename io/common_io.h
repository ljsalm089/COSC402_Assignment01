#ifndef __COMMON_IO_H__

#define __COMMON_IO_H__

typedef struct _cache {
    int allocated_size;
    int cache_size;
    void * buff;
} Cache;

typedef Cache * PCache;

PCache get_new_cache(int);

int recycle_cache(PCache);

void release_all_cache();

#endif // __COMMON_IO_H__
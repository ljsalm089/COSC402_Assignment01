#ifndef __COMMON_IO_H__
#define __COMMON_IO_H__

typedef struct _cache {
    int allocated_size;
    int cache_size;
    void * buff;
} Cache;

typedef Cache * PCache;

/**
 * get a new cache, may be from cache or newly allocated.
 */
PCache get_new_cache(int);

/**
 * recycle a cache, maybe release or cache it.
 */
int recycle_cache(PCache);

/**
 * release all cache and data used for the cache module,
 * must be the last function in this module called by other module.
 * After this function being called, all PCaches are invalid.
 */
void release_cache_module();

/**
 * initialise the basic data used for the cache module, 
 * must be the first function in this module called by other module.
 */
int initialise_cache_module();

#endif // __COMMON_IO_H__

#ifndef __BUFFER_H__
#define __BUFFER_H__

#include <string.h>
#include <stdlib.h>


typedef struct 
{
    void * pub;
} Buffer;

typedef Buffer * PBuffer;

PBuffer create_new_buffer();

size_t read_into_buffer(int fd, PBuffer buffer, size_t size);

size_t write_from_buffer(int fd, PBuffer buffer, size_t size);

size_t read_from_buffer(PBuffer buffer, void **target, size_t size);

size_t write_into_buffer(PBuffer buffer, void *src, size_t size);

size_t find_in_buffer(PBuffer buffer, size_t start_pos, size_t (*index)(void *, int, void*), void *arg);

int destroy_buffer(PBuffer buffer);

size_t buffer_size(PBuffer buffer);

#endif // __BUFFER_H__

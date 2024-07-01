#ifndef AVUTIL_MEM_H
#define AVUTIL_MEM_H

#include <limits.h>
#include <stdint.h>

void *av_malloc(size_t size);
void *av_mallocz(size_t size);
void *av_malloc_array(size_t nmemb, size_t size);
void *av_mallocz_array(size_t nmemb, size_t size);
void av_freep(void *ptr);

#endif /* AVUTIL_MEM_H */

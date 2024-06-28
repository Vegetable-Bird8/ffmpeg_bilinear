/*
 * default memory allocator for libavutil
 * Copyright (c) 2002 Fabrice Bellard
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file
 * default memory allocator for libavutil
 */

#define _XOPEN_SOURCE 600

// #include "config.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// #include "avutil.h"
// #include "common.h"
#include "mem.h"


#define ALIGN (HAVE_AVX512 ? 64 : (HAVE_AVX ? 32 : 16))

/* NOTE: if you want to override these functions with your own
 * implementations (not recommended) you have to link libav* as
 * dynamic libraries and remove -Wl,-Bsymbolic from the linker flags.
 * Note that this will cost performance. */

static size_t max_alloc_size= INT_MAX;

void *av_malloc(size_t size)
{
    void *ptr = NULL;

    /* let's disallow possibly ambiguous cases */
    if (size > (max_alloc_size - 32))
        return NULL;

    ptr = malloc(size);

    if(!ptr && !size) {
        size = 1;
        ptr= av_malloc(1);
    }
    return ptr;
}

void *av_realloc(void *ptr, size_t size)
{
    /* let's disallow possibly ambiguous cases */
    if (size > (max_alloc_size - 32))
        return NULL;

    return realloc(ptr, size + !size);

}

void *av_malloc_array(size_t nmemb, size_t size)
{
    if (!size || nmemb >= INT_MAX / size)
        return NULL;
    return av_malloc(nmemb * size);
}


void *av_mallocz_array(size_t nmemb, size_t size)
{
    if (!size || nmemb >= INT_MAX / size)
        return NULL;
    return av_mallocz(nmemb * size);
}

void *av_realloc_array(void *ptr, size_t nmemb, size_t size)
{
    if (!size || nmemb >= INT_MAX / size)
        return NULL;
    return av_realloc(ptr, nmemb * size);
}


void av_free(void *ptr)
{
    free(ptr);
}

void av_freep(void *arg)
{
    void *val;

    memcpy(&val, arg, sizeof(val));
    memcpy(arg, &(void *){ NULL }, sizeof(val));
    av_free(val);
}
// 多了一个把内存中的数据全部置0的过程
void *av_mallocz(size_t size)
{
    void *ptr = av_malloc(size);
    if (ptr)
        memset(ptr, 0, size);
    return ptr;
}

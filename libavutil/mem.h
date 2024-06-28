#ifndef AVUTIL_MEM_H
#define AVUTIL_MEM_H

#include <limits.h>
#include <stdint.h>


/**
 * @def DECLARE_ALIGNED(n,t,v)
 * Declare a variable that is aligned in memory.
 *
 * @code{.c}
 * DECLARE_ALIGNED(16, uint16_t, aligned_int) = 42;
 * DECLARE_ALIGNED(32, uint8_t, aligned_array)[128];
 *
 * // The default-alignment equivalent would be
 * uint16_t aligned_int = 42;
 * uint8_t aligned_array[128];
 * @endcode
 *
 * @param n Minimum alignment in bytes
 * @param t Type of the variable (or array element)
 * @param v Name of the variable
 */

/**
 * @def DECLARE_ASM_ALIGNED(n,t,v)
 * Declare an aligned variable appropriate for use in inline assembly code.
 *
 * @code{.c}
 * DECLARE_ASM_ALIGNED(16, uint64_t, pw_08) = UINT64_C(0x0008000800080008);
 * @endcode
 *
 * @param n Minimum alignment in bytes
 * @param t Type of the variable (or array element)
 * @param v Name of the variable
 */

/**
 * @def DECLARE_ASM_CONST(n,t,v)
 * Declare a static constant aligned variable appropriate for use in inline
 * assembly code.
 *
 * @code{.c}
 * DECLARE_ASM_CONST(16, uint64_t, pw_08) = UINT64_C(0x0008000800080008);
 * @endcode
 *
 * @param n Minimum alignment in bytes
 * @param t Type of the variable (or array element)
 * @param v Name of the variable
 */


#if defined(__GNUC__) || defined(__clang__)
    #define DECLARE_ALIGNED(n,t,v)      t __attribute__ ((aligned (n))) v
    #define DECLARE_ASM_ALIGNED(n,t,v)  t av_used __attribute__ ((aligned (n))) v
    #define DECLARE_ASM_CONST(n,t,v)    static const t av_used __attribute__ ((aligned (n))) v
#endif



/**
 * @defgroup lavu_mem_funcs Heap Management
 * Functions responsible for allocating, freeing, and copying memory.
 *
 * All memory allocation functions have a built-in upper limit of `INT_MAX`
 * bytes. This may be changed with av_max_alloc(), although exercise extreme
 * caution when doing so.
 *
 * @{
 */

/**
 * Allocate a memory block with alignment suitable for all memory accesses
 * (including vectors if available on the CPU).
 *
 * @param size Size in bytes for the memory block to be allocated
 * @return Pointer to the allocated block, or `NULL` if the block cannot
 *         be allocated
 * @see av_mallocz()
 */
void *av_malloc(size_t size);

/**
 * Allocate a memory block with alignment suitable for all memory accesses
 * (including vectors if available on the CPU) and zero all the bytes of the
 * block.
 *
 * @param size Size in bytes for the memory block to be allocated
 * @return Pointer to the allocated block, or `NULL` if it cannot be allocated
 * @see av_malloc()
 */
void *av_mallocz(size_t size);

/**
 * Allocate a memory block for an array with av_malloc().
 *
 * The allocated memory will have size `size * nmemb` bytes.
 *
 * @param nmemb Number of element
 * @param size  Size of a single element
 * @return Pointer to the allocated block, or `NULL` if the block cannot
 *         be allocated
 * @see av_malloc()
 */
void *av_malloc_array(size_t nmemb, size_t size);

/**
 * Allocate a memory block for an array with av_mallocz().
 *
 * The allocated memory will have size `size * nmemb` bytes.
 *
 * @param nmemb Number of elements
 * @param size  Size of the single element
 * @return Pointer to the allocated block, or `NULL` if the block cannot
 *         be allocated
 *
 * @see av_mallocz()
 * @see av_malloc_array()
 */
void *av_mallocz_array(size_t nmemb, size_t size);


/**
 * Allocate, reallocate, or free a block of memory.
 *
 * If `ptr` is `NULL` and `size` > 0, allocate a new block. If `size` is
 * zero, free the memory block pointed to by `ptr`. Otherwise, expand or
 * shrink that block of memory according to `size`.
 *
 * @param ptr  Pointer to a memory block already allocated with
 *             av_realloc() or `NULL`
 * @param size Size in bytes of the memory block to be allocated or
 *             reallocated
 *
 * @return Pointer to a newly-reallocated block or `NULL` if the block
 *         cannot be reallocated or the function is used to free the memory block
 *
 * @warning Unlike av_malloc(), the returned pointer is not guaranteed to be
 *          correctly aligned.
 * @see av_fast_realloc()
 * @see av_reallocp()
 */
void *av_realloc(void *ptr, size_t size);

/**
 * Free a memory block which has been allocated with a function of av_malloc()
 * or av_realloc() family.
 *
 * @param ptr Pointer to the memory block which should be freed.
 *
 * @note `ptr = NULL` is explicitly allowed.
 * @note It is recommended that you use av_freep() instead, to prevent leaving
 *       behind dangling pointers.
 * @see av_freep()
 */
void av_free(void *ptr);

/**
 * Free a memory block which has been allocated with a function of av_malloc()
 * or av_realloc() family, and set the pointer pointing to it to `NULL`.
 *
 * @code{.c}
 * uint8_t *buf = av_malloc(16);
 * av_free(buf);
 * // buf now contains a dangling pointer to freed memory, and accidental
 * // dereference of buf will result in a use-after-free, which may be a
 * // security risk.
 *
 * uint8_t *buf = av_malloc(16);
 * av_freep(&buf);
 * // buf is now NULL, and accidental dereference will only result in a
 * // NULL-pointer dereference.
 * @endcode
 *
 * @param ptr Pointer to the pointer to the memory block which should be freed
 * @note `*ptr = NULL` is safe and leads to no action.
 * @see av_free()
 */
void av_freep(void *ptr);


#endif /* AVUTIL_MEM_H */

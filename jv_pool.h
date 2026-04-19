#ifndef _JV_POOL_H_INCLUDED_
#define _JV_POOL_H_INCLUDED_

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JV_OK 0
#define JV_ERROR -1

#define jv_memzero(buf, n) (void *) memset(buf, 0, n)
#define jv_memset(buf, c, n) (void *) memset(buf, c, n)

#define jv_align(d, a) (((d) + (a - 1)) & ~(a - 1))

typedef intptr_t jv_int_t;
typedef uintptr_t jv_uint_t;

typedef unsigned char u_char;

#define JV_POOL_MIN_SIZE 0x4000 /* 0x4000 = 1024 * 16 = 16484 */

#define JV_POOL_DEFAULT_SIZE 0x200000 /* 0x200000  = 1024 * 1024 * 2 = 2097152 */

/**
 *  (2<<31) = 2147483648
 *  (2<<63) = 9223372036854775808s
 **/
#define JV_POOL_MAX_SIZE 1073741568 /*  (2<<30) - 256 = 1073741824 - 256 = 1073741568 */

#define JV_POOL_QUICK_MODE 0

#define JV_POOL_SAFE_MODE 1

typedef struct jv_pool_s jv_pool_t;
typedef struct jv_block_s jv_block_t;
typedef struct jv_lump_s jv_lump_t;

struct jv_block_s {
  jv_block_t *next;
  size_t size;
};

struct jv_lump_s {
  jv_lump_t *prev;
  jv_lump_t *next;
  unsigned size : 31;
  unsigned used : 1;
};

struct jv_pool_s {
  jv_block_t *first;
  jv_block_t *last;
  jv_lump_t *lump;
  jv_lump_t *idle; /* current idle lump's position */
  size_t size;     /* setting default block size */
  uint32_t lump_count;
  unsigned block_count : 31;
  unsigned mode : 1; /* quick mode is 0, safe mode is 1, default is 1 */
};

#define JV_POOL_ALIGNMENT ((size_t) _Alignof(max_align_t))
#define JV_BLOCK_HEADER_SIZE jv_align(sizeof(jv_block_t), JV_POOL_ALIGNMENT)
#define JV_POOL_HEADER_SIZE jv_align(sizeof(jv_pool_t), JV_POOL_ALIGNMENT)
#define JV_LUMP_HEADER_SIZE jv_align(sizeof(jv_lump_t), JV_POOL_ALIGNMENT)
#define JV_WORD_SIZE (JV_POOL_ALIGNMENT * 8)

jv_pool_t *jv_pool_create(size_t size, unsigned mode);

void *jv_pool_alloc(jv_pool_t *pool, size_t size);

void *jv_pool_realloc(jv_pool_t *pool, void *ptr, size_t size);

jv_int_t jv_pool_free(jv_pool_t *pool, void *ptr);

size_t jv_pool_sizeof(jv_pool_t *pool, void *ptr);

jv_int_t jv_pool_exist(jv_pool_t *pool, void *ptr);

jv_int_t jv_pool_recycle(jv_pool_t *pool, void *ptr);

jv_int_t jv_pool_reset(jv_pool_t *pool);

#define jv_pool_each_lump(pool, lump, i) \
  \
for(lump = (pool)->lump, i = 0; i == 0 || lump != (pool)->lump; i++, lump = lump->prev)

#define jv_pool_each_block(pool, block, i) \
  \
for(block = (pool)->first, i = 0; block != NULL; i++, block = block->next)

void jv_pool_destroy(jv_pool_t *pool);

void jv_pool_dump(jv_pool_t *pool, FILE *fd);

#endif /* _JV_POOL_H_INCLUDED_ */

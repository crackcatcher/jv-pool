# High performance and security memory allocation using ANSI C

A modern, high performance, efficient, security memory pool allocation using ANSI C

## Features

- No dependence
- ~400 LOC
- Use efficient algorithms to automatically allocate best fit memory
- Supports memory alloc, recycle, realloc, free, exist, sizeof (get memory size)...
- Support memory pool create, reset,dump, destroy...

## Getting Started

### jv_pool_main.c

```c
#include <assert.h>
#include <jv_pool.h>

int main(int argc, char *argv[]) {
  jv_pool_t *pool;
  jv_block_t *block;
  jv_lump_t *lump;
  jv_uint_t i;
  u_char *a;

  pool = jv_pool_create(JV_POOL_DEFAULT_SIZE, JV_POOL_SAFE_MODE);

  a = jv_pool_alloc(pool, 32);

  jv_memset(a, 'a', 32);

  a[31] = '\0';

  printf("a:%s, len:%lu\n", a, jv_pool_sizeof(pool, a));

  a = jv_pool_realloc(pool, a, 64);

  assert(jv_pool_sizeof(pool, a) == 64);

  jv_memset(a, '1', 64);

  a[63] = '\0';

  printf("a:%s, len:%lu\n", a, jv_pool_sizeof(pool, a));

  jv_pool_dump(pool, stdout);

  jv_pool_each_block(pool, block, i) {
    printf("block address: %p, block->size: %lu\n", (void *) block, (jv_uint_t) block->size);
  }

  jv_pool_each_lump(pool, lump, i) {
    printf("lump address: %p, lump->size: %u, lump->used: %u\n", (void *) lump, lump->size, lump->used);
  }

  assert(jv_pool_free(pool, a) == JV_OK);

  jv_pool_destroy(pool);

  return 0;
}
```

## Build

    $ make

## Run

    $ ./jv_pool_main

## Api References

### jv_pool_t *jv_pool_create(size_t size, unsigned mode);

  `TODO`

### void *jv_pool_alloc(jv_pool_t *pool, size_t size);

  `TODO`

### void *jv_pool_realloc(jv_pool_t *pool, void *ptr, size_t size);

  `TODO`

### jv_int_t jv_pool_free(jv_pool_t *pool, void *ptr);

  `TODO`

### size_t jv_pool_sizeof(jv_pool_t *pool, void *ptr);

  `TODO`

### jv_int_t jv_pool_exist(jv_pool_t *pool, void *ptr);

  `TODO`

### jv_int_t jv_pool_recycle(jv_pool_t *pool, void *ptr);

  `TODO`

### jv_int_t jv_pool_reset(jv_pool_t *pool);

  `TODO`

### jv_pool_each_lump(jv_pool_t *pool, jv_lump_t *lump, jv_uint_t i);

  `TODO`

### jv_pool_each_block(jv_pool_t *pool,jv_block_t *block, jv_uint_t i);

  `TODO`

### void jv_pool_destroy(jv_pool_t *pool);

  `TODO`

### void jv_pool_dump(jv_pool_t *pool, FILE *fd);

  `TODO`

## License

This project is under MIT License. See the [LICENSE](LICENSE) file for the full license text.


# JV Pool

`jv-pool` is a small ANSI C memory-pool library that wraps repeated `malloc/free`
operations behind a reusable pool allocator.

It is suitable for workloads that:

- allocate many small or medium-lived objects from the same logical context
- want one-shot cleanup through `jv_pool_reset()` or `jv_pool_destroy()`
- want allocation metadata such as `exist` and `sizeof`

## Features

- single-header/single-source style API
- aligned allocations based on `max_align_t`
- automatic block growth when the current pool cannot satisfy a request
- dedicated large-block allocation for requests larger than the default block
- `alloc`, `realloc`, `free`, `recycle`, `exist`, `sizeof`, `reset`, `dump`
- frees fully-idle extra blocks on `jv_pool_free()`

## Files

- `jv_pool.h`: public API and data structures
- `jv_pool.c`: allocator implementation
- `jv_pool_main.c`: minimal usage example
- `jv_pool_test.c`: regression tests

## Build

```sh
make
```

Targets produced by the `Makefile`:

- `jv_pool_main`
- `jv_pool_test`

## Run

```sh
./jv_pool_main
./jv_pool_test
```

## Quick Start

```c
#include <assert.h>
#include <stdio.h>
#include <jv_pool.h>

int main(void) {
  jv_pool_t *pool;
  char *buf;

  pool = jv_pool_create(JV_POOL_DEFAULT_SIZE, JV_POOL_SAFE_MODE);
  assert(pool != NULL);

  buf = jv_pool_alloc(pool, 32);
  assert(buf != NULL);

  jv_memset(buf, 'a', 32);
  buf[31] = '\0';

  printf("buf=%s size=%zu\n", buf, jv_pool_sizeof(pool, buf));

  buf = jv_pool_realloc(pool, buf, 64);
  assert(buf != NULL);
  assert(jv_pool_sizeof(pool, buf) == 64);

  jv_pool_dump(pool, stdout);

  assert(jv_pool_free(pool, buf) == JV_OK);
  jv_pool_destroy(pool);
  return 0;
}
```

## How It Works

Each pool starts with one block. A block contains one or more lumps:

- a `block` is one contiguous memory region owned by the pool
- a `lump` is one allocatable fragment inside a block
- small/normal allocations are served from free lumps inside existing blocks
- if no existing lump fits, the pool allocates a new block
- very large allocations may get their own dedicated block

When freeing memory:

- adjacent free lumps are merged
- if an extra block becomes entirely free, that extra block is released
- the first block stays with the pool until `reset` or `destroy`

## Modes

The API exposes two mode constants:

- `JV_POOL_SAFE_MODE`
- `JV_POOL_QUICK_MODE`

Use `JV_POOL_SAFE_MODE` unless you have a compatibility reason not to.
In the current implementation both mode values are accepted, but runtime
validation behavior is effectively the same.

## Important Semantics

- All returned pointers are aligned to `max_align_t`.
- Requested size `0` returns `NULL`.
- If `size < JV_POOL_MIN_SIZE`, `jv_pool_create()` promotes it to
  `JV_POOL_DEFAULT_SIZE`.
- If `size > JV_POOL_MAX_SIZE`, creation/allocation fails.
- `jv_pool_exist()` and `jv_pool_sizeof()` only report live allocations.
  A pointer that has already been freed or recycled is treated as invalid.
- `jv_pool_realloc()` preserves old contents up to `min(old_size, new_size)`.
- If `jv_pool_realloc()` fails, the original pointer remains valid.
- After a successful `jv_pool_realloc()`, the old pointer is invalid.
- `jv_pool_recycle()` only marks a lump unused. It does not merge neighbors and
  does not release now-idle extra blocks.
- `jv_pool_free()` is the normal API for releasing memory. Prefer it over
  `jv_pool_recycle()` unless you explicitly want the lighter-weight behavior.
- The allocator is not thread-safe.
- Any pointer obtained from the pool becomes invalid after `jv_pool_reset()` or
  `jv_pool_destroy()`.

## API Reference

### `jv_pool_t *jv_pool_create(size_t size, unsigned mode);`

Creates a new memory pool.

Parameters:

- `size`: preferred block size for the pool
- `mode`: `JV_POOL_SAFE_MODE` or `JV_POOL_QUICK_MODE`

Behavior:

- if `size < JV_POOL_MIN_SIZE`, the pool uses `JV_POOL_DEFAULT_SIZE`
- if `size > JV_POOL_MAX_SIZE`, creation fails
- the internal block size is aligned automatically

Returns:

- pool pointer on success
- `NULL` on failure

### `void *jv_pool_alloc(jv_pool_t *pool, size_t size);`

Allocates `size` bytes from the pool and zero-fills the returned memory.

Parameters:

- `pool`: target memory pool
- `size`: requested size in bytes

Returns:

- allocation pointer on success
- `NULL` if `pool == NULL`, `size == 0`, size is too large, or allocation fails

Notes:

- returned memory is aligned
- actual stored size may be rounded up to the allocator alignment

### `void *jv_pool_realloc(jv_pool_t *pool, void *ptr, size_t size);`

Changes the size of an existing pool allocation.

Behavior:

- if `ptr == NULL`, this behaves like `jv_pool_alloc(pool, size)`
- if `size == 0`, this frees `ptr` and returns `NULL`
- if reallocation succeeds, old contents are copied to the new region
- if reallocation fails, the original allocation remains valid

Returns:

- new allocation pointer on success
- `NULL` on failure

### `jv_int_t jv_pool_free(jv_pool_t *pool, void *ptr);`

Frees a live pool allocation.

Behavior:

- validates that `ptr` belongs to the pool and is currently allocated
- merges adjacent free lumps
- may release an extra block if that block becomes entirely idle

Returns:

- `JV_OK` on success
- `JV_ERROR` if `pool`/`ptr` is invalid or the pointer is already free

### `size_t jv_pool_sizeof(jv_pool_t *pool, void *ptr);`

Returns the stored size of a live allocation.

Returns:

- allocation size in bytes
- `0` if `ptr` is invalid or no longer live

Notes:

- returned size is the aligned size managed by the pool

### `jv_int_t jv_pool_exist(jv_pool_t *pool, void *ptr);`

Checks whether `ptr` is a live allocation from `pool`.

Returns:

- `JV_OK` if the pointer is currently allocated
- `JV_ERROR` otherwise

### `jv_int_t jv_pool_recycle(jv_pool_t *pool, void *ptr);`

Marks a live allocation as unused without performing full free coalescing.

Returns:

- `JV_OK` on success
- `JV_ERROR` if the pointer is invalid or already not live

Use carefully:

- `recycle` is lighter weight but leaves more fragmentation behind than `free`
- a recycled pointer is no longer considered valid by `exist`/`sizeof`

### `jv_int_t jv_pool_reset(jv_pool_t *pool);`

Resets the pool back to its initial state.

Behavior:

- frees all extra blocks
- restores the first block to one large free lump
- invalidates all pointers previously returned by the pool

Returns:

- `JV_OK` on success
- `JV_ERROR` if `pool == NULL`

### `void jv_pool_destroy(jv_pool_t *pool);`

Destroys the pool and frees all owned blocks.

Behavior:

- safe to call with `NULL`
- invalidates all outstanding pool pointers

### `void jv_pool_dump(jv_pool_t *pool, FILE *fd);`

Prints the current pool layout to `fd`.

Typical output includes:

- block count
- lump count
- each lump address, size, and `used` state
- each block address and size

If `pool == NULL`, the function returns immediately.

### `jv_pool_each_lump(pool, lump, i)`

Iterates over all lumps in the pool.

Usage:

```c
jv_lump_t *lump;
jv_uint_t i;

jv_pool_each_lump(pool, lump, i) {
  printf("lump=%p size=%u used=%u\n", (void *) lump, lump->size, lump->used);
}
```

Notes:

- iteration follows the pool's internal linked-list order
- do not modify pool topology while iterating

### `jv_pool_each_block(pool, block, i)`

Iterates over all blocks owned by the pool.

Usage:

```c
jv_block_t *block;
jv_uint_t i;

jv_pool_each_block(pool, block, i) {
  printf("block=%p size=%zu\n", (void *) block, block->size);
}
```

## Usage Recommendations

- Use `JV_POOL_SAFE_MODE` as the default mode.
- Prefer `jv_pool_free()` over `jv_pool_recycle()`.
- Use `jv_pool_reset()` when a whole request/context ends and all allocations can
  be discarded together.
- Do not pass pool pointers to the system `free()`.
- Do not use pool pointers after `free`, `recycle`, `reset`, `destroy`, or
  successful `realloc`.

## Limitations

- not thread-safe
- prints diagnostic messages directly through `printf`
- pool metadata fields use bit-fields, so the allocator is not intended for
  allocations larger than `JV_POOL_MAX_SIZE`

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE).

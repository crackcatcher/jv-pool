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

#include <assert.h>
#include <jv_pool.h>
#include <string.h>
#include <time.h>
void test1(void) {
  jv_pool_t *pool;
  void *a, *b, *c, *d, *e, *f, *g, *h, *i, *j;
  jv_uint_t base;

  jv_lump_t *lump;
  u_char *s;

  pool = jv_pool_create(128, JV_POOL_SAFE_MODE);

  base = (jv_uint_t) pool - JV_BLOCK_HEADER_SIZE;

  printf("base: %lu, lump size: %lu\n\n", (jv_uint_t) base, (jv_uint_t) JV_LUMP_HEADER_SIZE);
  printf("pool: %lu, pos: %lu, block: %lu, block->size: %lu, block->next: %lu, lump: %lu, lump->size: %lu, lump->next: %lu\n",
         (jv_uint_t) pool - base, (jv_uint_t) pool->idle - base, (jv_uint_t) pool->first - base, (jv_uint_t) pool->first->size,
         (jv_uint_t) pool->first->next, (jv_uint_t) pool->lump - base, (jv_uint_t) pool->lump->size, (jv_uint_t) pool->lump->next - base);

  /* jv_pool_dump(pool, stdout); */

  assert((u_char *) pool->first + JV_BLOCK_HEADER_SIZE == (u_char *) pool);
  assert((u_char *) pool->first + JV_BLOCK_HEADER_SIZE + JV_POOL_HEADER_SIZE == (u_char *) pool->lump);
  assert((u_char *) pool + JV_POOL_HEADER_SIZE == (u_char *) (pool->lump));

  a = jv_pool_alloc(pool, 40);

  printf("pool: %lu, pos: %lu, block: %lu, block->size: %lu, block->next: %lu, lump: %lu, lump->size: %lu, lump->next: %lu\n",
         (jv_uint_t) pool - base, (jv_uint_t) pool->idle - base, (jv_uint_t) pool->first - base, (jv_uint_t) pool->first->size,
         (jv_uint_t) pool->first->next, (jv_uint_t) pool->lump - base, (jv_uint_t) pool->lump->size, (jv_uint_t) pool->lump->next - base);

  printf("a address: %lu, pos: %lu\n", (jv_uint_t) a - base, (jv_uint_t) pool->idle - base);

  assert(jv_pool_sizeof(pool, a) == 40);
  assert((u_char *) a == (u_char *) pool->lump->next + JV_LUMP_HEADER_SIZE);

  b = jv_pool_alloc(pool, 56);

  printf("pool: %lu, pos: %lu, block: %lu, block->size: %lu, block->next: %lu, lump: %lu, lump->size: %lu, lump->next: %lu\n",
         (jv_uint_t) pool - base, (jv_uint_t) pool->idle - base, (jv_uint_t) pool->first - base, (jv_uint_t) pool->first->size,
         (jv_uint_t) pool->first->next, (jv_uint_t) pool->lump - base, (jv_uint_t) pool->lump->size, (jv_uint_t) pool->lump->next - base);

  printf("b address: %lu, pos: %lu\n", (jv_uint_t) b - base, (jv_uint_t) pool->idle - base);

  assert(jv_pool_sizeof(pool, b) == 56);
  assert((u_char *) b == (u_char *) pool->lump->next + JV_LUMP_HEADER_SIZE);

  c = jv_pool_alloc(pool, 32);

  printf("pool: %lu, pos: %lu, block: %lu, block->size: %lu, block->next: %lu, lump: %lu, lump->size: %lu, lump->next: %lu\n",
         (jv_uint_t) pool - base, (jv_uint_t) pool->idle - base, (jv_uint_t) pool->first - base, (jv_uint_t) pool->first->size,
         (jv_uint_t) pool->first->next, (jv_uint_t) pool->lump - base, (jv_uint_t) pool->lump->size, (jv_uint_t) pool->lump->next - base);

  printf("c address: %lu, pos: %lu\n", (jv_uint_t) c - base, (jv_uint_t) pool->idle - base);

  assert(jv_pool_sizeof(pool, c) == 32);
  assert((u_char *) c == (u_char *) pool->lump->next + JV_LUMP_HEADER_SIZE);

  d = jv_pool_alloc(pool, 8);

  printf("pool: %lu, pos: %lu, block: %lu, block->size: %lu, block->next: %lu, lump: %lu, lump->size: %lu, lump->next: %lu\n",
         (jv_uint_t) pool - base, (jv_uint_t) pool->idle - base, (jv_uint_t) pool->first - base, (jv_uint_t) pool->first->size,
         (jv_uint_t) pool->first->next, (jv_uint_t) pool->lump - base, (jv_uint_t) pool->lump->size, (jv_uint_t) pool->lump->next - base);

  printf("d address: %lu, pos: %lu\n", (jv_uint_t) d - base, (jv_uint_t) pool->idle - base);

  assert(jv_pool_sizeof(pool, d) == 8);
  assert((u_char *) d == (u_char *) pool->lump->next + JV_LUMP_HEADER_SIZE);

  e = jv_pool_alloc(pool, 4);

  printf("pool: %lu, pos: %lu, block: %lu, block->size: %lu, block->next: %lu, lump: %lu, lump->size: %lu, lump->next: %lu\n",
         (jv_uint_t) pool - base, (jv_uint_t) pool->idle - base, (jv_uint_t) pool->first - base, (jv_uint_t) pool->first->size,
         (jv_uint_t) pool->first->next, (jv_uint_t) pool->lump - base, (jv_uint_t) pool->lump->size, (jv_uint_t) pool->lump->next - base);

  printf("e address: %lu, pos: %lu\n", (jv_uint_t) e - base, (jv_uint_t) pool->idle - base);

  assert(jv_pool_sizeof(pool, e) == jv_align(4, JV_WORD_SIZE / 8));
  assert((u_char *) e == (u_char *) pool->lump->next + JV_LUMP_HEADER_SIZE);

  f = jv_pool_alloc(pool, 24);

  printf("pool: %lu, pos: %lu, block: %lu, block->size: %lu, block->next: %lu, lump: %lu, lump->size: %lu, lump->next: %lu\n",
         (jv_uint_t) pool - base, (jv_uint_t) pool->idle - base, (jv_uint_t) pool->first - base, (jv_uint_t) pool->first->size,
         (jv_uint_t) pool->first->next, (jv_uint_t) pool->lump - base, (jv_uint_t) pool->lump->size, (jv_uint_t) pool->lump->next - base);

  printf("f address: %lu, pos: %lu\n", (jv_uint_t) f - base, (jv_uint_t) pool->idle - base);

  assert(jv_pool_sizeof(pool, f) == 24);
  assert((u_char *) f == (u_char *) pool->lump->next + JV_LUMP_HEADER_SIZE);

  g = jv_pool_alloc(pool, 104);

  printf("pool: %lu, pos: %lu, block: %lu, block->size: %lu, block->next: %lu, lump: %lu, lump->size: %lu, lump->next: %lu\n",
         (jv_uint_t) pool - base, (jv_uint_t) pool->idle - base, (jv_uint_t) pool->first - base, (jv_uint_t) pool->first->size,
         (jv_uint_t) pool->first->next, (jv_uint_t) pool->lump - base, (jv_uint_t) pool->lump->size, (jv_uint_t) pool->lump->next - base);

  printf("g address: %lu, pos: %lu\n", (jv_uint_t) g - base, (jv_uint_t) pool->idle - base);

  assert(jv_pool_sizeof(pool, g) == 104);
  assert((u_char *) g == (u_char *) pool->lump->next + JV_LUMP_HEADER_SIZE);

  h = jv_pool_alloc(pool, 1); /* 12 */
  assert(jv_pool_sizeof(pool, h) == jv_align(1, JV_WORD_SIZE / 8));

  i = jv_pool_alloc(pool, 310);
  assert(jv_pool_sizeof(pool, i) == jv_align(310, JV_WORD_SIZE / 8));

  j = jv_pool_alloc(pool, 12312);
  assert(jv_pool_sizeof(pool, j) == jv_align(12312, JV_WORD_SIZE / 8));

  jv_pool_dump(pool, stdout);

  assert(jv_pool_free(pool, j) == JV_OK);

  j = jv_pool_alloc(pool, 9000);

  assert(jv_pool_sizeof(pool, j) == jv_align(9000, JV_WORD_SIZE / 8));

  assert(jv_pool_free(pool, h) == JV_OK);

  assert(jv_pool_free(pool, c) == JV_OK);

  assert(jv_pool_free(pool, f) == JV_OK);

  assert(jv_pool_free(pool, a) == JV_OK);

  assert(jv_pool_free(pool, e) == JV_OK);

  assert(jv_pool_free(pool, d) == JV_OK);

  assert(jv_pool_free(pool, b) == JV_OK);

  assert(jv_pool_free(pool, g) == JV_OK);

  assert(jv_pool_free(pool, i) == JV_OK);

  assert(jv_pool_free(pool, j) == JV_OK);

  jv_pool_dump(pool, stdout);

  h = jv_pool_alloc(pool, 36);
  assert(jv_pool_sizeof(pool, h) == jv_align(36, JV_WORD_SIZE / 8));

  assert(jv_pool_free(pool, h) == JV_OK);
  assert(jv_pool_sizeof(pool, h) == 0);
  assert(jv_pool_exist(pool, h) == JV_ERROR);
  assert(jv_pool_free(pool, h) == JV_ERROR);

  s = malloc(4);
  assert(jv_pool_sizeof(pool, s) == 0);
  assert(jv_pool_free(pool, s) == JV_ERROR);
  free(s);

  lump = (jv_lump_t *) ((u_char *) pool->first + JV_BLOCK_HEADER_SIZE + JV_POOL_HEADER_SIZE);

  assert(lump->used == 0);
  assert(lump->size == pool->size);
  assert(pool->block_count == 1);

  jv_pool_destroy(pool);
}

void test2(void) {
  jv_pool_t *pool;
  unsigned i;

  pool = jv_pool_create(1024 * 16, JV_POOL_SAFE_MODE);

  srand(time(NULL));
  for (i = 0; i < 10000; i++) {
    jv_uint_t j = rand() % 1001;
    jv_uint_t k = rand() % 10001;
    size_t size = j * k;
    jv_lump_t *lump = jv_pool_alloc(pool, size);
    /* printf("allocate memory size: %lu\n", j); */
    if (lump == NULL) {
      printf("allocate memory error: %lu\n", (jv_uint_t) size);
      break;
    }
    /*printf("allocate memory size:%lu,  %lu, %lu\n", (jv_uint_t)lump, jv_align(size, JV_WORD_SIZE / 8), (jv_uint_t) jv_pool_sizeof(pool, lump));*/
    /* assert(jv_pool_sizeof(pool, lump) == jv_align(size, JV_WORD_SIZE / 8)); */
    assert(jv_pool_free(pool, lump) == JV_OK);
  }
  /* jv_pool_dump(pool,stdout); */
  jv_pool_destroy(pool);
}

void test3(void) {
  jv_pool_t *pool;
  unsigned i;

  pool = jv_pool_create(1024 * 16, JV_POOL_SAFE_MODE);

  srand(time(NULL));
  for (i = 0; i < 10000; i++) {
    jv_uint_t j = rand() % 1001;
    jv_lump_t *lump = jv_pool_alloc(pool, j);
    /* printf("allocate memory size: %lu\n", j); */
    if (lump == NULL) {
      printf("allocate memory error: %u\n", i);
      break;
    }
    assert(jv_pool_free(pool, lump) == JV_OK);
  }
  /* jv_pool_dump(pool,stdout); */
  jv_pool_destroy(pool);
}

void test4(void) {
  jv_pool_t *pool;
  unsigned i, *s;
  pool = jv_pool_create(1024 * 16, JV_POOL_SAFE_MODE);

  srand(time(NULL));
  for (i = 0; i < 50000; i++) {
    jv_uint_t j = rand() % 1001 + 1;
    assert((s = jv_pool_alloc(pool, j)) != NULL);
    assert(jv_pool_free(pool, s) == JV_OK);
  }
  jv_pool_destroy(pool);
}

void test5(void) {
  jv_pool_t *pool;

  pool = jv_pool_create(1024 * 16, JV_POOL_SAFE_MODE);

  jv_pool_alloc(pool, 432141);
  jv_pool_alloc(pool, 10000);
  jv_pool_alloc(pool, 10000);

  assert(jv_pool_free(pool, jv_pool_alloc(pool, 432141)) == JV_OK);
  assert(jv_pool_free(pool, jv_pool_alloc(pool, 4)) == JV_OK);
  assert(jv_pool_free(pool, jv_pool_alloc(pool, 634)) == JV_OK);
  assert(jv_pool_free(pool, jv_pool_alloc(pool, 10000)) == JV_OK);
  assert(jv_pool_free(pool, jv_pool_alloc(pool, 10000)) == JV_OK);
  assert(jv_pool_free(pool, jv_pool_alloc(pool, 324542)) == JV_OK);
  /*assert(jv_pool_free(pool, jv_pool_alloc(pool, 0x1fffffff)) == JV_OK);*/
  assert(jv_pool_free(pool, jv_pool_alloc(pool, 41233)) == JV_OK);

  /* jv_pool_alloc(pool, 0x1fffffff); */

  /* jv_pool_dump(pool,stdout); */
  jv_pool_destroy(pool);
}

void test6(void) {
  jv_pool_t *pool;
  char *a, *b, *c, *d;

  pool = jv_pool_create(128, JV_POOL_SAFE_MODE);

  /* printf("sizeof(jv_pool_t): %lu\n",sizeof(jv_pool_t)); */
  printf("pool: %lu, pos: %lu, block: %lu, block->size: %lu, block->next: %lu, lump: %lu, lump->size: %lu, lump->next: %lu\n", (jv_uint_t) pool,
         (jv_uint_t) pool->idle, (jv_uint_t) pool->first, (jv_uint_t) pool->first->size, (jv_uint_t) pool->first->next, (jv_uint_t) pool->lump,
         (jv_uint_t) pool->lump->size, (jv_uint_t) pool->lump->next);

  jv_pool_dump(pool, stdout);

  assert((u_char *) pool->first + JV_BLOCK_HEADER_SIZE == (u_char *) pool);
  assert((u_char *) pool->first + JV_BLOCK_HEADER_SIZE + JV_POOL_HEADER_SIZE == (u_char *) pool->lump);
  assert((u_char *) pool + JV_POOL_HEADER_SIZE == (u_char *) (pool->lump));

  a = jv_pool_alloc(pool, 40);

  b = jv_pool_alloc(pool, 56);

  c = jv_pool_alloc(pool, 32);
  memcpy(c, "realloc-check", sizeof("realloc-check"));

  jv_pool_dump(pool, stdout);

  d = jv_pool_realloc(pool, c, 48);

  assert(d != NULL);
  assert(jv_pool_sizeof(pool, d) == 48);
  assert(memcmp(d, "realloc-check", sizeof("realloc-check")) == 0);
  assert(jv_pool_sizeof(pool, c) == 0);

  jv_pool_dump(pool, stdout);

  assert(jv_pool_recycle(pool, a) == JV_OK);

  assert(jv_pool_recycle(pool, b) == JV_OK);

  assert(jv_pool_recycle(pool, c) == JV_ERROR);

  assert(jv_pool_recycle(pool, d) == JV_OK);

  jv_pool_dump(pool, stdout);

  jv_pool_destroy(pool);
}

void test7(void) {
  jv_pool_t *pool;
  jv_lump_t *lump;
  unsigned i;

  pool = jv_pool_create(1024 * 16, JV_POOL_SAFE_MODE);

  srand(time(NULL));
  for (i = 0; i < 1000; i++) {
    jv_uint_t j = rand() % 1001 + 1;
    jv_uint_t k = rand() % 1024 * 18 + 1;
    assert((lump = jv_pool_alloc(pool, j * k)) != NULL);
    /* printf("allocate memory size: %lu\n", j); */
    if (lump == NULL) {
      printf("allocate memory error: %u\n", i);
      break;
    }
  }
  /* jv_pool_dump(pool,stdout); */
  jv_pool_destroy(pool);
}

void test8(void) {
  jv_pool_t *pool;

  pool = jv_pool_create(1024 * 16, JV_POOL_SAFE_MODE);

  jv_pool_dump(pool, stdout);

  assert(jv_pool_reset(pool) == JV_OK);

  jv_pool_alloc(pool, 4432);
  jv_pool_alloc(pool, 412);
  jv_pool_alloc(pool, 3);

  jv_pool_dump(pool, stdout);

  assert(jv_pool_reset(pool) == JV_OK);

  jv_pool_dump(pool, stdout);
  jv_pool_destroy(pool);
}

void test11(void) {
  jv_pool_t *pool;
  jv_pool_t *pool2;
  char *a, *b, *c, *d, *r;
  void *huge;

  pool = jv_pool_create(128, JV_POOL_SAFE_MODE);

  a = jv_pool_alloc(pool, 16);
  b = jv_pool_alloc(pool, 16);
  c = jv_pool_alloc(pool, 16);
  memcpy(a, "keep-me", sizeof("keep-me"));

  assert(jv_pool_free(pool, b) == JV_OK);
  assert(jv_pool_free(pool, b) == JV_ERROR);
  assert(jv_pool_sizeof(pool, b) == 0);
  assert(jv_pool_exist(pool, b) == JV_ERROR);

  r = jv_pool_realloc(pool, a, JV_POOL_MAX_SIZE + 1ULL);
  assert(r == NULL);
  assert(jv_pool_sizeof(pool, a) == 16);
  assert(memcmp(a, "keep-me", sizeof("keep-me")) == 0);

  assert(jv_pool_free(pool, a) == JV_OK);
  assert(jv_pool_free(pool, c) == JV_OK);
  jv_pool_destroy(pool);

  pool = jv_pool_create(128, JV_POOL_SAFE_MODE);
  assert(pool != NULL);
  a = jv_pool_alloc_nz(pool, 16);
  b = jv_pool_alloc_nz(pool, 32);
  d = jv_pool_alloc_nz(pool, 8);
  assert(a != NULL && b != NULL && d != NULL);
  assert(jv_pool_sizeof(pool, b) == 32);
  assert(jv_pool_free(pool, a) == JV_OK);
  r = jv_pool_realloc(pool, b, 40);
  assert(r != NULL);
  assert(jv_pool_sizeof(pool, r) >= jv_align(40, JV_WORD_SIZE / 8));
  assert(jv_pool_free(pool, d) == JV_OK);
  assert(jv_pool_free(pool, r) == JV_OK);
  jv_pool_destroy(pool);

  pool2 = jv_pool_create(1024 * 16, JV_POOL_SAFE_MODE);
  huge = jv_pool_alloc(pool2, 500000);
  assert(huge != NULL);
  assert(pool2->block_count == 2);
  assert(jv_pool_free(pool2, huge) == JV_OK);
  assert(pool2->block_count == 1);
  jv_pool_destroy(pool2);
}

void test9(void) {
  jv_pool_t *pool;

  pool = jv_pool_create(JV_POOL_MIN_SIZE, JV_POOL_SAFE_MODE);

  jv_pool_alloc(pool, 4432);
  jv_pool_alloc(pool, 412);
  jv_pool_alloc(pool, 3);
  jv_pool_alloc(pool, 12);

  jv_pool_dump(pool, stdout);

  assert(jv_pool_reset(pool) == JV_OK);

  jv_pool_dump(pool, stdout);
  jv_pool_destroy(pool);
}

void test10(void) {
  jv_pool_t *pool;

  pool = jv_pool_create(JV_POOL_MAX_SIZE, JV_POOL_SAFE_MODE);

  jv_pool_dump(pool, stdout);

  jv_pool_alloc(pool, 4432);
  jv_pool_alloc(pool, 412);
  jv_pool_alloc(pool, 3);
  jv_pool_alloc(pool, 12);

  /*assert(jv_pool_reset(pool) == JV_OK);*/

  jv_pool_dump(pool, stdout);
  jv_pool_destroy(pool);
}

int main(int argc, char *argv[]) {
  test1();
  test2();
  test3();
  test4();
  test5();
  test6();
  test7();
  test8();
  test9();
  test10();
  test11();
  return 0;
}

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include <jv_pool.h>

#define JV_BENCH_DEFAULT_ITERATIONS 200000UL
#define JV_BENCH_DEFAULT_BATCH_ROUNDS 10UL
#define JV_BENCH_DEFAULT_LIVE_SET 4096UL
#define JV_BENCH_DEFAULT_SEED 123456789U
#define JV_BENCH_MIXED_MAX_SIZE 1024UL

typedef struct jv_bench_result_s jv_bench_result_t;

struct jv_bench_result_s {
  const char *name;
  size_t operations;
  double seconds;
  unsigned long checksum;
};

static double jv_bench_now_sec(void);

static unsigned jv_bench_rand(unsigned *state);

static size_t jv_bench_rand_range(unsigned *state, size_t max_value);

static void jv_bench_touch(unsigned char *ptr, size_t size, unsigned *state, volatile unsigned long *checksum);

static void jv_bench_print_result(jv_bench_result_t result);

static jv_bench_result_t jv_bench_pool_alloc_free(size_t iterations, size_t alloc_size, size_t block_size, unsigned zero_fill);

static jv_bench_result_t jv_bench_libc_alloc_free(size_t iterations, size_t alloc_size, unsigned zero_fill);

static jv_bench_result_t jv_bench_pool_batch_reset(size_t iterations, size_t rounds, size_t alloc_size, size_t block_size, unsigned zero_fill);

static jv_bench_result_t jv_bench_libc_batch_free(size_t iterations, size_t rounds, size_t alloc_size, unsigned zero_fill);

static jv_bench_result_t jv_bench_pool_mixed(size_t iterations, size_t live_set, size_t block_size, unsigned seed);

static jv_bench_result_t jv_bench_libc_mixed(size_t iterations, size_t live_set, unsigned seed);

static void jv_bench_usage(const char *prog_name) {
  fprintf(stderr,
          "usage: %s [iterations] [batch_rounds] [live_set] [seed] [pool_block_size]\n"
          "defaults: iterations=%lu batch_rounds=%lu live_set=%lu seed=%u pool_block_size=%lu\n",
          prog_name,
          (unsigned long) JV_BENCH_DEFAULT_ITERATIONS,
          (unsigned long) JV_BENCH_DEFAULT_BATCH_ROUNDS,
          (unsigned long) JV_BENCH_DEFAULT_LIVE_SET,
          JV_BENCH_DEFAULT_SEED,
          (unsigned long) JV_POOL_DEFAULT_SIZE);
}

static double jv_bench_now_sec(void) {
  struct timeval tv;

  if (gettimeofday(&tv, NULL) != 0) {
    return 0.0;
  }

  return (double) tv.tv_sec + (double) tv.tv_usec / 1000000.0;
}

static unsigned jv_bench_rand(unsigned *state) {
  unsigned value;

  value = *state;
  value ^= value << 13;
  value ^= value >> 17;
  value ^= value << 5;
  *state = value;

  return value;
}

static size_t jv_bench_rand_range(unsigned *state, size_t max_value) {
  if (max_value == 0) {
    return 0;
  }

  return (size_t) (jv_bench_rand(state) % (unsigned) max_value);
}

static void jv_bench_touch(unsigned char *ptr, size_t size, unsigned *state, volatile unsigned long *checksum) {
  size_t i;
  size_t step;

  if (ptr == NULL || size == 0) {
    return;
  }

  step = size / 4;
  if (step == 0) {
    step = 1;
  }

  for (i = 0; i < size; i += step) {
    ptr[i] = (unsigned char) jv_bench_rand(state);
    *checksum += ptr[i];
  }

  ptr[size - 1] = (unsigned char) jv_bench_rand(state);
  *checksum += ptr[size - 1];
}

static void jv_bench_print_result(jv_bench_result_t result) {
  double ns_per_op;
  double ops_per_sec;

  if (result.operations == 0 || result.seconds <= 0.0) {
    ns_per_op = 0.0;
    ops_per_sec = 0.0;
  } else {
    ns_per_op = result.seconds * 1000000000.0 / (double) result.operations;
    ops_per_sec = (double) result.operations / result.seconds;
  }

  printf("%-24s time=%9.6f s  ops=%10lu  ns/op=%10.2f  ops/s=%12.2f  checksum=%lu\n",
         result.name,
         result.seconds,
         (unsigned long) result.operations,
         ns_per_op,
         ops_per_sec,
         result.checksum);
}

static jv_bench_result_t jv_bench_pool_alloc_free(size_t iterations, size_t alloc_size, size_t block_size, unsigned zero_fill) {
  jv_pool_t *pool;
  size_t i;
  double start;
  double end;
  volatile unsigned long checksum;
  unsigned rand_state;
  const char *name;

  pool = jv_pool_create(block_size, JV_POOL_SAFE_MODE);
  assert(pool != NULL);

  checksum = 0;
  rand_state = JV_BENCH_DEFAULT_SEED;
  name = zero_fill ? "pool alloc/free" : "pool alloc_nz/free";

  start = jv_bench_now_sec();
  for (i = 0; i < iterations; i++) {
    unsigned char *ptr;

    ptr = zero_fill ? jv_pool_alloc(pool, alloc_size) : jv_pool_alloc_nz(pool, alloc_size);
    assert(ptr != NULL);
    jv_bench_touch(ptr, alloc_size, &rand_state, &checksum);
    assert(jv_pool_free(pool, ptr) == JV_OK);
  }
  end = jv_bench_now_sec();

  jv_pool_destroy(pool);

  {
    jv_bench_result_t result;

    result.name = name;
    result.operations = iterations;
    result.seconds = end - start;
    result.checksum = checksum;
    return result;
  }
}

static jv_bench_result_t jv_bench_libc_alloc_free(size_t iterations, size_t alloc_size, unsigned zero_fill) {
  size_t i;
  double start;
  double end;
  volatile unsigned long checksum;
  unsigned rand_state;
  const char *name;

  checksum = 0;
  rand_state = JV_BENCH_DEFAULT_SEED;
  name = zero_fill ? "libc calloc/free" : "libc malloc/free";

  start = jv_bench_now_sec();
  for (i = 0; i < iterations; i++) {
    unsigned char *ptr;

    ptr = zero_fill ? calloc(1, alloc_size) : malloc(alloc_size);
    assert(ptr != NULL);
    jv_bench_touch(ptr, alloc_size, &rand_state, &checksum);
    free(ptr);
  }
  end = jv_bench_now_sec();

  {
    jv_bench_result_t result;

    result.name = name;
    result.operations = iterations;
    result.seconds = end - start;
    result.checksum = checksum;
    return result;
  }
}

static jv_bench_result_t jv_bench_pool_batch_reset(size_t iterations, size_t rounds, size_t alloc_size, size_t block_size, unsigned zero_fill) {
  jv_pool_t *pool;
  void **ptrs;
  size_t round;
  size_t i;
  double start;
  double end;
  volatile unsigned long checksum;
  unsigned rand_state;
  const char *name;

  pool = jv_pool_create(block_size, JV_POOL_SAFE_MODE);
  assert(pool != NULL);

  ptrs = malloc(iterations * sizeof(void *));
  assert(ptrs != NULL);

  checksum = 0;
  rand_state = JV_BENCH_DEFAULT_SEED;
  name = zero_fill ? "pool alloc+reset" : "pool alloc_nz+reset";

  start = jv_bench_now_sec();
  for (round = 0; round < rounds; round++) {
    for (i = 0; i < iterations; i++) {
      unsigned char *ptr;

      ptr = zero_fill ? jv_pool_alloc(pool, alloc_size) : jv_pool_alloc_nz(pool, alloc_size);
      assert(ptr != NULL);
      ptrs[i] = ptr;
      jv_bench_touch(ptr, alloc_size, &rand_state, &checksum);
    }

    assert(jv_pool_reset(pool) == JV_OK);
  }
  end = jv_bench_now_sec();

  free(ptrs);
  jv_pool_destroy(pool);

  {
    jv_bench_result_t result;

    result.name = name;
    result.operations = iterations * rounds;
    result.seconds = end - start;
    result.checksum = checksum;
    return result;
  }
}

static jv_bench_result_t jv_bench_libc_batch_free(size_t iterations, size_t rounds, size_t alloc_size, unsigned zero_fill) {
  void **ptrs;
  size_t round;
  size_t i;
  double start;
  double end;
  volatile unsigned long checksum;
  unsigned rand_state;
  const char *name;

  ptrs = malloc(iterations * sizeof(void *));
  assert(ptrs != NULL);

  checksum = 0;
  rand_state = JV_BENCH_DEFAULT_SEED;
  name = zero_fill ? "libc calloc+free" : "libc malloc+free";

  start = jv_bench_now_sec();
  for (round = 0; round < rounds; round++) {
    for (i = 0; i < iterations; i++) {
      unsigned char *ptr;

      ptr = zero_fill ? calloc(1, alloc_size) : malloc(alloc_size);
      assert(ptr != NULL);
      ptrs[i] = ptr;
      jv_bench_touch(ptr, alloc_size, &rand_state, &checksum);
    }

    for (i = 0; i < iterations; i++) {
      free(ptrs[i]);
    }
  }
  end = jv_bench_now_sec();

  free(ptrs);

  {
    jv_bench_result_t result;

    result.name = name;
    result.operations = iterations * rounds;
    result.seconds = end - start;
    result.checksum = checksum;
    return result;
  }
}

static jv_bench_result_t jv_bench_pool_mixed(size_t iterations, size_t live_set, size_t block_size, unsigned seed) {
  jv_pool_t *pool;
  void **slots;
  size_t *sizes;
  size_t rounds;
  size_t round;
  size_t i;
  double start;
  double end;
  volatile unsigned long checksum;
  unsigned rand_state;

  pool = jv_pool_create(block_size, JV_POOL_SAFE_MODE);
  assert(pool != NULL);

  slots = calloc(live_set, sizeof(void *));
  sizes = calloc(live_set, sizeof(size_t));
  assert(slots != NULL);
  assert(sizes != NULL);

  checksum = 0;
  rand_state = seed;
  rounds = iterations / live_set;
  if (rounds == 0) {
    rounds = 1;
  }

  start = jv_bench_now_sec();
  for (round = 0; round < rounds; round++) {
    for (i = 0; i < live_set; i++) {
      size_t size;
      unsigned char *ptr;

      size = jv_bench_rand_range(&rand_state, JV_BENCH_MIXED_MAX_SIZE) + 1;
      ptr = jv_pool_alloc_nz(pool, size);
      assert(ptr != NULL);
      slots[i] = ptr;
      sizes[i] = size;
      jv_bench_touch(ptr, size, &rand_state, &checksum);
    }

    for (i = 1; i < live_set; i += 2) {
      assert(jv_pool_free(pool, slots[i]) == JV_OK);
      slots[i] = NULL;
      sizes[i] = 0;
    }

    for (i = 1; i < live_set; i += 2) {
      size_t size;
      unsigned char *ptr;

      size = jv_bench_rand_range(&rand_state, JV_BENCH_MIXED_MAX_SIZE) + 1;
      ptr = jv_pool_alloc_nz(pool, size);
      assert(ptr != NULL);
      slots[i] = ptr;
      sizes[i] = size;
      jv_bench_touch(ptr, size, &rand_state, &checksum);
    }

    for (i = 0; i < live_set; i++) {
      assert(jv_pool_free(pool, slots[i]) == JV_OK);
      slots[i] = NULL;
      sizes[i] = 0;
    }
  }
  end = jv_bench_now_sec();

  free(slots);
  free(sizes);
  jv_pool_destroy(pool);

  {
    jv_bench_result_t result;

    result.name = "pool fragmentation";
    result.operations = rounds * live_set * 3;
    result.seconds = end - start;
    result.checksum = checksum;
    return result;
  }
}

static jv_bench_result_t jv_bench_libc_mixed(size_t iterations, size_t live_set, unsigned seed) {
  void **slots;
  size_t *sizes;
  size_t rounds;
  size_t round;
  size_t i;
  double start;
  double end;
  volatile unsigned long checksum;
  unsigned rand_state;

  slots = calloc(live_set, sizeof(void *));
  sizes = calloc(live_set, sizeof(size_t));
  assert(slots != NULL);
  assert(sizes != NULL);

  checksum = 0;
  rand_state = seed;
  rounds = iterations / live_set;
  if (rounds == 0) {
    rounds = 1;
  }

  start = jv_bench_now_sec();
  for (round = 0; round < rounds; round++) {
    for (i = 0; i < live_set; i++) {
      size_t size;
      unsigned char *ptr;

      size = jv_bench_rand_range(&rand_state, JV_BENCH_MIXED_MAX_SIZE) + 1;
      ptr = malloc(size);
      assert(ptr != NULL);
      slots[i] = ptr;
      sizes[i] = size;
      jv_bench_touch(ptr, size, &rand_state, &checksum);
    }

    for (i = 1; i < live_set; i += 2) {
      free(slots[i]);
      slots[i] = NULL;
      sizes[i] = 0;
    }

    for (i = 1; i < live_set; i += 2) {
      size_t size;
      unsigned char *ptr;

      size = jv_bench_rand_range(&rand_state, JV_BENCH_MIXED_MAX_SIZE) + 1;
      ptr = malloc(size);
      assert(ptr != NULL);
      slots[i] = ptr;
      sizes[i] = size;
      jv_bench_touch(ptr, size, &rand_state, &checksum);
    }

    for (i = 0; i < live_set; i++) {
      free(slots[i]);
      slots[i] = NULL;
      sizes[i] = 0;
    }
  }
  end = jv_bench_now_sec();

  free(slots);
  free(sizes);

  {
    jv_bench_result_t result;

    result.name = "libc fragmentation";
    result.operations = rounds * live_set * 3;
    result.seconds = end - start;
    result.checksum = checksum;
    return result;
  }
}

int main(int argc, char *argv[]) {
  size_t iterations;
  size_t batch_rounds;
  size_t live_set;
  unsigned seed;
  size_t block_size;
  size_t alloc_size;

  iterations = JV_BENCH_DEFAULT_ITERATIONS;
  batch_rounds = JV_BENCH_DEFAULT_BATCH_ROUNDS;
  live_set = JV_BENCH_DEFAULT_LIVE_SET;
  seed = JV_BENCH_DEFAULT_SEED;
  block_size = JV_POOL_DEFAULT_SIZE;
  alloc_size = 64;

  if (argc > 1) {
    iterations = (size_t) strtoul(argv[1], NULL, 10);
  }
  if (argc > 2) {
    batch_rounds = (size_t) strtoul(argv[2], NULL, 10);
  }
  if (argc > 3) {
    live_set = (size_t) strtoul(argv[3], NULL, 10);
  }
  if (argc > 4) {
    seed = (unsigned) strtoul(argv[4], NULL, 10);
  }
  if (argc > 5) {
    block_size = (size_t) strtoul(argv[5], NULL, 10);
  }
  if (argc > 6) {
    jv_bench_usage(argv[0]);
    return 1;
  }

  if (iterations == 0 || batch_rounds == 0 || live_set == 0 || block_size == 0) {
    jv_bench_usage(argv[0]);
    return 1;
  }

  printf("jv_pool benchmark\n");
  printf("iterations=%lu batch_rounds=%lu live_set=%lu seed=%u pool_block_size=%lu alloc_size=%lu\n\n",
         (unsigned long) iterations,
         (unsigned long) batch_rounds,
         (unsigned long) live_set,
         seed,
         (unsigned long) block_size,
         (unsigned long) alloc_size);

  jv_bench_print_result(jv_bench_pool_alloc_free(iterations, alloc_size, block_size, 1));
  jv_bench_print_result(jv_bench_pool_alloc_free(iterations, alloc_size, block_size, 0));
  jv_bench_print_result(jv_bench_libc_alloc_free(iterations, alloc_size, 1));
  jv_bench_print_result(jv_bench_libc_alloc_free(iterations, alloc_size, 0));

  printf("\n");

  jv_bench_print_result(jv_bench_pool_batch_reset(iterations, batch_rounds, alloc_size, block_size, 1));
  jv_bench_print_result(jv_bench_pool_batch_reset(iterations, batch_rounds, alloc_size, block_size, 0));
  jv_bench_print_result(jv_bench_libc_batch_free(iterations, batch_rounds, alloc_size, 1));
  jv_bench_print_result(jv_bench_libc_batch_free(iterations, batch_rounds, alloc_size, 0));

  printf("\n");

  jv_bench_print_result(jv_bench_pool_mixed(iterations, live_set, block_size, seed));
  jv_bench_print_result(jv_bench_libc_mixed(iterations, live_set, seed));

  return 0;
}

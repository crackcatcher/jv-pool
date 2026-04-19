#include <jv_pool.h>

struct jv_pool_index_entry_s {
  void *ptr;
  jv_lump_t *lump;
};

#define JV_POOL_INDEX_INITIAL_CAPACITY 256U
#define JV_POOL_INDEX_MAX_LOAD_NUM 7U
#define JV_POOL_INDEX_MAX_LOAD_DEN 10U
#define JV_POOL_INDEX_TOMBSTONE ((void *) (uintptr_t) 1)

static jv_lump_t *jv_pool_slb(jv_pool_t *pool, size_t size);

static jv_lump_t *jv_pool_alloc_block(jv_pool_t *pool, size_t size);

static jv_lump_t *jv_pool_alloc_huge(jv_pool_t *pool, size_t size);

static jv_lump_t *jv_pool_find_lump(jv_pool_t *pool, void *ptr, unsigned require_used);

static jv_lump_t *jv_pool_block_lump(jv_block_t *block);

static void *jv_pool_lump_to_ptr(jv_lump_t *lump);

static void jv_pool_release_block(jv_pool_t *pool, jv_block_t *block);

static jv_int_t jv_pool_index_grow(jv_pool_t *pool, size_t min_capacity);

static jv_int_t jv_pool_index_insert(jv_pool_t *pool, void *ptr, jv_lump_t *lump);

static jv_lump_t *jv_pool_index_find(jv_pool_t *pool, void *ptr);

static void jv_pool_index_remove(jv_pool_t *pool, void *ptr);

static void jv_pool_index_clear(jv_pool_t *pool);

static jv_int_t jv_pool_make_lump_free(jv_pool_t *pool, jv_lump_t *lump, unsigned remove_index, unsigned coalesce);

static jv_lump_t *jv_pool_coalesce_lump(jv_pool_t *pool, jv_lump_t *lump);

static void jv_pool_maybe_release_block(jv_pool_t *pool, jv_lump_t *lump);

static void *jv_pool_alloc_impl(jv_pool_t *pool, size_t size, unsigned zero_fill);

static int jv_pool_try_realloc_in_place(jv_pool_t *pool, jv_lump_t *lump, size_t size);

static size_t jv_pool_hash_ptr(void *ptr) {
  uintptr_t value;

  value = (uintptr_t) ptr >> 4;
  value ^= value >> 17;
  value ^= value >> 9;

  return (size_t) value;
}

static size_t jv_pool_next_capacity(size_t capacity) {
  size_t next;

  next = JV_POOL_INDEX_INITIAL_CAPACITY;
  while (next < capacity) {
    next <<= 1;
  }

  return next;
}

static size_t jv_pool_index_slot(jv_pool_index_entry_t *index, size_t capacity, void *ptr, unsigned *found) {
  size_t start;
  size_t i;
  size_t tombstone;

  start = jv_pool_hash_ptr(ptr) & (capacity - 1);
  tombstone = (size_t) -1;

  for (i = 0; i < capacity; i++) {
    size_t pos;
    void *entry_ptr;

    pos = (start + i) & (capacity - 1);
    entry_ptr = index[pos].ptr;

    if (entry_ptr == ptr) {
      *found = 1;
      return pos;
    }

    if (entry_ptr == NULL) {
      *found = 0;
      return tombstone != (size_t) -1 ? tombstone : pos;
    }

    if (entry_ptr == JV_POOL_INDEX_TOMBSTONE && tombstone == (size_t) -1) {
      tombstone = pos;
    }
  }

  *found = 0;
  return tombstone;
}

static jv_int_t jv_pool_index_grow(jv_pool_t *pool, size_t min_capacity) {
  jv_pool_index_entry_t *old_index;
  jv_pool_index_entry_t *new_index;
  size_t old_capacity;
  size_t new_capacity;
  size_t i;

  new_capacity = jv_pool_next_capacity(min_capacity);
  new_index = calloc(new_capacity, sizeof(jv_pool_index_entry_t));
  if (new_index == NULL) {
    return JV_ERROR;
  }

  old_index = pool->index;
  old_capacity = pool->index_capacity;

  pool->index = new_index;
  pool->index_capacity = new_capacity;
  pool->live_count = 0;
  pool->index_tombstones = 0;

  for (i = 0; i < old_capacity; i++) {
    if (old_index[i].ptr != NULL && old_index[i].ptr != JV_POOL_INDEX_TOMBSTONE) {
      if (jv_pool_index_insert(pool, old_index[i].ptr, old_index[i].lump) != JV_OK) {
        free(old_index);
        return JV_ERROR;
      }
    }
  }

  free(old_index);

  return JV_OK;
}

static jv_int_t jv_pool_index_insert(jv_pool_t *pool, void *ptr, jv_lump_t *lump) {
  unsigned found;
  size_t slot;
  size_t active_slots;

  if (pool->index == NULL || pool->index_capacity == 0) {
    if (jv_pool_index_grow(pool, JV_POOL_INDEX_INITIAL_CAPACITY) != JV_OK) {
      return JV_ERROR;
    }
  }

  active_slots = pool->live_count + pool->index_tombstones + 1;
  if (active_slots * JV_POOL_INDEX_MAX_LOAD_DEN >= pool->index_capacity * JV_POOL_INDEX_MAX_LOAD_NUM) {
    if (jv_pool_index_grow(pool, pool->index_capacity << 1) != JV_OK) {
      return JV_ERROR;
    }
  }

  slot = jv_pool_index_slot(pool->index, pool->index_capacity, ptr, &found);
  if (slot == (size_t) -1) {
    if (jv_pool_index_grow(pool, pool->index_capacity << 1) != JV_OK) {
      return JV_ERROR;
    }
    slot = jv_pool_index_slot(pool->index, pool->index_capacity, ptr, &found);
  }

  if (found == 0) {
    if (pool->index[slot].ptr == JV_POOL_INDEX_TOMBSTONE) {
      pool->index_tombstones--;
    }
    pool->live_count++;
  }

  pool->index[slot].ptr = ptr;
  pool->index[slot].lump = lump;

  return JV_OK;
}

static jv_lump_t *jv_pool_index_find(jv_pool_t *pool, void *ptr) {
  unsigned found;
  size_t slot;

  if (pool == NULL || ptr == NULL || pool->index == NULL || pool->index_capacity == 0) {
    return NULL;
  }

  slot = jv_pool_index_slot(pool->index, pool->index_capacity, ptr, &found);
  if (found == 0) {
    return NULL;
  }

  return pool->index[slot].lump;
}

static void jv_pool_index_remove(jv_pool_t *pool, void *ptr) {
  unsigned found;
  size_t slot;

  if (pool == NULL || ptr == NULL || pool->index == NULL || pool->index_capacity == 0) {
    return;
  }

  slot = jv_pool_index_slot(pool->index, pool->index_capacity, ptr, &found);
  if (found == 0) {
    return;
  }

  pool->index[slot].ptr = JV_POOL_INDEX_TOMBSTONE;
  pool->index[slot].lump = NULL;
  pool->live_count--;
  pool->index_tombstones++;
}

static void jv_pool_index_clear(jv_pool_t *pool) {
  if (pool == NULL || pool->index == NULL || pool->index_capacity == 0) {
    return;
  }

  jv_memzero(pool->index, pool->index_capacity * sizeof(jv_pool_index_entry_t));
  pool->live_count = 0;
  pool->index_tombstones = 0;
}

static jv_lump_t *jv_pool_find_lump(jv_pool_t *pool, void *ptr, unsigned require_used) {
  jv_lump_t *lump;

  lump = jv_pool_index_find(pool, ptr);
  if (lump == NULL) {
    return NULL;
  }

  if (require_used == 0 || lump->used == 1) {
    return lump;
  }

  return NULL;
}

static jv_lump_t *jv_pool_block_lump(jv_block_t *block) {
  size_t offset;

  offset = JV_BLOCK_HEADER_SIZE;
  if (block == block->pool->first) {
    offset += JV_POOL_HEADER_SIZE;
  }

  return (jv_lump_t *) ((u_char *) block + offset);
}

static void *jv_pool_lump_to_ptr(jv_lump_t *lump) {
  return (void *) ((u_char *) lump + JV_LUMP_HEADER_SIZE);
}

static void jv_pool_release_block(jv_pool_t *pool, jv_block_t *block) {
  jv_block_t *prev;
  jv_lump_t *lump;

  if (pool == NULL || block == NULL || block == pool->first) {
    return;
  }

  lump = jv_pool_block_lump(block);

  if (pool->lump == lump) {
    pool->lump = jv_pool_block_lump(pool->first);
  }

  if (pool->idle == lump) {
    pool->idle = jv_pool_block_lump(pool->first);
  }

  lump->prev->next = lump->next;
  lump->next->prev = lump->prev;
  pool->lump_count--;

  prev = pool->first;
  while (prev != NULL && prev->next != block) {
    prev = prev->next;
  }

  if (prev == NULL) {
    return;
  }

  prev->next = block->next;
  if (pool->last == block) {
    pool->last = prev;
  }

  pool->block_count--;
  free(block);
}

jv_pool_t *jv_pool_create(size_t size, unsigned mode) {
  jv_pool_t *pool;
  jv_block_t *block;
  jv_lump_t *lump;
  u_char *cp;
  size_t total_size;

  if (size < JV_POOL_MIN_SIZE) {
    size = JV_POOL_DEFAULT_SIZE;
  }

  if (size > JV_POOL_MAX_SIZE) {
    JV_POOL_LOG("jv_pool_create() failed, allow max memory size is %u\n", JV_POOL_MAX_SIZE);
    return (jv_pool_t *) NULL;
  }

  size = jv_align(size, JV_WORD_SIZE / 8);
  total_size = JV_BLOCK_HEADER_SIZE + JV_POOL_HEADER_SIZE + JV_LUMP_HEADER_SIZE + size;

  cp = malloc(total_size);
  if (cp == NULL) {
    JV_POOL_LOG("jv_pool_create() failed, alloc size is %lu\n", size);
    return (jv_pool_t *) NULL;
  }

  block = (jv_block_t *) cp;
  pool = (jv_pool_t *) (cp + JV_BLOCK_HEADER_SIZE);
  lump = (jv_lump_t *) (cp + JV_BLOCK_HEADER_SIZE + JV_POOL_HEADER_SIZE);

  block->next = NULL;
  block->pool = pool;
  block->size = size;

  pool->first = pool->last = block;
  pool->lump = pool->idle = lump;
  pool->size = size;
  pool->live_count = 0;
  pool->index_capacity = 0;
  pool->index_tombstones = 0;
  pool->index = NULL;
  pool->lump_count = 1;
  pool->block_count = 1;
  pool->mode = mode == JV_POOL_QUICK_MODE ? JV_POOL_QUICK_MODE : JV_POOL_SAFE_MODE;

  lump->block = block;
  lump->prev = lump;
  lump->next = lump;
  lump->size = size;
  lump->used = 0;

  if (jv_pool_index_grow(pool, JV_POOL_INDEX_INITIAL_CAPACITY) != JV_OK) {
    free(block);
    return NULL;
  }

  JV_POOL_LOG("create a new memory pool, size is %lu\n", (jv_uint_t) size);

  return pool;
}

static jv_lump_t *jv_pool_slb(jv_pool_t *pool, size_t size) {
  jv_lump_t *p;

  if (pool == NULL) {
    return NULL;
  }

  if (size <= pool->size) {
    p = pool->idle;
    do {
      if (p->used == 0 && p->size <= pool->size && p->size >= size) {
        if (p->size <= size + 2 * JV_LUMP_HEADER_SIZE) {
          p->used = 1;
          if (pool->idle == p) {
            pool->idle = p->next;
          }
          JV_POOL_LOG("alloc memory in lump using fit, size is: %u\n", p->size);
          return p;
        } else {
          jv_lump_t *lump;

          lump = (jv_lump_t *) ((u_char *) p + JV_LUMP_HEADER_SIZE + p->size - JV_LUMP_HEADER_SIZE - size);
          lump->block = p->block;
          lump->size = size;
          lump->used = 1;
          lump->next = p->next;
          p->next = lump;
          lump->next->prev = lump;
          lump->prev = p;

          p->size -= size + JV_LUMP_HEADER_SIZE;
          pool->lump_count++;
          pool->idle = p;

          JV_POOL_LOG("alloc memory in lump using split, size is: %u\n", lump->size);
          return lump;
        }
      }

      p = p->next;
    } while (p != pool->idle);

    return jv_pool_alloc_block(pool, size);
  }

  return jv_pool_alloc_huge(pool, size);
}

static void *jv_pool_alloc_impl(jv_pool_t *pool, size_t size, unsigned zero_fill) {
  jv_lump_t *lump;
  void *ptr;

  if (pool == NULL) {
    return NULL;
  }

  if (size == 0) {
    JV_POOL_LOG("alloc memory must be greater than zero\n");
    return NULL;
  }

  if (size > JV_POOL_MAX_SIZE) {
    JV_POOL_LOG("alloc memory is too huge, allow max memory size is: %u\n", JV_POOL_MAX_SIZE);
    return NULL;
  }

  size = jv_align(size, JV_WORD_SIZE / 8);

  lump = jv_pool_slb(pool, size);
  if (lump == NULL) {
    return NULL;
  }

  ptr = jv_pool_lump_to_ptr(lump);
  if (jv_pool_index_insert(pool, ptr, lump) != JV_OK) {
    (void) jv_pool_make_lump_free(pool, lump, 0, 1);
    return NULL;
  }

  if (zero_fill != 0) {
    jv_memzero(ptr, lump->size);
  }

  return ptr;
}

void *jv_pool_alloc(jv_pool_t *pool, size_t size) {
  return jv_pool_alloc_impl(pool, size, 1);
}

void *jv_pool_alloc_nz(jv_pool_t *pool, size_t size) {
  return jv_pool_alloc_impl(pool, size, 0);
}

size_t jv_pool_sizeof(jv_pool_t *pool, void *ptr) {
  jv_lump_t *lump;

  lump = jv_pool_find_lump(pool, ptr, 1);
  if (lump == NULL) {
    return 0;
  }

  return lump->size;
}

jv_int_t jv_pool_exist(jv_pool_t *pool, void *ptr) {
  if (jv_pool_find_lump(pool, ptr, 1) != NULL) {
    return JV_OK;
  }

  JV_POOL_LOG("ptr not exist in memroy pool\n");
  return JV_ERROR;
}

static jv_lump_t *jv_pool_coalesce_lump(jv_pool_t *pool, jv_lump_t *lump) {
  jv_lump_t *next;
  jv_lump_t *prior;

  if (pool == NULL || lump == NULL) {
    return lump;
  }

  next = lump->next;
  if (next != lump && next->used == 0 && next->block == lump->block &&
      (u_char *) lump + JV_LUMP_HEADER_SIZE + lump->size == (u_char *) next) {
    if (pool->idle == next) {
      pool->idle = lump;
    }

    lump->size += JV_LUMP_HEADER_SIZE + next->size;
    lump->next = next->next;
    lump->next->prev = lump;
    pool->lump_count--;
  }

  prior = lump->prev;
  if (prior != lump && prior->used == 0 && prior->block == lump->block &&
      (u_char *) prior + JV_LUMP_HEADER_SIZE + prior->size == (u_char *) lump) {
    if (pool->idle == lump) {
      pool->idle = prior;
    }

    prior->size += JV_LUMP_HEADER_SIZE + lump->size;
    prior->next = lump->next;
    prior->next->prev = prior;
    pool->lump_count--;
    lump = prior;
  }

  return lump;
}

static void jv_pool_maybe_release_block(jv_pool_t *pool, jv_lump_t *lump) {
  jv_block_t *block;

  if (pool == NULL || lump == NULL) {
    return;
  }

  block = lump->block;
  if (block != NULL && block != pool->first && lump == jv_pool_block_lump(block) && lump->size == block->size) {
    jv_pool_release_block(pool, block);
  }
}

static jv_int_t jv_pool_make_lump_free(jv_pool_t *pool, jv_lump_t *lump, unsigned remove_index, unsigned coalesce) {
  if (pool == NULL || lump == NULL || lump->used == 0) {
    return JV_ERROR;
  }

  if (remove_index != 0) {
    jv_pool_index_remove(pool, jv_pool_lump_to_ptr(lump));
  }

  lump->used = 0;
  pool->idle = lump;

  if (coalesce != 0) {
    lump = jv_pool_coalesce_lump(pool, lump);
    jv_pool_maybe_release_block(pool, lump);
  }

  return JV_OK;
}

static int jv_pool_try_realloc_in_place(jv_pool_t *pool, jv_lump_t *lump, size_t size) {
  if (size <= lump->size) {
    size_t remainder;

    remainder = lump->size - size;
    if (remainder > JV_LUMP_HEADER_SIZE + JV_POOL_ALIGNMENT) {
      jv_lump_t *free_lump;

      free_lump = (jv_lump_t *) ((u_char *) lump + JV_LUMP_HEADER_SIZE + size);
      free_lump->block = lump->block;
      free_lump->used = 0;
      free_lump->size = remainder - JV_LUMP_HEADER_SIZE;
      free_lump->next = lump->next;
      free_lump->prev = lump;
      lump->next->prev = free_lump;
      lump->next = free_lump;
      lump->size = size;
      pool->lump_count++;
      pool->idle = free_lump;
      (void) jv_pool_coalesce_lump(pool, free_lump);
    }

    return 1;
  }

  if (lump->next != lump && lump->next->used == 0 && lump->next->block == lump->block &&
      (u_char *) lump + JV_LUMP_HEADER_SIZE + lump->size == (u_char *) lump->next) {
    jv_lump_t *next;
    size_t combined;

    next = lump->next;
    combined = lump->size + JV_LUMP_HEADER_SIZE + next->size;
    if (combined >= size) {
      size_t remainder;

      if (pool->idle == next) {
        pool->idle = lump;
      }

      remainder = combined - size;
      if (remainder > JV_LUMP_HEADER_SIZE + JV_POOL_ALIGNMENT) {
        jv_lump_t *free_lump;

        free_lump = (jv_lump_t *) ((u_char *) lump + JV_LUMP_HEADER_SIZE + size);
        free_lump->block = lump->block;
        free_lump->used = 0;
        free_lump->size = remainder - JV_LUMP_HEADER_SIZE;
        free_lump->next = next->next;
        free_lump->prev = lump;
        next->next->prev = free_lump;
        lump->next = free_lump;
        lump->size = size;
        pool->idle = free_lump;
      } else {
        lump->size = combined;
        lump->next = next->next;
        lump->next->prev = lump;
        pool->lump_count--;
      }

      return 1;
    }
  }

  return 0;
}

void *jv_pool_realloc(jv_pool_t *pool, void *ptr, size_t size) {
  jv_lump_t *lump;
  void *new_ptr;
  size_t copy_size;

  if (pool == NULL) {
    return NULL;
  }

  if (ptr == NULL) {
    return jv_pool_alloc(pool, size);
  }

  if (size == 0) {
    (void) jv_pool_free(pool, ptr);
    return NULL;
  }

  lump = jv_pool_find_lump(pool, ptr, 1);
  if (lump == NULL) {
    return NULL;
  }

  size = jv_align(size, JV_WORD_SIZE / 8);

  if (jv_pool_try_realloc_in_place(pool, lump, size) != 0) {
    return ptr;
  }

  copy_size = lump->size;
  new_ptr = jv_pool_alloc_nz(pool, size);
  if (new_ptr == NULL) {
    return NULL;
  }

  if (copy_size > size) {
    copy_size = size;
  }

  memcpy(new_ptr, ptr, copy_size);

  if (jv_pool_free(pool, ptr) == JV_OK) {
    if (size > copy_size) {
      jv_memzero((u_char *) new_ptr + copy_size, size - copy_size);
    }
    return new_ptr;
  }

  (void) jv_pool_free(pool, new_ptr);
  return NULL;
}

static jv_lump_t *jv_pool_alloc_block(jv_pool_t *pool, size_t size) {
  jv_block_t *block;
  jv_lump_t *tail;
  u_char *cp;

  cp = malloc(pool->size + JV_BLOCK_HEADER_SIZE + JV_LUMP_HEADER_SIZE);
  if (cp == NULL) {
    JV_POOL_LOG("alloc block memory failed, alloc size is %lu\n", (jv_uint_t) pool->size);
    return NULL;
  }

  block = (jv_block_t *) cp;
  block->next = NULL;
  block->pool = pool;
  block->size = pool->size;

  pool->last->next = block;
  pool->last = block;
  pool->block_count++;
  pool->lump_count++;

  tail = pool->lump->prev;

  if (size > pool->size - 2 * JV_LUMP_HEADER_SIZE) {
    jv_lump_t *lump;

    lump = (jv_lump_t *) (cp + JV_BLOCK_HEADER_SIZE);
    lump->block = block;
    lump->size = pool->size;
    lump->used = 1;
    lump->next = pool->lump;
    tail->next = lump;
    lump->next->prev = lump;
    lump->prev = tail;

    JV_POOL_LOG("alloc a new block with only one lump, size is: %u\n", lump->size);
    return lump;
  }

  {
    jv_lump_t *free_lump;
    jv_lump_t *alloc_lump;

    free_lump = (jv_lump_t *) (cp + JV_BLOCK_HEADER_SIZE);
    free_lump->block = block;
    free_lump->size = pool->size - size - JV_LUMP_HEADER_SIZE;
    free_lump->used = 0;

    alloc_lump = (jv_lump_t *) ((u_char *) free_lump + JV_LUMP_HEADER_SIZE + free_lump->size);
    alloc_lump->block = block;
    alloc_lump->size = size;
    alloc_lump->used = 1;

    alloc_lump->next = pool->lump;
    free_lump->next = alloc_lump;
    tail->next = free_lump;

    alloc_lump->next->prev = alloc_lump;
    alloc_lump->prev = free_lump;
    free_lump->prev = tail;

    pool->idle = free_lump;
    pool->lump_count++;

    JV_POOL_LOG("alloc a new block with two lumps, alloc size is: %u\n", alloc_lump->size);
    return alloc_lump;
  }
}

static jv_lump_t *jv_pool_alloc_huge(jv_pool_t *pool, size_t size) {
  jv_block_t *block;
  jv_lump_t *tail;
  jv_lump_t *lump;
  u_char *cp;

  cp = malloc(size + JV_BLOCK_HEADER_SIZE + JV_LUMP_HEADER_SIZE);
  if (cp == NULL) {
    JV_POOL_LOG("alloc huge memory failed, alloc size is %lu\n", (jv_uint_t) size);
    return NULL;
  }

  block = (jv_block_t *) cp;
  block->next = NULL;
  block->pool = pool;
  block->size = size;

  pool->last->next = block;
  pool->last = block;
  pool->block_count++;
  pool->lump_count++;

  tail = pool->lump->prev;
  lump = (jv_lump_t *) (cp + JV_BLOCK_HEADER_SIZE);
  lump->block = block;
  lump->size = size;
  lump->used = 1;
  lump->next = pool->lump;
  tail->next = lump;
  lump->next->prev = lump;
  lump->prev = tail;

  JV_POOL_LOG("alloc a new huge block with only one lump, size is: %u\n", lump->size);
  return lump;
}

jv_int_t jv_pool_free(jv_pool_t *pool, void *ptr) {
  jv_lump_t *lump;

  lump = jv_pool_find_lump(pool, ptr, 1);
  if (lump == NULL) {
    return JV_ERROR;
  }

  return jv_pool_make_lump_free(pool, lump, 1, 1);
}

jv_int_t jv_pool_recycle(jv_pool_t *pool, void *ptr) {
  jv_lump_t *lump;

  lump = jv_pool_find_lump(pool, ptr, 1);
  if (lump == NULL) {
    return JV_ERROR;
  }

  return jv_pool_make_lump_free(pool, lump, 1, 0);
}

jv_int_t jv_pool_reset(jv_pool_t *pool) {
  jv_block_t *first;
  jv_block_t *block;
  jv_block_t *tmp;
  jv_lump_t *lump;

  if (pool == NULL) {
    return JV_ERROR;
  }

  first = pool->first;
  lump = jv_pool_block_lump(first);

  jv_pool_index_clear(pool);

  for (block = first->next, tmp = NULL; block != NULL; block = tmp) {
    tmp = block->next;
    free(block);
  }

  first->next = NULL;
  first->size = pool->size;

  lump->block = first;
  lump->size = pool->size;
  lump->used = 0;
  lump->next = lump;
  lump->prev = lump;

  pool->first = pool->last = first;
  pool->lump = pool->idle = lump;
  pool->lump_count = 1;
  pool->block_count = 1;

  return JV_OK;
}

void jv_pool_destroy(jv_pool_t *pool) {
  jv_block_t *block;
  jv_block_t *tmp;

  if (pool == NULL) {
    return;
  }

  JV_POOL_LOG("destory a memory pool, size is %lu\n", (jv_uint_t) pool->size);

  free(pool->index);
  pool->index = NULL;

  for (block = pool->first, tmp = NULL; block != NULL; block = tmp) {
    tmp = block->next;
    free(block);
  }
}

void jv_pool_dump(jv_pool_t *pool, FILE *fd) {
  jv_lump_t *lump;
  jv_block_t *block;

  if (pool == NULL) {
    return;
  }

  fprintf(fd, "\n[ pool monitor, block count: %u, lump count: %u, live count: %lu ]\n",
          pool->block_count, pool->lump_count, (unsigned long) pool->live_count);
  fprintf(fd, "lumps: \n");

  lump = pool->lump;
  do {
    fprintf(fd, "\taddress: %-12lu size: %-10lu used: %lu block: %-12lu\n",
            (unsigned long) lump, (unsigned long) lump->size, (unsigned long) lump->used, (unsigned long) lump->block);
    lump = lump->next;
  } while (lump != pool->lump);

  fprintf(fd, "blocks\n");

  for (block = pool->first; block != NULL; block = block->next) {
    fprintf(fd, "\taddress: %-12lu size: %-10lu\n", (unsigned long) block, (unsigned long) block->size);
  }
}

#include <stddef.h>
#include <string.h>

// cell bisecting implementation of malloc
// as described in
// https://github.com/miguelperes/custom-malloc
// block layout: [size][next][prev]

#define SIZE_SIZE_T 4

extern unsigned char __heap_base;
size_t __first_free = (size_t)&__heap_base;
size_t __root_size = 1 << 8;
const size_t __min_block_size = 1 << 4;
int __memory_status = 0;

size_t memory_size(void) {
  return __builtin_wasm_memory_size(0);
}

size_t memory_grow(int delta) {
    return __builtin_wasm_memory_grow(0, delta);
}

void _ensure_root_size(size_t total) {
  size_t size = memory_size()<<16;
  if (total > size) {
    size_t retcode = memory_grow( (total>>16)-(size>>16)+1 );
    printf("%d\n", retcode);
  }
  /*
  size_t size;
  while((size = memory_size() << 16) < total) {
    memory_grow(1);
  }
  */
}

void _link_block(size_t before, size_t after) {
  ((size_t*)before)[1] = after;
  ((size_t*)after)[2] = before;
}

void _remove_block(size_t block) {
  size_t before = ((size_t*)block)[2];
  size_t after = ((size_t*)block)[1];
  _link_block(before, after);
}

size_t _malloc_search_block(size_t n) {
  // search for suitable block
  size_t cur_block = __first_free;
  do{
    size_t cur_block_size = ((size_t*)cur_block)[0];
    size_t next_block = ((size_t*)cur_block)[1];
    if(cur_block_size < n) {
      cur_block = next_block;
      continue;
    }
    // this block is large enough
    // split the block if it's too large
    // but still above the minimum size
    while(cur_block_size > __min_block_size && 2 * n <= cur_block_size) {
      // mitosis
      cur_block_size >>= 1;
      size_t sibling_block = cur_block + cur_block_size;
      ((size_t*)sibling_block)[0] = cur_block_size;
      _link_block(cur_block, sibling_block);
      _link_block(sibling_block, next_block);
      next_block = sibling_block;
    }
    // take the block
    if(__first_free == cur_block) {
      // it's not free now
      // set it to something else
      __first_free = next_block;
      // if it's the only block
      if(__first_free == cur_block) {
        // indicate with a status
        __memory_status = 2;
      }
    }
    _remove_block(cur_block);
    ((size_t*)cur_block)[1] = NULL; // mark used
    return cur_block + SIZE_SIZE_T * 3;
  }while(cur_block != __first_free);
  // failed to find a block
  return NULL;
}

void _append_tail(size_t r) {
  if(__memory_status == 2) {
    // all blocks were used, now we have a block
    __first_free = r;
    _link_block(__first_free, __first_free);
    return;
  }
  // add block back to list
  size_t second_free = ((size_t*)__first_free)[1];
  _link_block(__first_free, r);
  _link_block(r, second_free);
}

void* malloc(size_t n) {
  // word size assumptions
  _Static_assert(sizeof(size_t) == SIZE_SIZE_T);
  // 0 case
  if(n == 0)return NULL;
  // alignment
  n = -((-n) & ~(SIZE_SIZE_T - 1));
  // account for header size
  n += SIZE_SIZE_T * 3;
  // first time: init available memory
  if(__memory_status == 0) {
    __memory_status = 1;
    _ensure_root_size(__root_size);
    ((size_t*)__first_free)[0] = __root_size;
    _link_block(__first_free, __first_free);
  }
  // all memory used: expand
  if(__memory_status == 2) {
    __memory_status = 1;
    _ensure_root_size(__root_size << 1);
    __first_free = ((size_t)&__heap_base) + __root_size;
    ((size_t*)__first_free)[0] = __root_size;
    _link_block(__first_free, __first_free);
    __root_size <<= 1;
  }
  // search for suitable block
  size_t result = _malloc_search_block(n);
  if(result != NULL)return result;
  // memory available, but no blocks large enough: expand
  do {
    _ensure_root_size(__root_size << 1);
    size_t big_block = ((size_t)&__heap_base) + __root_size;
    ((size_t*)big_block)[0] = __root_size;
    _append_tail(big_block);
    __root_size <<= 1;
    printf("%d\n", __root_size);
    if(__root_size >= (1 << 17))return NULL;
  }while((__root_size >> 1) < n);
  // there will definitely be a block available this time
  return _malloc_search_block(n);
}

void *calloc(size_t nmemb, size_t size)
{
	void *mem = malloc(nmemb * size);
	memset(mem, 0, nmemb * size);

	return mem;
}

void free(void* p) {
  const size_t root = (size_t)&__heap_base;
  size_t n;
  // null case
  if (p == NULL) return;
  size_t r=(size_t)p;
  r-=SIZE_SIZE_T*3;
  // try to merge siblings
  size_t r_size = ((size_t*)r)[0];
  while(1) {
    if(r_size >= __root_size) {
      // already the largest
      break;
    }
    size_t sibling_block, sibling_block_next;
    if((r - root + r_size) & r_size) {
      // sibling on right
      sibling_block = r + r_size;
      sibling_block_next = ((size_t*)sibling_block)[1];
      if(sibling_block_next == NULL) {
        // sibling is in use, can't merge further
        break;
      }
      _remove_block(sibling_block);
      ((size_t*)r)[0] = r_size = r_size << 1;
    } else {
      // sibling on left
      sibling_block = r - r_size;
      sibling_block_next = ((size_t*)sibling_block)[1];
      if(sibling_block_next == NULL) {
        // sibling is in use, can't merge further
        break;
      }
      _remove_block(sibling_block);
      r = sibling_block;
      ((size_t*)r)[0] = r_size = r_size << 1;
    }
  }
  // add block back to free list
  _append_tail(r);
}

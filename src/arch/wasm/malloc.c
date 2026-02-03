#include <stddef.h>
#include <string.h>

// cell bisecting implementation of malloc
// as described in
// https://github.com/miguelperes/custom-malloc
// block layout: [size][next][prev]

// about the pad space:
// before this variable was introduced, the game crashes
// (try setting it to 0, see for yourself)
// small values also don't work
// I suspect it has to do with the memory taken by the program itself
// at the time of writing, that is around 160 KB
// so I set the pad space accordingly
// and the game does not crash.
// in the future, if the game is crashing, try setting the pad space higher.
#ifndef MALLOC_PAD_SPACE
#define MALLOC_PAD_SPACE 180000
#endif
#ifndef MALLOC_MIN_BLOCK
#define MALLOC_MIN_BLOCK 4
#endif
#ifndef MALLOC_INITIAL_ROOT
#define MALLOC_INITIAL_ROOT MALLOC_MIN_BLOCK
#endif

#define SIZE_SIZE_T 4
const size_t __min_block_size = 1 << MALLOC_MIN_BLOCK;
const size_t __pad_space = MALLOC_PAD_SPACE;

extern unsigned char __heap_base;
size_t __first_free = (size_t)&__heap_base;
size_t __root_size = 1 << MALLOC_INITIAL_ROOT;
int __memory_status = 0;

size_t memory_size(void) {
  return __builtin_wasm_memory_size(0);
}

size_t memory_grow(int delta) {
    return __builtin_wasm_memory_grow(0, delta);
}

void _ensure_root_size(size_t total) {
  total += __pad_space;
  size_t size = memory_size()<<16;
  if (total > size) {
    // no checking return code. assume it succeeds :)
    memory_grow( (total>>16)-(size>>16)+1 );
  }
}

void _link_block(size_t before, size_t after) {
  ((size_t*)before)[1] = after;
  ((size_t*)after)[2] = before;
}

void _remove_block(size_t block) {
  // fix before and after
  size_t before = ((size_t*)block)[2];
  size_t after = ((size_t*)block)[1];
  _link_block(before, after);
  // fix first
  if(__first_free == block) {
    // set to something else
    __first_free = ((size_t*)block)[1];
    // if it was the only block
    if(__first_free == block) {
      // indicate with a status
      __memory_status = 2;
    }
  }
  // mark used
  ((void**)block)[1] = NULL;
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

void* _malloc_search_block(size_t n) {
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
      ((size_t*)sibling_block)[0] = ((size_t*)cur_block)[0] = cur_block_size;
      _append_tail(sibling_block);
    }
    _remove_block(cur_block);
    return (void*)(cur_block + SIZE_SIZE_T * 3);
  }while(cur_block != __first_free);
  // failed to find a block
  return NULL;
}

void* malloc(size_t n) {
  // word size assumptions
  _Static_assert(sizeof(size_t) == SIZE_SIZE_T, "hardcoded value for sizeof(size_t) is incorrect");
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
  void* result = _malloc_search_block(n);
  if(result != NULL)return result;
  // memory available, but no blocks large enough: expand
  do {
    _ensure_root_size(__root_size << 1);
    size_t big_block = ((size_t)&__heap_base) + __root_size;
    ((size_t*)big_block)[0] = __root_size;
    _append_tail(big_block);
    __root_size <<= 1;
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
    size_t sibling_block;
    size_t is_on_right = (r - root + r_size) & r_size;
    if(is_on_right) {
      // sibling on right
      sibling_block = r + r_size;
    } else {
      // sibling on left
      sibling_block = r - r_size;
    }
    size_t sibling_block_next = ((size_t*)sibling_block)[1];
    if(sibling_block_next == (size_t)NULL) {
      // sibling is in use, can't merge further
      break;
    }
    size_t sibling_block_size = ((size_t*)sibling_block)[0];
    if(sibling_block_size != r_size) {
      // sibling has wrong size (possibly split), can't merge further
      break;
    }
    _remove_block(sibling_block);
    if(!is_on_right) {
      r = sibling_block;
    }
    ((size_t*)r)[0] = r_size = r_size << 1;
  }
  // add block back to free list
  _append_tail(r);
}

size_t total_memory_used_bytes() {
  // each page is 64 KiB
  return memory_size() << 16;
}
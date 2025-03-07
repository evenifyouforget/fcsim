#include <stddef.h>
#include <string.h>

// cell bisecting implementation of malloc
// as described in
// https://github.com/miguelperes/custom-malloc
// block layout: [size][next][prev]

#define SIZE_SIZE_T 4
const size_t __min_block_size = 1 << 4;
const size_t __pad_space = 10000000;

extern unsigned char __heap_base;
size_t __first_free = (size_t)&__heap_base;
size_t __root_size = 1 << 8;
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
  printf("ensure root %d %d\n", size, total);
  if (total > size) {
    size_t retcode = memory_grow( (total>>16)-(size>>16)+1 );
  }
  /*
  size_t size;
  while((size = memory_size() << 16) < total) {
    memory_grow(1);
  }
  */
}

void print_node(size_t cur_block) {
  size_t before = ((size_t*)cur_block)[2];
  size_t after = ((size_t*)cur_block)[1];
  size_t size = ((size_t*)cur_block)[0];
  printf("addr = %d | size = %d | next = %d | prev = %d\n", cur_block, size, after, before);
}

void check_node_integrity(size_t cur_block) {
  size_t before = ((size_t*)cur_block)[2];
  size_t after = ((size_t*)cur_block)[1];
  size_t before_after = ((size_t*)after)[2];
  size_t after_before = ((size_t*)before)[1];
  if(cur_block != before_after || cur_block != after_before) {
    printf("node error! %d\n", 0);
    print_node(cur_block);
    print_node(after);
    print_node(before);
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
  check_node_integrity(before);
  check_node_integrity(after);
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
  ((size_t*)block)[1] = NULL;
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

size_t _malloc_search_block(size_t n) {
  // search for suitable block
  size_t cur_block = __first_free;
  int iter = 0;
  do{
    iter++;if(iter>30){
      printf("iter limit reached, giving up %d\n", 0);
      //return NULL+1;
    }
    size_t cur_block_size = ((size_t*)cur_block)[0];
    size_t next_block = ((size_t*)cur_block)[1];
    check_node_integrity(cur_block);
    check_node_integrity(next_block);
    printf("checking block | addr = %d | size = %d | next = %d | prev = %d | first = %d\n", cur_block, cur_block_size, next_block, ((size_t*)cur_block)[2], __first_free);
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
      printf("splitting block %d %d %d\n", cur_block, sibling_block, cur_block_size);
      check_node_integrity(sibling_block);
      check_node_integrity(cur_block);
      check_node_integrity(__first_free);
    }
    check_node_integrity(__first_free);
    check_node_integrity(cur_block);
    _remove_block(cur_block);
    return cur_block + SIZE_SIZE_T * 3;
  }while(cur_block != __first_free);
  // failed to find a block
  return NULL;
}

void* malloc(size_t n) {
  printf("malloc %d\n", n);
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
  check_node_integrity(__first_free);
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
    printf("root expanded %d\n", __root_size);
    check_node_integrity(__first_free);
    check_node_integrity(big_block);
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
  printf("free %d %d\n", p, r);
  // already free? should never happen
  if(((size_t*)r)[1] != NULL) {
    printf("already freed %d %d\n", r, r+SIZE_SIZE_T*3);
    return;
  }
  // try to merge siblings
  size_t r_size = ((size_t*)r)[0];
  while(1) {
    if(r_size >= __root_size) {
      // already the largest
      break;
    }
    size_t sibling_block, sibling_block_next, sibling_block_size;
    if((r - root + r_size) & r_size) {
      // sibling on right
      sibling_block = r + r_size;
      sibling_block_next = ((size_t*)sibling_block)[1];
      if(sibling_block_next == NULL) {
        // sibling is in use, can't merge further
        break;
      }
      sibling_block_size = ((size_t*)sibling_block)[0];
      if(sibling_block_size != r_size) {
        // sibling has wrong size (possibly split), can't merge further
        break;
      }
      check_node_integrity(sibling_block);
      printf("merge right %d %d %d\n", r, sibling_block, r_size);
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
      sibling_block_size = ((size_t*)sibling_block)[0];
      if(sibling_block_size != r_size) {
        // sibling has wrong size (possibly split), can't merge further
        break;
      }
      check_node_integrity(sibling_block);
      printf("merge left %d %d %d\n", r, sibling_block, r_size);
      _remove_block(sibling_block);
      r = sibling_block;
      ((size_t*)r)[0] = r_size = r_size << 1;
    }
  }
  // add block back to free list
  _append_tail(r);
  check_node_integrity(r);
  check_node_integrity(__first_free);
}

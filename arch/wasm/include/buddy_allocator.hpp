#pragma once
#include <stddef.h>
#include <string.h>

// cell bisecting implementation of malloc
// as described in
// https://github.com/miguelperes/custom-malloc
//
// Block header layout — three size_t words immediately before the pointer
// returned to the caller:
//
//   BLKHDR_SIZE    [ptr-3]  buddy block size (power of 2); valid in both states
//   BLKHDR_NEXT    [ptr-2]  next-free pointer when FREE; 0 when ALLOCATED
//   BLKHDR_PAYLOAD [ptr-1]  prev-free pointer when FREE;
//                           caller's original request size when ALLOCATED
//
// Slot [ptr-1] (BLKHDR_PAYLOAD) is safely reused because:
//   - when a block is on the free list, it holds the prev pointer
//   - when a block is allocated, remove_block sets BLKHDR_NEXT to 0 and
//     malloc immediately writes the request size into BLKHDR_PAYLOAD before
//     returning — so free() can always recover the original size from there

#define BLKHDR_SIZE(b) (((size_t *)(b))[0])
#define BLKHDR_NEXT(b) (((size_t *)(b))[1])
#define BLKHDR_PAYLOAD(b) (((size_t *)(b))[2])

#ifndef MALLOC_MIN_BLOCK
#define MALLOC_MIN_BLOCK 4
#endif
#ifndef MALLOC_INITIAL_ROOT
#define MALLOC_INITIAL_ROOT MALLOC_MIN_BLOCK
#endif

// ASan integration: poison/unpoison the user-visible payload region so that
// use-after-free, double-free, and intra-block overflow are detected.
// The header region [block, block+HDR) is never poisoned — the allocator
// always needs to read it.
// These macros compile to no-ops in non-ASan builds.
#if defined(__has_feature) && __has_feature(address_sanitizer)
#include <sanitizer/asan_interface.h>
#define BUDDY_POISON(addr, sz) __asan_poison_memory_region((addr), (sz))
#define BUDDY_UNPOISON(addr, sz) __asan_unpoison_memory_region((addr), (sz))
#define BUDDY_IS_POISONED(addr) __asan_address_is_poisoned((addr))
#else
#define BUDDY_POISON(addr, sz) ((void)0)
#define BUDDY_UNPOISON(addr, sz) ((void)0)
#define BUDDY_IS_POISONED(addr) (0)
#endif

struct BuddyAllocator {
  unsigned char *heap_base_ptr;
  size_t first_free;
  size_t root_size;
  int status; // 0 = uninitialized, 1 = active, 2 = exhausted

  size_t live_alloc_count_;
  size_t live_useful_bytes_;
  size_t live_block_bytes_;

#ifndef WASM_MEMORY_BACKEND
  size_t total_bytes;
  size_t committed_bytes;
#endif

  // Header overhead in bytes.
  static constexpr size_t HDR = sizeof(size_t) * 3;

  // Minimum block size: must be large enough to hold the 3-word header plus
  // at least one word of payload, and must be a power of 2.
  // MALLOC_MIN_BLOCK specifies the minimum as a power-of-2 exponent; we clamp
  // up to sizeof(size_t)*4 so the invariant holds on both 32-bit and 64-bit.
  static constexpr size_t min_block_size =
      (size_t)(1 << MALLOC_MIN_BLOCK) < sizeof(size_t) * 4
          ? sizeof(size_t) * 4
          : (size_t)(1 << MALLOC_MIN_BLOCK);

  static constexpr size_t initial_root =
      (size_t)(1 << MALLOC_INITIAL_ROOT) < min_block_size
          ? min_block_size
          : (size_t)(1 << MALLOC_INITIAL_ROOT);

  static_assert(HDR == sizeof(size_t) * 3,
      "HDR: three header words (BLKHDR_SIZE, BLKHDR_NEXT, BLKHDR_PAYLOAD)");
  static_assert(min_block_size >= sizeof(size_t) * 4,
      "min_block_size must fit header (3 words) plus at least one payload word");
  static_assert((min_block_size & (min_block_size - 1)) == 0,
      "min_block_size must be a power of 2 for buddy splitting to work");
  static_assert(HDR < min_block_size,
      "header must fit entirely within the minimum block size");
  static_assert(initial_root >= min_block_size,
      "initial_root must be at least min_block_size");
  static_assert((initial_root & (initial_root - 1)) == 0,
      "initial_root must be a power of 2");

  explicit BuddyAllocator(unsigned char *base,
                          [[maybe_unused]] size_t buf_bytes)
      : heap_base_ptr(base), first_free((size_t)base), root_size(initial_root),
        status(0), live_alloc_count_(0), live_useful_bytes_(0),
        live_block_bytes_(0)
#ifndef WASM_MEMORY_BACKEND
        ,
        total_bytes(buf_bytes), committed_bytes(0)
#endif
  {
  }

  // ── platform: memory growth ───────────────────────────────────────────────

  size_t memory_size() const {
#ifdef WASM_MEMORY_BACKEND
    return __builtin_wasm_memory_size(0);
#else
    // Simulate "total pages from address 0 to end of committed region".
    // heap_base_ptr must be aligned to 65536 for this to work correctly
    // with ensure_root_size's arithmetic.
    return ((size_t)heap_base_ptr + committed_bytes) >> 16;
#endif
  }

  size_t memory_grow(int delta) {
#ifdef WASM_MEMORY_BACKEND
    return __builtin_wasm_memory_grow(0, delta);
#else
    size_t old = memory_size();
    committed_bytes += (size_t)delta << 16;
    return old;
#endif
  }

  // ── internals ─────────────────────────────────────────────────────────────

  void ensure_root_size(size_t total) {
    total += (size_t)heap_base_ptr;
    size_t size = memory_size() << 16;
    if (total > size)
      memory_grow((int)((total >> 16) - (size >> 16) + 1));
  }

  void link_block(size_t before, size_t after) {
    BLKHDR_NEXT(before) = after;
    BLKHDR_PAYLOAD(after) = before;
  }

  void remove_block(size_t block) {
    size_t before = BLKHDR_PAYLOAD(block);
    size_t after = BLKHDR_NEXT(block);
    link_block(before, after);
    if (first_free == block) {
      first_free = BLKHDR_NEXT(block);
      if (first_free == block)
        status = 2;
    }
    BLKHDR_NEXT(block) = 0;
  }

  void append_tail(size_t r) {
    if (status == 2) {
      first_free = r;
      link_block(first_free, first_free);
      return;
    }
    size_t second_free = BLKHDR_NEXT(first_free);
    link_block(first_free, r);
    link_block(r, second_free);
  }

  void *search_block(size_t n) {
    size_t cur = first_free;
    do {
      size_t cur_size = BLKHDR_SIZE(cur);
      size_t next = BLKHDR_NEXT(cur);
      if (cur_size < n) {
        cur = next;
        continue;
      }
      // split the block while it's too large but still above minimum size
      while (cur_size > min_block_size && 2 * n <= cur_size) {
        cur_size >>= 1;
        size_t sibling = cur + cur_size;
        // The sibling's header [sibling, sibling+HDR) sits inside the parent
        // block's user payload, which may be poisoned (freed earlier).
        // Unpoison it before the header write; re-poison the sibling's own
        // user payload afterwards.
        BUDDY_UNPOISON((void *)sibling, HDR);
        BLKHDR_SIZE(sibling) = BLKHDR_SIZE(cur) = cur_size;
        append_tail(sibling);
        BUDDY_POISON((void *)(sibling + HDR), cur_size - HDR);
      }
      remove_block(cur);
      return (void *)(cur + HDR);
    } while (cur != first_free);
    return nullptr;
  }

  void track_alloc(void *result, size_t useful_n) {
    size_t block = (size_t)result - HDR;
    BLKHDR_PAYLOAD(block) = useful_n;
    size_t block_payload = BLKHDR_SIZE(block) - HDR;
    BUDDY_UNPOISON(result, useful_n);
    if (block_payload > useful_n)
      BUDDY_POISON((unsigned char *)result + useful_n,
                   block_payload - useful_n);
    live_useful_bytes_ += useful_n;
    live_block_bytes_ += BLKHDR_SIZE(block);
    live_alloc_count_++;
  }

  // ── public API ────────────────────────────────────────────────────────────

  void *malloc(size_t n) {
    if (n == 0)
      return nullptr;
    size_t useful_n = n;
    // align to size_t boundary
    n = -((-n) & ~(sizeof(size_t) - 1));
    // account for header
    n += HDR;
    // first time: init available memory
    if (status == 0) {
      status = 1;
      ensure_root_size(root_size);
      BLKHDR_SIZE(first_free) = root_size;
      link_block(first_free, first_free);
      BUDDY_POISON((void *)(first_free + HDR), root_size - HDR);
    }
    // all memory used: expand
    if (status == 2) {
      status = 1;
      ensure_root_size(root_size << 1);
      first_free = (size_t)heap_base_ptr + root_size;
      BLKHDR_SIZE(first_free) = root_size;
      link_block(first_free, first_free);
      BUDDY_POISON((void *)(first_free + HDR), root_size - HDR);
      root_size <<= 1;
    }
    void *result = search_block(n);
    if (result != nullptr) {
      track_alloc(result, useful_n);
      return result;
    }
    // memory available but no block large enough: expand
    do {
      ensure_root_size(root_size << 1);
      size_t big_block = (size_t)heap_base_ptr + root_size;
      BLKHDR_SIZE(big_block) = root_size;
      append_tail(big_block);
      BUDDY_POISON((void *)(big_block + HDR), root_size - HDR);
      root_size <<= 1;
    } while ((root_size >> 1) < n);
    result = search_block(n);
    if (result != nullptr)
      track_alloc(result, useful_n);
    return result;
  }

  void *calloc(size_t nmemb, size_t size) {
    void *mem = malloc(nmemb * size);
    if (mem)
      memset(mem, 0, nmemb * size);
    return mem;
  }

  void free(void *p) {
    if (p == nullptr)
      return;
    // Double-free guard: if the user region is already poisoned, this pointer
    // was already freed. Trigger an intentional read of the poisoned address
    // so ASan reports it as use-after-free.
    if (BUDDY_IS_POISONED(p)) {
      volatile char bad = *(volatile char *)p;
      (void)bad;
    }
    const size_t root = (size_t)heap_base_ptr;
    size_t r = (size_t)p - HDR;
    live_useful_bytes_ -= BLKHDR_PAYLOAD(r);
    live_block_bytes_ -= BLKHDR_SIZE(r);
    live_alloc_count_--;
    // Poison before relinking so use-after-free on adjacent allocations is
    // detected even during the merge loop below.
    BUDDY_POISON(p, BLKHDR_SIZE(r) - HDR);
    size_t r_size = BLKHDR_SIZE(r);
    while (true) {
      if (r_size >= root_size)
        break;
      size_t is_on_right = (r - root + r_size) & r_size;
      size_t sibling = is_on_right ? r + r_size : r - r_size;
      if (BLKHDR_NEXT(sibling) == 0)
        break;
      if (BLKHDR_SIZE(sibling) != r_size)
        break;
      remove_block(sibling);
      if (!is_on_right)
        r = sibling;
      BLKHDR_SIZE(r) = r_size = r_size << 1;
    }
    append_tail(r);
  }

  // ── counters ──────────────────────────────────────────────────────────────

  size_t total_memory_used_bytes() const { return memory_size() << 16; }
  size_t live_alloc_count_get() const { return live_alloc_count_; }
  size_t live_useful_bytes_get() const { return live_useful_bytes_; }
  size_t live_block_bytes_get() const { return live_block_bytes_; }
};

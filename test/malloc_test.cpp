#include "buddy_allocator.hpp"
#include "test_framework.h"

// Alignment required so ensure_root_size's page-counting arithmetic works:
//   memory_size() = ((size_t)heap_base_ptr + committed_bytes) >> 16
// heap_base_ptr must be a multiple of 65536 so the initial memory_size() equals
// heap_base_ptr >> 16 and grows by exactly 1 per memory_grow(1) call.
static constexpr size_t BUF_ALIGN = 65536;
static constexpr size_t BUF_SZ = 4 * 1024 * 1024;

// Per-test heap buffer aligned to 65536. Using heap allocation (not a static
// array) avoids disrupting ASan's global red-zone layout for other tests.
struct TestBuf {
  void *raw;
  unsigned char *base;
  TestBuf() {
    raw = __builtin_malloc(BUF_SZ + BUF_ALIGN);
    size_t addr = (size_t)raw;
    size_t aligned = (addr + BUF_ALIGN - 1) & ~(BUF_ALIGN - 1);
    base = (unsigned char *)aligned;
    __builtin_memset(base, 0, BUF_SZ);
  }
  ~TestBuf() { __builtin_free(raw); }
};

TEST(MallocTests, BasicAllocFree) {
  TestBuf tb;
  BuddyAllocator a(tb.base, BUF_SZ);
  void *p = a.malloc(16);
  CHECK(p != nullptr);
  CHECK_EQUAL(1u, a.live_alloc_count_get());
  a.free(p);
  CHECK_EQUAL(0u, a.live_alloc_count_get());
}

TEST(MallocTests, MultipleAllocs) {
  TestBuf tb;
  BuddyAllocator a(tb.base, BUF_SZ);
  void *p1 = a.malloc(100);
  void *p2 = a.malloc(200);
  void *p3 = a.malloc(300);
  CHECK_EQUAL(3u, a.live_alloc_count_get());
  a.free(p2);
  CHECK_EQUAL(2u, a.live_alloc_count_get());
  a.free(p1);
  a.free(p3);
  CHECK_EQUAL(0u, a.live_alloc_count_get());
}

TEST(MallocTests, DataSurvivesAlloc) {
  TestBuf tb;
  BuddyAllocator a(tb.base, BUF_SZ);
  int *arr = (int *)a.malloc(10 * sizeof(int));
  CHECK(arr != nullptr);
  for (int i = 0; i < 10; i++)
    arr[i] = i * 7;
  for (int i = 0; i < 10; i++)
    CHECK_EQUAL(i * 7, arr[i]);
  a.free(arr);
}

TEST(MallocTests, CallocZeroes) {
  TestBuf tb;
  BuddyAllocator a(tb.base, BUF_SZ);
  int *p = (int *)a.calloc(8, sizeof(int));
  CHECK(p != nullptr);
  for (int i = 0; i < 8; i++)
    CHECK_EQUAL(0, p[i]);
  a.free(p);
}

TEST(MallocTests, LiveUsefulBytes) {
  TestBuf tb;
  BuddyAllocator a(tb.base, BUF_SZ);
  void *p = a.malloc(100);
  CHECK_EQUAL(100u, a.live_useful_bytes_get());
  a.free(p);
  CHECK_EQUAL(0u, a.live_useful_bytes_get());
}

TEST(MallocTests, BuddyMerge) {
  TestBuf tb;
  BuddyAllocator a(tb.base, BUF_SZ);
  void *ptrs[32];
  for (int i = 0; i < 32; i++)
    ptrs[i] = a.malloc(16);
  for (int i = 0; i < 32; i++)
    a.free(ptrs[i]);
  CHECK_EQUAL(0u, a.live_alloc_count_get());
  CHECK_EQUAL(0u, a.live_useful_bytes_get());
}

TEST(MallocTests, AllocZeroReturnsNull) {
  TestBuf tb;
  BuddyAllocator a(tb.base, BUF_SZ);
  void *p = a.malloc(0);
  CHECK(p == nullptr);
  CHECK_EQUAL(0u, a.live_alloc_count_get());
}

TEST(MallocTests, FreeNullNoOp) {
  TestBuf tb;
  BuddyAllocator a(tb.base, BUF_SZ);
  a.free(nullptr);
  CHECK_EQUAL(0u, a.live_alloc_count_get());
}

TEST(MallocTests, ManySmallAllocs) {
  TestBuf tb;
  BuddyAllocator a(tb.base, BUF_SZ);
  const int N = 256;
  void *ptrs[N];
  for (int i = 0; i < N; i++) {
    ptrs[i] = a.malloc(8);
    CHECK(ptrs[i] != nullptr);
  }
  CHECK_EQUAL((size_t)N, a.live_alloc_count_get());
  for (int i = 0; i < N; i++)
    a.free(ptrs[i]);
  CHECK_EQUAL(0u, a.live_alloc_count_get());
}

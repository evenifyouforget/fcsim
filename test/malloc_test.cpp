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

// ── M5: pointer uniqueness
// ────────────────────────────────────────────────────

TEST(MallocTests, PointerUniqueness) {
  TestBuf tb;
  BuddyAllocator a(tb.base, BUF_SZ);
  void *p1 = a.malloc(16);
  void *p2 = a.malloc(16);
  CHECK(p1 != p2);
  a.free(p1);
  a.free(p2);
}

// ── M4: non-overlapping
// ───────────────────────────────────────────────────────

TEST(MallocTests, NonOverlapping) {
  TestBuf tb;
  BuddyAllocator a(tb.base, BUF_SZ);
  const int N = 16, SZ = 64;
  unsigned char *ptrs[N];
  for (int i = 0; i < N; i++) {
    ptrs[i] = (unsigned char *)a.malloc(SZ);
    CHECK(ptrs[i] != nullptr);
    __builtin_memset(ptrs[i], i + 1, SZ);
  }
  // if any two allocations overlapped, a later fill would corrupt an earlier
  // one
  for (int i = 0; i < N; i++) {
    for (int j = 0; j < SZ; j++)
      CHECK_EQUAL(i + 1, (int)ptrs[i][j]);
    a.free(ptrs[i]);
  }
}

// ── A1: sizeof(size_t) alignment ─────────────────────────────────────────────

TEST(MallocTests, AlignmentSizeT) {
  TestBuf tb;
  BuddyAllocator a(tb.base, BUF_SZ);
  const size_t sizes[] = {1, 3, 7, 8, 15, 16, 31, 100, 200};
  for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
    void *p = a.malloc(sizes[i]);
    CHECK(p != nullptr);
    CHECK_EQUAL(0u, (size_t)p % sizeof(size_t));
    a.free(p);
  }
}

// ── F3: free enables reuse
// ────────────────────────────────────────────────────

TEST(MallocTests, FreeEnablesReuse) {
  TestBuf tb;
  BuddyAllocator a(tb.base, BUF_SZ);
  void *p1 = a.malloc(64);
  CHECK(p1 != nullptr);
  a.free(p1);
  CHECK_EQUAL(0u, a.live_alloc_count_get());
  void *p2 = a.malloc(64);
  CHECK(p2 != nullptr);
  CHECK_EQUAL(1u, a.live_alloc_count_get());
  a.free(p2);
}

// ── C2: calloc with zero arguments ───────────────────────────────────────────

TEST(MallocTests, CallocZeroArgs) {
  TestBuf tb;
  BuddyAllocator a(tb.base, BUF_SZ);
  CHECK(a.calloc(0, sizeof(int)) == nullptr);
  CHECK(a.calloc(4, 0) == nullptr);
  CHECK_EQUAL(0u, a.live_alloc_count_get());
}

// ── S1: large allocation (forces memory_grow)
// ─────────────────────────────────

TEST(MallocTests, LargeAlloc) {
  TestBuf tb;
  BuddyAllocator a(tb.base, BUF_SZ);
  const size_t LARGE = 128 * 1024;
  unsigned char *p = (unsigned char *)a.malloc(LARGE);
  CHECK(p != nullptr);
  for (size_t i = 0; i < LARGE; i += 4096)
    p[i] = (unsigned char)(i & 0xFF);
  for (size_t i = 0; i < LARGE; i += 4096)
    CHECK_EQUAL((int)(i & 0xFF), (int)p[i]);
  a.free(p);
  CHECK_EQUAL(0u, a.live_alloc_count_get());
}

// ── F5: double-free detection (ASan via BUDDY_POISON) ────────────────────────

ASAN_XFAIL_TEST(MallocTests, DoubleFree) {
  TestBuf tb;
  BuddyAllocator a(tb.base, BUF_SZ);
  int *p = (int *)a.malloc(sizeof(int));
  *p = 42;
  a.free(p);
  a.free(p); // triggers read of poisoned region in free()'s double-free guard
}

// ── F6: use-after-free detection (ASan via BUDDY_POISON) ─────────────────────

ASAN_XFAIL_TEST(MallocTests, UseAfterFreeWrite) {
  TestBuf tb;
  BuddyAllocator a(tb.base, BUF_SZ);
  int *p = (int *)a.malloc(sizeof(int));
  a.free(p);
  *p = 99; // write to poisoned region
}

// ── O1: intra-block overflow detection (ASan via BUDDY_POISON padding)
// ────────

ASAN_XFAIL_TEST(MallocTests, BufferOverflowWrite) {
  TestBuf tb;
  BuddyAllocator a(tb.base, BUF_SZ);
  // malloc(1): useful region = 1 byte; buddy block = min_block_size bytes;
  // bytes [ptr+1 .. ptr+block_payload-1] are poisoned padding.
  char *p = (char *)a.malloc(1);
  p[4] = 'x'; // write into poisoned padding
}

// ── TestBuf / BuddyAllocator test-backend self-tests ─────────────────────────
//
// These tests verify the invariants that every MallocTest silently depends on.
// A failure here means the test backend is broken, not the allocator logic.

// TB1: base pointer is BUF_ALIGN-aligned.
// Precondition for ensure_root_size's page-counting arithmetic:
//   memory_size() = ((size_t)heap_base_ptr + committed_bytes) >> 16
// If base is not a multiple of BUF_ALIGN (65536), the initial memory_size()
// != base >> 16 and the page-growth invariant below would not hold.
TEST(TestBufTests, BaseIsAligned) {
  TestBuf tb;
  CHECK_EQUAL(0u, (size_t)tb.base % BUF_ALIGN);
}

// TB2: base memory is zero-initialized.
// BuddyAllocator assumes all block-header words start at zero; a non-zero
// BLKHDR_NEXT in the very first block would confuse the free-list logic.
TEST(TestBufTests, BaseIsZeroed) {
  TestBuf tb;
  for (size_t i = 0; i < BUF_SZ; i += 4096)
    CHECK_EQUAL(0, (int)tb.base[i]);
}

// TB3 + TB4: memory_size() starts at base>>16 and grows by exactly n per
// memory_grow(n) call.  This is the contract that lets ensure_root_size work
// correctly in the non-WASM backend.
TEST(TestBufTests, MemoryGrowIncrements) {
  TestBuf tb;
  BuddyAllocator a(tb.base, BUF_SZ);
  size_t base_pages = (size_t)tb.base >> 16;
  CHECK_EQUAL(base_pages, a.memory_size()); // TB3: initial value
  a.memory_grow(1);
  CHECK_EQUAL(base_pages + 1, a.memory_size()); // TB4: +1 page
  a.memory_grow(3);
  CHECK_EQUAL(base_pages + 4, a.memory_size()); // TB4: +3 more pages
}

// TB5: memory_grow has no bounds check against total_bytes — allocating well
// past BUF_SZ causes the allocator to write a block header past the end of the
// TestBuf heap allocation, which ASan catches as heap-buffer-overflow.
ASAN_XFAIL_TEST(TestBufTests, ExceedBuffer) {
  TestBuf tb;
  BuddyAllocator a(tb.base, BUF_SZ);
  // BUF_SZ * 2 forces memory_grow to "commit" pages beyond the end of tb.raw.
  // The next block-header write lands out-of-bounds → ASan:
  // heap-buffer-overflow.
  void *p = a.malloc(BUF_SZ * 2);
  (void)p;
}

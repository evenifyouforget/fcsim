#include "stl_mock.h"
#include "test_framework.h"

// ── _new ─────────────────────────────────────────────────────────────────────

TEST_GROUP(NewTests){};

TEST(NewTests, DefaultValue) {
  int v;
  _new<int>(&v);
  CHECK_EQUAL(0, v);
}

// ── vector ───────────────────────────────────────────────────────────────────

TEST_GROUP(VectorTests){};

TEST(VectorTests, ConstructDestruct) { std::vector<int> vec; }

TEST(VectorTests, InitialSize) {
  std::vector<int> vec;
  CHECK_EQUAL(0, (int)vec.size());
}

TEST(VectorTests, PushBack) {
  for (int i = 0; i < 100; ++i) {
    std::vector<int> vec;
    for (int j = 0; j < i; ++j)
      vec.push_back(j);
    CHECK_EQUAL(i, (int)vec.size());
  }
}

TEST(VectorTests, Index) {
  for (int i = 0; i < 100; ++i) {
    std::vector<int> vec;
    for (int j = 0; j < i; ++j) {
      vec.push_back(j);
      CHECK_EQUAL(j, vec[j]);
    }
    CHECK_EQUAL(i, (int)vec.size());
  }
}

TEST(VectorTests, Clear) {
  for (int i = 0; i < 100; ++i) {
    std::vector<int> vec;
    for (int j = 0; j < i; ++j)
      vec.push_back(j);
    CHECK_EQUAL(i, (int)vec.size());
    vec.clear();
    for (int j = 0; j < i; ++j) {
      vec.push_back(2 * j);
      CHECK_EQUAL(2 * j, vec[j]);
    }
    CHECK_EQUAL(i, (int)vec.size());
  }
}

TEST(VectorTests, Copy) {
  for (int i = 0; i < 100; ++i) {
    std::vector<std::vector<int>> vec;
    vec.push_back(std::vector<int>());
    for (int j = 0; j < 100; ++j)
      vec[0].push_back(j);
    for (int j = 0; j < i; ++j) {
      vec.push_back(vec[j]);
      vec[j][0] = 2 * j;
    }
    for (int j = 0; j < i; ++j)
      CHECK_EQUAL(2 * j, vec[j][0]);
    CHECK_EQUAL(0, vec[i][0]);
  }
}

TEST(VectorTests, ExplicitCopy) {
  for (int i = 0; i < 100; ++i) {
    std::vector<std::vector<int>> vec;
    vec.push_back(std::vector<int>());
    for (int j = 0; j < 100; ++j)
      vec[0].push_back(j);
    for (int j = 0; j < i; ++j) {
      vec.push_back(std::vector<int>());
      vec[j + 1] = vec[j];
      vec[j][0] = 2 * j;
    }
    for (int j = 0; j < i; ++j)
      CHECK_EQUAL(2 * j, vec[j][0]);
    CHECK_EQUAL(0, vec[i][0]);
  }
}

TEST(VectorTests, ResizeSize) {
  std::vector<int> vec;
  for (int i = 0; i < 100; ++i) {
    vec.resize(i);
    CHECK_EQUAL(i, (int)vec.size());
  }
  for (int i = 100; i >= 0; --i) {
    vec.resize(i);
    CHECK_EQUAL(i, (int)vec.size());
  }
}

TEST(VectorTests, ResizeKeepsData) {
  std::vector<int> vec;
  for (int i = 1; i < 100; ++i) {
    vec.resize(i);
    CHECK_EQUAL(i, (int)vec.size());
    vec[i - 1] = i - 1;
    for (int j = 0; j < i; ++j)
      CHECK_EQUAL(j, vec[j]);
  }
}

TEST(VectorTests, ResizeDefaultElements) {
  int default_value;
  _new<int>(&default_value);
  std::vector<int> vec;
  vec.resize(10);
  for (int i = 0; i < 10; ++i)
    CHECK_EQUAL(default_value, vec[i]);
}

TEST(VectorTests, LargeResize) {
  // jumps from default capacity (8) to 1000 in one call — exposes the
  // _ensure_capacity if-instead-of-while bug: a single doubling is not enough
  std::vector<int> vec;
  vec.resize(1000);
  CHECK_EQUAL(1000, (int)vec.size());
  vec[999] = 42;
  CHECK_EQUAL(42, vec[999]);
}

TEST(VectorTests, ConstSize) {
  // size() must be const so it can be called on a const reference
  std::vector<int> vec;
  vec.push_back(1);
  const std::vector<int> &cvec = vec;
  CHECK_EQUAL(1, (int)cvec.size());
}

// ── string ───────────────────────────────────────────────────────────────────

TEST_GROUP(StringTests){};

TEST(StringTests, ConstructDestruct) { std::string str; }

TEST(StringTests, ToStringData) {
  std::string str = std::to_string(12345);
  for (int i = 0; i < 5; ++i)
    CHECK_EQUAL('0' + i + 1, str._data[i]);
  CHECK_EQUAL(5, (int)str._length);
}

TEST(StringTests, ToStringNegative) {
  std::string str = std::to_string(-12345);
  CHECK_EQUAL('-', str._data[0]);
  for (int i = 0; i < 5; ++i)
    CHECK_EQUAL('0' + i + 1, str._data[i + 1]);
  CHECK_EQUAL(6, (int)str._length);
}

TEST(StringTests, ToStringLargeValue) {
  std::to_string((int64_t)9223372036854000000LL);
  std::to_string((int64_t)-9223372036854000000LL);
}

TEST(StringTests, AssignByStringLiteral) {
  std::string str;
  str = "Hello world";
}

TEST(StringTests, ConstructByStringLiteral) { std::string str = "Hello world"; }

TEST(StringTests, Size) {
  std::string str;
  CHECK_EQUAL(0, (int)str.size());
  for (int i = 1; i < 10; ++i) {
    str.append('0' + i);
    CHECK_EQUAL(i, (int)str.size());
  }
}

TEST(StringTests, ToStringZero) {
  std::string str = std::to_string(0);
  CHECK_EQUAL(1, (int)str.size());
  CHECK_EQUAL('0', str[0]);
}

TEST(StringTests, ToStringSingleDigit) {
  std::string str;
  for (int i = 0; i < 10; ++i) {
    str = std::to_string(i);
    CHECK_EQUAL(1, (int)str.size());
  }
}

TEST(StringTests, ToStringSingleDigitNegative) {
  std::string str;
  for (int i = -9; i < 0; ++i) {
    str = std::to_string(i);
    CHECK_EQUAL(2, (int)str.size());
  }
}

TEST(StringTests, TwoDMemory) {
  for (int i = 0; i < 100; ++i) {
    std::vector<std::string> vec;
    for (int j = 0; j < i; ++j)
      vec.push_back(std::to_string(j));
    vec.clear();
    for (int j = 0; j < i; ++j)
      vec.push_back(std::to_string(2 * j));
  }
}

TEST(StringTests, TwoDMemoryOverwrite) {
  for (int i = 0; i < 100; ++i) {
    std::vector<std::string> vec;
    for (int j = 0; j < i; ++j)
      vec.push_back(std::to_string(j));
    for (int j = 0; j < i; ++j)
      vec[j] = std::to_string(2 * j);
  }
}

// ── unordered_map
// ─────────────────────────────────────────────────────────────

TEST_GROUP(UnorderedMapTests){};

TEST(UnorderedMapTests, EmptyCount) {
  std::unordered_map<int, int> map;
  CHECK_EQUAL(0, (int)map.count(1));
}

TEST(UnorderedMapTests, InsertAndCount) {
  std::unordered_map<int, int> map;
  map.insert(std::make_pair(1, 42));
  CHECK_EQUAL(1, (int)map.count(1));
  CHECK_EQUAL(0, (int)map.count(2));
}

TEST(UnorderedMapTests, AtReturnsCorrectValue) {
  std::unordered_map<int, int> map;
  map.insert(std::make_pair(1, 42));
  CHECK_EQUAL(42, map.at(1));
}

TEST(UnorderedMapTests, InsertDoesNotOverwrite) {
  std::unordered_map<int, int> map;
  map.insert(std::make_pair(1, 42));
  map.insert(std::make_pair(1, 99));
  CHECK_EQUAL(42, map.at(1));
}

TEST(UnorderedMapTests, AtReturnsReference) {
  std::unordered_map<int, int> map;
  map.insert(std::make_pair(1, 42));
  map.at(1) = 100;
  CHECK_EQUAL(100, map.at(1));
}

TEST(UnorderedMapTests, LargerDataTrivial) {
  std::unordered_map<int, int> map;
  for (int i = 0; i < 100; ++i)
    map.insert(std::make_pair(i, i * 2));
  for (int i = 0; i < 100; ++i)
    CHECK_EQUAL(i * 2, map.at(i));
}

TEST(UnorderedMapTests, LargerDataEvenlyDistributed) {
  std::unordered_map<int, int> map;
  int k = 1, v = 1;
  for (int i = 0; i < 100; ++i) {
    k = k * 5 % 107;
    v = v * 3 % 113;
    map.insert(std::make_pair(k, v));
  }
  k = 1;
  v = 1;
  for (int i = 0; i < 100; ++i) {
    k = k * 5 % 107;
    v = v * 3 % 113;
    CHECK_EQUAL(v, map.at(k));
  }
}

TEST(UnorderedMapTests, LargerDataUnevenlyDistributed) {
  std::unordered_map<int, int> map;
  for (int i = 0; i < 250; ++i) {
    unsigned u = (unsigned)i;
    map.insert(std::make_pair((int)(u * u * u * u), i * 2));
  }
  for (int i = 0; i < 250; ++i) {
    unsigned u = (unsigned)i;
    int key = (int)(u * u * u * u);
    CHECK_EQUAL(i * 2, map.at(key));
    CHECK_EQUAL(1, (int)map.count(key));
    CHECK_EQUAL(0, (int)map.count(key + 2));
  }
}

// ─────────────────────────────────────────────────────────────────────────────

// ── xfail demos ──────────────────────────────────────────────────────────────

TEST_GROUP(XfailTests){};

XFAIL_TEST(XfailTests, WrongExpectedValue) {
  // deliberately wrong: 2 + 2 is not 99
  CHECK_EQUAL(99, 2 + 2);
}

XFAIL_TEST(XfailTests, FalseCondition) {
  // deliberately wrong: an empty vector does not have size 99
  std::vector<int> vec;
  CHECK(vec.size() == 99);
}

ASAN_XFAIL_TEST(XfailTests, HeapBufferOverflow) {
  // deliberately wrong: write well past the end of a 4-byte allocation;
  // ASan catches this (a dead read would be optimized away, but a write cannot)
  int *p = (int *)__builtin_malloc(sizeof(int));
  p[16] = 42;
  __builtin_free(p);
}

// ── ASan heap tests
// ───────────────────────────────────────────────────────────

TEST_GROUP(AsanHeapTests){};

ASAN_XFAIL_TEST(AsanHeapTests, BufferOverflowRead) {
  // volatile prevents the dead read from being eliminated by the optimiser
  int *p = (int *)__builtin_malloc(sizeof(int));
  volatile int v = p[4];
  (void)v;
  __builtin_free(p);
}

ASAN_XFAIL_TEST(AsanHeapTests, BufferUnderflow) {
  int *p = (int *)__builtin_malloc(sizeof(int));
  p[-4] = 42;
  __builtin_free(p);
}

ASAN_XFAIL_TEST(AsanHeapTests, UseAfterFreeWrite) {
  int *p = (int *)__builtin_malloc(sizeof(int));
  *p = 42;
  __builtin_free(p);
  *p = 99;
}

ASAN_XFAIL_TEST(AsanHeapTests, UseAfterFreeRead) {
  // volatile prevents the dead read from being eliminated by the optimiser
  int *p = (int *)__builtin_malloc(sizeof(int));
  *p = 42;
  __builtin_free(p);
  volatile int v = *p;
  (void)v;
}

ASAN_XFAIL_TEST(AsanHeapTests, DoubleFree) {
  // the write prevents LLVM from eliminating the malloc+free pair as a no-op
  int *p = (int *)__builtin_malloc(sizeof(int));
  *p = 42;
  __builtin_free(p);
  __builtin_free(p);
}

ASAN_XFAIL_TEST(AsanHeapTests, InvalidFree) {
  // free() of a stack address
  int x = 42;
  __builtin_free((void *)&x);
}

ASAN_XFAIL_TEST(AsanHeapTests, MemoryLeak) {
  // p goes out of scope without being freed; LSan detects it at subprocess exit
  int *p = (int *)__builtin_malloc(sizeof(int) * 64);
  *p = 42;
}

// ── ASan stack tests
// ──────────────────────────────────────────────────────────

// noinline helper for UseAfterReturn: returns a pointer to a local variable.
// The caller then writes through it after the callee's stack frame is gone.
// Note: use-after-scope (escape within the same function's inner block) cannot
// be reliably triggered at -O1 because the optimizer promotes the variable out
// of its block when it detects the pointer escaping — making it
// indistinguishable from a normal stack variable. UseAfterReturn covers the
// closely related pattern via a function boundary instead.
static int *__attribute__((noinline)) _make_local_addr() {
  int x = 42;
  return &x;
}

TEST_GROUP(AsanStackTests){};

ASAN_XFAIL_TEST(AsanStackTests, StackBufferOverflow) {
  // valgrind cannot catch stack overflows; ASan can
  volatile int a[4];
  a[10] = 42;
}

ASAN_XFAIL_TEST(AsanStackTests, StackBufferUnderflow) {
  volatile int a[4];
  volatile int *p = a;
  *(p - 4) = 42;
}

ASAN_XFAIL_TEST(AsanStackTests, UseAfterReturn) {
  // fires on clang 18 without any extra ASAN_OPTIONS flags
  int *p = _make_local_addr();
  *p = 99;
}

// ── ASan global tests
// ─────────────────────────────────────────────────────────

TEST_GROUP(AsanGlobalTests){};

ASAN_XFAIL_TEST(AsanGlobalTests, GlobalBufferOverflow) {
  // valgrind cannot catch global overflows; ASan can
  static int g[4];
  volatile int *p = g;
  p[10] = 42;
}

// ── UBSan tests
// ───────────────────────────────────────────────────────────────

TEST_GROUP(UbsanTests){};

XFAIL_TEST(UbsanTests, SignedIntegerOverflow) {
  volatile int x = __INT_MAX__;
  volatile int y = x + 1;
  (void)y;
}

XFAIL_TEST(UbsanTests, DivideByZero) {
  volatile int x = 1;
  volatile int y = 0;
  volatile int z = x / y;
  (void)z;
}

XFAIL_TEST(UbsanTests, ShiftPastBitwidth) {
  volatile int x = 1;
  volatile int y = x << 32;
  (void)y;
}

XFAIL_TEST(UbsanTests, NullPointerDereference) {
  int *p = nullptr;
  volatile int v = *p;
  (void)v;
}

ASAN_XFAIL_TEST(UbsanTests, MisalignedAccess) {
  // x86 tolerates misaligned reads in hardware; UBSan surfaces the UB.
  // MSan's instrumentation suppresses UBSan's alignment check, so this
  // only fires under the ASan build.
  char buf[sizeof(int) + 1] = {};
  int *p = (int *)(buf + 1);
  volatile int v = *p;
  (void)v;
}

XFAIL_TEST(UbsanTests, PointerOverflow) {
  // p - PTRDIFF_MAX underflows for any small heap address:
  // result > p even though we subtracted a positive offset
  char *p = (char *)__builtin_malloc(1);
  char *volatile q = p - __PTRDIFF_MAX__;
  (void)q;
  __builtin_free(p);
}

// ── MSan uninitialized read tests ────────────────────────────────────────────
// These only fire under MSan. ASan does not detect uninitialized reads.

TEST_GROUP(MsanTests){};

// At -O1, MSan's shadow checks survive only when the uninitialized value flows
// into a genuinely observable side effect. volatile+(void) is eliminated;
// passing the value to printf is not. All MSan tests use __builtin_printf to
// force the uninitialized value into an observable argument.

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"

MSAN_XFAIL_TEST(MsanTests, UninitStack) {
  int x;
  __builtin_printf("%d\n", x);
}

MSAN_XFAIL_TEST(MsanTests, UninitPartialStruct) {
  // MSan tracks initialization per byte: writing field a does not mark field b
  struct {
    int a;
    int b;
  } s;
  s.a = 42;
  __builtin_printf("%d\n", s.b);
}

MSAN_XFAIL_TEST(MsanTests, UninitBranchCondition) {
  // classic MSan pattern: uninitialized value controls a branch
  int x;
  if (x)
    __builtin_printf("a\n");
  else
    __builtin_printf("b\n");
}

#pragma clang diagnostic pop

MSAN_XFAIL_TEST(MsanTests, UninitHeap) {
  // malloc does not initialize memory; calloc does
  int *p = (int *)__builtin_malloc(sizeof(int));
  __builtin_printf("%d\n", *p);
  __builtin_free(p);
}

TEST(MsanTests, CallocIsClean) {
  // calloc zero-initializes: MSan should NOT fire here; this is a negative
  // case verifying MSan distinguishes calloc from malloc
  int *p = (int *)__builtin_calloc(1, sizeof(int));
  __builtin_printf("%d\n", *p);
  __builtin_free(p);
}

// ─────────────────────────────────────────────────────────────────────────────

int main(int argc, char **argv) {
  if (argc == 2 && __builtin_strcmp(argv[1], "--list") == 0)
    return list_tests();
  if (argc == 2 && __builtin_strcmp(argv[1], "--list-xfail") == 0)
    return list_xfail_tests();
  if (argc == 2 && __builtin_strcmp(argv[1], "--list-skip") == 0)
    return list_skip_tests();
  if (argc == 3 && __builtin_strcmp(argv[1], "--run") == 0)
    return run_single_test(argv[2]);
  return run_all_tests();
}

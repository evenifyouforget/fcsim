#pragma once
#include <cstdio>

// ─── internals ───────────────────────────────────────────────────────────────

namespace _testfw {

enum class Expected { Pass, Fail, Skip };

struct Entry {
  const char *name;
  void (*fn)();
  Entry *next;
  Expected asan_expected;
  Expected msan_expected;
};

static Entry *list_head = nullptr;
static int passed = 0;
static int failed = 0;
static bool test_ok = true;

struct Reg {
  Entry e;
  Reg(const char *name, void (*fn)(), Expected asan = Expected::Pass,
      Expected msan = Expected::Pass) {
    e.name = name;
    e.fn = fn;
    e.asan_expected = asan;
    e.msan_expected = msan;
    e.next = list_head;
    list_head = &e;
  }
};

inline Expected current_expected(const Entry *e) {
#if defined(SANITIZER_MSAN)
  return e->msan_expected;
#else
  return e->asan_expected;
#endif
}

inline void fail_at(const char *file, int line, const char *expr) {
  printf("    [%s:%d] %s\n", file, line, expr);
  test_ok = false;
}

} // namespace _testfw

// ─── public API ──────────────────────────────────────────────────────────────

inline void _reverse_list() {
  using namespace _testfw;
  Entry *prev = nullptr, *cur = list_head;
  while (cur) {
    Entry *next = cur->next;
    cur->next = prev;
    prev = cur;
    cur = next;
  }
  list_head = prev;
}

inline int run_all_tests() {
  using namespace _testfw;
  _reverse_list();
  for (Entry *e = list_head; e; e = e->next) {
    test_ok = true;
    e->fn();
    if (test_ok) {
      passed++;
      printf("PASS  %s\n", e->name);
    } else {
      failed++;
      printf("FAIL  %s\n", e->name);
    }
  }
  printf("\n%d passed, %d failed\n", passed, failed);
  return failed > 0 ? 1 : 0;
}

inline int list_tests() {
  _reverse_list();
  for (_testfw::Entry *e = _testfw::list_head; e; e = e->next)
    printf("%s\n", e->name);
  return 0;
}

inline int list_xfail_tests() {
  _reverse_list();
  for (_testfw::Entry *e = _testfw::list_head; e; e = e->next)
    if (_testfw::current_expected(e) == _testfw::Expected::Fail)
      printf("%s\n", e->name);
  return 0;
}

inline int list_skip_tests() {
  _reverse_list();
  for (_testfw::Entry *e = _testfw::list_head; e; e = e->next)
    if (_testfw::current_expected(e) == _testfw::Expected::Skip)
      printf("%s\n", e->name);
  return 0;
}

inline int run_single_test(const char *name) {
  _reverse_list();
  for (_testfw::Entry *e = _testfw::list_head; e; e = e->next) {
    if (__builtin_strcmp(e->name, name) == 0) {
      _testfw::test_ok = true;
      e->fn();
      return _testfw::test_ok ? 0 : 1;
    }
  }
  fprintf(stderr, "unknown test: %s\n", name);
  return 2;
}

// ─── macros ──────────────────────────────────────────────────────────────────

#define TEST_GROUP(name) struct _testfw_group_##name

// normal test: expected to pass under both sanitizers
#define TEST(group, name)                                                      \
  static void _test_impl_##group##_##name();                                   \
  static _testfw::Reg _test_reg_##group##_##name(#group "::" #name,            \
                                                 _test_impl_##group##_##name); \
  static void _test_impl_##group##_##name()

// expected to fail under both sanitizers (CHECK failures, UBSan, etc.)
#define XFAIL_TEST(group, name)                                                \
  static void _test_impl_##group##_##name();                                   \
  static _testfw::Reg _test_reg_##group##_##name(                              \
      #group "::" #name, _test_impl_##group##_##name, _testfw::Expected::Fail, \
      _testfw::Expected::Fail);                                                \
  static void _test_impl_##group##_##name()

// expected to fail under ASan; not applicable under MSan (skip)
#define ASAN_XFAIL_TEST(group, name)                                           \
  static void _test_impl_##group##_##name();                                   \
  static _testfw::Reg _test_reg_##group##_##name(                              \
      #group "::" #name, _test_impl_##group##_##name, _testfw::Expected::Fail, \
      _testfw::Expected::Skip);                                                \
  static void _test_impl_##group##_##name()

// expected to fail under MSan; not applicable under ASan (skip)
#define MSAN_XFAIL_TEST(group, name)                                           \
  static void _test_impl_##group##_##name();                                   \
  static _testfw::Reg _test_reg_##group##_##name(                              \
      #group "::" #name, _test_impl_##group##_##name, _testfw::Expected::Skip, \
      _testfw::Expected::Fail);                                                \
  static void _test_impl_##group##_##name()

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      _testfw::fail_at(__FILE__, __LINE__, "CHECK(" #cond ")");                \
      return;                                                                  \
    }                                                                          \
  } while (0)

#define CHECK_EQUAL(expected, actual)                                          \
  do {                                                                         \
    if (!((expected) == (actual))) {                                           \
      _testfw::fail_at(__FILE__, __LINE__,                                     \
                       "CHECK_EQUAL(" #expected ", " #actual ")");             \
      return;                                                                  \
    }                                                                          \
  } while (0)

#pragma once
#include <cstdio>

// ─── internals ───────────────────────────────────────────────────────────────

namespace _testfw {

struct Entry {
    const char *name;
    void (*fn)();
    Entry *next;
};

static Entry *list_head = nullptr;
static int passed = 0;
static int failed = 0;
static bool test_ok = true;

struct Reg {
    Entry e;
    Reg(const char *name, void (*fn)()) {
        e.name = name;
        e.fn = fn;
        e.next = list_head;
        list_head = &e;
    }
};

inline void fail_at(const char *file, int line, const char *expr) {
    printf("    [%s:%d] %s\n", file, line, expr);
    test_ok = false;
}

} // namespace _testfw

// ─── public API ──────────────────────────────────────────────────────────────

inline int run_all_tests() {
    using namespace _testfw;
    // reverse list to restore declaration order
    Entry *prev = nullptr, *cur = list_head;
    while (cur) {
        Entry *next = cur->next;
        cur->next = prev;
        prev = cur;
        cur = next;
    }
    list_head = prev;

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

// ─── macros ──────────────────────────────────────────────────────────────────

#define TEST_GROUP(name) struct _testfw_group_##name

#define TEST(group, name)                                                      \
    static void _test_impl_##group##_##name();                                 \
    static _testfw::Reg _test_reg_##group##_##name(                            \
        #group "::" #name, _test_impl_##group##_##name);                       \
    static void _test_impl_##group##_##name()

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            _testfw::fail_at(__FILE__, __LINE__, "CHECK(" #cond ")");          \
            return;                                                            \
        }                                                                      \
    } while (0)

#define CHECK_EQUAL(expected, actual)                                          \
    do {                                                                       \
        if (!((expected) == (actual))) {                                       \
            _testfw::fail_at(__FILE__, __LINE__,                               \
                             "CHECK_EQUAL(" #expected ", " #actual ")");       \
            return;                                                            \
        }                                                                      \
    } while (0)

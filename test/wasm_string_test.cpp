#include "test_framework.h"

// The wasm build's string functions (src/arch/wasm/string.cpp), compiled
// natively under fcsim_ names so they don't replace the host libc that the
// sanitizer runtimes use.
#define strlen fcsim_strlen
#define strcmp fcsim_strcmp
#define memset fcsim_memset
#define memcpy fcsim_memcpy
#define memmove fcsim_memmove
#define memcmp fcsim_memcmp
#include "../src/arch/wasm/string.cpp"
#undef strlen
#undef strcmp
#undef memset
#undef memcpy
#undef memmove
#undef memcmp

static int sign(int x) { return (x > 0) - (x < 0); }

TEST_GROUP(WasmStringTests){};

// strcmp/memcmp compare bytes as unsigned char, as the C standard requires
// (#156). wasm32 char is signed (the test build uses -fsigned-char to match),
// so bytes >= 0x80 catch a signed comparison.
TEST(WasmStringTests, MemcmpUnsigned) {
  CHECK_EQUAL(1, sign(fcsim_memcmp("\x80", "\x01", 1)));
  CHECK_EQUAL(-1, sign(fcsim_memcmp("\x01", "\x80", 1)));
  CHECK_EQUAL(1, sign(fcsim_memcmp("a\xff", "a\x7f", 2)));
  CHECK_EQUAL(0, sign(fcsim_memcmp("abc", "abc", 3)));
}

TEST(WasmStringTests, StrcmpUnsigned) {
  CHECK_EQUAL(1, sign(fcsim_strcmp("\x80", "\x01")));
  CHECK_EQUAL(1, sign(fcsim_strcmp("\xc3\xa9", "z")));
  CHECK_EQUAL(-1, sign(fcsim_strcmp("abc", "abd")));
  CHECK_EQUAL(-1, sign(fcsim_strcmp("ab", "abc")));
  CHECK_EQUAL(0, sign(fcsim_strcmp("abc", "abc")));
}

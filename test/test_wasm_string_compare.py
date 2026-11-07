"""The wasm build's strcmp/memcmp (src/arch/wasm/string.cpp) must compare
bytes as unsigned char, as the C standard requires (see #156).

string.cpp only builds for wasm32, so this compiles it natively with its
functions renamed (so they don't clash with the host libc) and with
-fsigned-char to match wasm32, then runs a small driver against it.
"""

import subprocess
from pathlib import Path

import pytest

SOURCE = Path(__file__).parent.parent / "src" / "arch" / "wasm" / "string.cpp"

FUNCTIONS = ["strlen", "strcmp", "memset", "memcpy", "memmove", "memcmp"]

DRIVER = r"""
#include <stddef.h>
#include <stdio.h>
extern "C" int fcsim_strcmp(const char *, const char *);
extern "C" int fcsim_memcmp(const void *, const void *, size_t);
static int sign(int x) { return (x > 0) - (x < 0); }
int main() {
  printf("%d\n", sign(fcsim_memcmp("\x80", "\x01", 1)));
  printf("%d\n", sign(fcsim_memcmp("\x01", "\x80", 1)));
  printf("%d\n", sign(fcsim_memcmp("a\xff", "a\x7f", 2)));
  printf("%d\n", sign(fcsim_memcmp("abc", "abc", 3)));
  printf("%d\n", sign(fcsim_strcmp("\x80", "\x01")));
  printf("%d\n", sign(fcsim_strcmp("\xc3\xa9", "z")));
  printf("%d\n", sign(fcsim_strcmp("abc", "abd")));
  printf("%d\n", sign(fcsim_strcmp("ab", "abc")));
  printf("%d\n", sign(fcsim_strcmp("abc", "abc")));
  return 0;
}
"""

EXPECTED = [1, -1, 1, 0, 1, 1, -1, -1, 0]


@pytest.fixture(scope="module")
def driver(tmp_path_factory):
    tmp = tmp_path_factory.mktemp("wasm_string")
    (tmp / "driver.cpp").write_text(DRIVER)
    exe = tmp / "driver"
    renames = [f"-D{name}=fcsim_{name}" for name in FUNCTIONS]
    subprocess.run(
        [
            "clang++",
            "-O2",
            "-fsigned-char",
            "-fno-builtin",
            *renames,
            str(SOURCE),
            str(tmp / "driver.cpp"),
            "-o",
            str(exe),
        ],
        check=True,
    )
    return exe


def test_compare_as_unsigned_char(driver):
    result = subprocess.run([str(driver)], capture_output=True, text=True, check=True)
    assert [int(x) for x in result.stdout.split()] == EXPECTED

"""The wasm build's memcpy/memset/memmove (src/arch/wasm/string.cpp) must not
compile into calls to themselves (see #154).

clang's loop idiom recognition turns byte-copy and byte-fill loops into
llvm.memcpy/llvm.memset, which lower to calls to memcpy/memset. Inside the
functions that implement those, that is infinite recursion. string.cpp opts
out with no_builtin on every function; this checks that it still does, and
that the optimized output has no such calls.
"""

import re
import subprocess
from pathlib import Path

SOURCE = Path(__file__).parent.parent / "src" / "arch" / "wasm" / "string.cpp"

# matches the wasm flags in SConstruct
WASM_FLAGS = [
    "-O2",
    "--target=wasm32",
    "-nostdlib",
    "-fno-rtti",
    "-fno-exceptions",
]

MEM_CALL = re.compile(r"\bcall\b[^\n]*@(llvm\.mem(cpy|set|move)\b|mem(cpy|set|move)\()")


def _compile_to_ir():
    result = subprocess.run(
        ["clang++", *WASM_FLAGS, "-S", "-emit-llvm", "-o", "-", str(SOURCE)],
        capture_output=True,
        text=True,
        check=True,
    )
    return result.stdout


def test_every_function_is_no_builtin():
    ir = _compile_to_ir()
    groups = dict(re.findall(r"^attributes (#\d+) = \{(.*)\}$", ir, re.MULTILINE))
    defines = re.findall(
        r"^define [^\n]*@(\w+)\([^\n]*\) [^\n]*?(#\d+) \{$", ir, re.MULTILINE
    )
    assert defines, "no functions found in string.cpp IR"
    missing = [name for name, group in defines if '"no-builtins"' not in groups[group]]
    assert not missing, f"functions without no_builtin: {missing}"


def test_no_calls_to_mem_functions():
    ir = _compile_to_ir()
    calls = [line.strip() for line in ir.splitlines() if MEM_CALL.search(line)]
    assert not calls, "string.cpp calls mem* functions:\n" + "\n".join(calls)

"""Editor regression tests: runs each scenario in arena_test_asan (built from
test/arena_test_main.cpp with ASan/UBSan) in its own process."""

import os
import subprocess
from pathlib import Path

import pytest

BINARY = Path(__file__).parent.parent / "arena_test_asan"
ENV = {
    **os.environ,
    "ASAN_OPTIONS": "halt_on_error=1:print_stacktrace=1",
    "UBSAN_OPTIONS": "halt_on_error=1:print_stacktrace=1",
}


def _scenarios():
    result = subprocess.run(
        [str(BINARY), "--list"], capture_output=True, text=True, check=True
    )
    return result.stdout.split()


@pytest.mark.parametrize("scenario", _scenarios())
def test_arena(scenario):
    result = subprocess.run(
        [str(BINARY), scenario], capture_output=True, text=True, env=ENV, timeout=60
    )
    assert result.returncode == 0, result.stdout + result.stderr

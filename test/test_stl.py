import os
import subprocess
import pytest

BINARY = "./stl_test"
ASAN_ENV = {
    "ASAN_OPTIONS": "halt_on_error=1:print_stacktrace=1",
    "UBSAN_OPTIONS": "halt_on_error=1:print_stacktrace=1",
}


def get_tests():
    r = subprocess.run([BINARY, "--list"], capture_output=True, text=True, check=True)
    xfail = set(
        subprocess.run(
            [BINARY, "--list-xfail"], capture_output=True, text=True, check=True
        ).stdout.strip().splitlines()
    )
    return [
        pytest.param(name, marks=pytest.mark.xfail) if name in xfail else name
        for name in r.stdout.strip().splitlines()
    ]


@pytest.mark.parametrize("name", get_tests())
def test_stl(name):
    r = subprocess.run(
        [BINARY, "--run", name],
        capture_output=True,
        text=True,
        env={**os.environ, **ASAN_ENV},
    )
    assert r.returncode == 0, r.stderr + r.stdout

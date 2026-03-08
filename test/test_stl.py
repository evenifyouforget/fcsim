import subprocess
import pytest

BINARY = "./stl_test"
ASAN_ENV = {
    "ASAN_OPTIONS": "halt_on_error=1:print_stacktrace=1",
    "UBSAN_OPTIONS": "halt_on_error=1:print_stacktrace=1",
}


def get_tests():
    r = subprocess.run([BINARY, "--list"], capture_output=True, text=True, check=True)
    return r.stdout.strip().splitlines()


@pytest.mark.parametrize("name", get_tests())
def test_stl(name):
    r = subprocess.run(
        [BINARY, "--run", name],
        capture_output=True,
        text=True,
        env={**__import__("os").environ, **ASAN_ENV},
    )
    assert r.returncode == 0, r.stderr + r.stdout

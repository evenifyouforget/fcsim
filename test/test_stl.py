import os
import subprocess
import pytest

SANITIZERS = {
    "asan": {
        "binary": "./stl_test_asan",
        "env": {
            "ASAN_OPTIONS": "halt_on_error=1:print_stacktrace=1",
            "UBSAN_OPTIONS": "halt_on_error=1:print_stacktrace=1",
        },
    },
    "msan": {
        "binary": "./stl_test_msan",
        "env": {
            "MSAN_OPTIONS": "halt_on_error=1:print_stacktrace=1",
            "UBSAN_OPTIONS": "halt_on_error=1:print_stacktrace=1",
        },
    },
}


def _query(binary, flag):
    r = subprocess.run([binary, flag], capture_output=True, text=True, check=True)
    return set(r.stdout.strip().splitlines())


def get_params():
    params = []
    for san, cfg in SANITIZERS.items():
        binary = cfg["binary"]
        all_tests = (
            subprocess.run(
                [binary, "--list"], capture_output=True, text=True, check=True
            )
            .stdout.strip()
            .splitlines()
        )
        xfail = _query(binary, "--list-xfail")
        skip = _query(binary, "--list-skip")
        for name in all_tests:
            if name in skip:
                marks = [pytest.mark.skip]
            elif name in xfail:
                marks = [pytest.mark.xfail]
            else:
                marks = []
            params.append(pytest.param(san, name, marks=marks, id=f"{san}-{name}"))
    return params


@pytest.mark.parametrize("san,name", get_params())
def test_stl(san, name):
    cfg = SANITIZERS[san]
    r = subprocess.run(
        [cfg["binary"], "--run", name],
        capture_output=True,
        text=True,
        env={**os.environ, **cfg["env"]},
    )
    assert r.returncode == 0, r.stderr + r.stdout


# TF6: run_single_test returns exit code 2 for an unknown test name.
def test_unknown_test_exit_code():
    for san, cfg in SANITIZERS.items():
        r = subprocess.run(
            [cfg["binary"], "--run", "NoSuch::Test"],
            capture_output=True,
            text=True,
            env={**os.environ, **cfg["env"]},
        )
        assert r.returncode == 2, f"{san}: expected exit 2, got {r.returncode}"

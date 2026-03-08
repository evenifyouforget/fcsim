"""
Coverage collection for stl_mock.cpp using llvm-cov.

Run with:
    pytest test/test_coverage.py

After the session, the HTML report is written to coverage_html/ and a
text summary is printed. The raw profile data is left in coverage_profiles/
for inspection.

Only the 31 real tests are exercised (xfail/skip entries are excluded:
they abort via sanitizer or CHECK failure and would not flush complete
profile data).
"""

import glob
import os
import shutil
import subprocess

import pytest

BINARY = "./stl_test_cov"
PROFDIR = "coverage_profiles"
PROFDATA = "coverage_profiles/merged.profdata"
REPORT_DIR = "coverage_html"
SOURCES = ["src/stl_mock.cpp"]
LLVM_PROFDATA = "llvm-profdata-18"
LLVM_COV = "llvm-cov-18"


def _get_real_tests():
    all_tests = subprocess.run(
        [BINARY, "--list"], capture_output=True, text=True, check=True
    ).stdout.strip().splitlines()
    xfail = set(
        subprocess.run(
            [BINARY, "--list-xfail"], capture_output=True, text=True, check=True
        ).stdout.strip().splitlines()
    )
    skip = set(
        subprocess.run(
            [BINARY, "--list-skip"], capture_output=True, text=True, check=True
        ).stdout.strip().splitlines()
    )
    return [n for n in all_tests if n not in xfail and n not in skip]


@pytest.fixture(scope="session", autouse=True)
def coverage_report():
    os.makedirs(PROFDIR, exist_ok=True)
    # clear any stale profiles from previous runs
    for f in glob.glob(f"{PROFDIR}/*.profraw"):
        os.remove(f)

    yield  # tests run here

    profraw = glob.glob(f"{PROFDIR}/*.profraw")
    if not profraw:
        print("\nNo profile data collected.")
        return

    subprocess.run(
        [LLVM_PROFDATA, "merge", *profraw, "-o", PROFDATA],
        check=True,
    )

    shutil.rmtree(REPORT_DIR, ignore_errors=True)
    subprocess.run(
        [
            LLVM_COV, "show", BINARY,
            f"-instr-profile={PROFDATA}",
            "-format=html",
            f"-output-dir={REPORT_DIR}",
            *SOURCES,
        ],
        check=True,
    )

    print(f"\nHTML report: {REPORT_DIR}/index.html\n")
    subprocess.run(
        [LLVM_COV, "report", BINARY, f"-instr-profile={PROFDATA}", *SOURCES],
    )


@pytest.mark.parametrize("name", _get_real_tests())
def test_coverage(name):
    env = {**os.environ, "LLVM_PROFILE_FILE": f"{PROFDIR}/%p.profraw"}
    r = subprocess.run(
        [BINARY, "--run", name],
        capture_output=True,
        text=True,
        env=env,
    )
    assert r.returncode == 0, r.stderr + r.stdout

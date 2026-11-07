"""fp_sin/fp_cos for large arguments (#13).

For |x| >= 90112 the 794-target runtime computes sin/cos with the x87
FSIN/FCOS instructions, which reduce the argument with a 66-bit pi. fcsim
emulates them in src/fpmath/sincos.c. This compiles that file natively and
checks it bit for bit against an exact model of the instructions:
sin/cos(x * pi / pi66), rounded to the x87's 64-bit significand and then to
double, both to nearest with ties to even.
"""

import random
import subprocess
from decimal import Decimal, getcontext
from fractions import Fraction
from pathlib import Path

import pytest

ROOT = Path(__file__).parent.parent

# set before the constants below are computed
getcontext().prec = 90

PI = Decimal(
    "3.14159265358979323846264338327950288419716939937510"
    "58209749445923078164062862089986280348253421170679821"
)
PI66 = Decimal(0xC90FDAA22168C234C) / Decimal(2) ** 66

DRIVER = r"""
#include <stdio.h>
#include <stdlib.h>
#include <fpmath/fpmath.h>
int main(int argc, char **argv) {
  for (int i = 1; i < argc; i++) {
    double x = strtod(argv[i], 0);
    printf("%a %a\n", fp_sin(x), fp_cos(x));
  }
  return 0;
}
"""


def _round_bits(q, bits):
    """Round the rational q to `bits` significant bits, ties to even."""
    if q == 0:
        return q
    sign = -1 if q < 0 else 1
    q = abs(q)
    e = q.numerator.bit_length() - q.denominator.bit_length()
    while Fraction(2) ** e > q:
        e -= 1
    while Fraction(2) ** (e + 1) <= q:
        e += 1
    scale = Fraction(2) ** (bits - 1 - e)
    m = q * scale
    whole = m.numerator // m.denominator
    rem = m - whole
    if rem > Fraction(1, 2) or (rem == Fraction(1, 2) and whole % 2 == 1):
        whole += 1
    return sign * Fraction(whole) / scale


def _x87_model(x):
    s = (Decimal(x) * PI / PI66) % (2 * PI)
    r2 = s * s
    results = []
    for term, n in ((s, 1), (Decimal(1), 0)):  # sin, then cos
        total = term
        while abs(term) > Decimal(10) ** -85:
            term = -term * r2 / ((n + 1) * (n + 2))
            total += term
            n += 2
        results.append(float(_round_bits(_round_bits(Fraction(total), 64), 53)))
    return results


def _inputs():
    rng = random.Random(13)
    xs = [90112.0, -90112.0, 90500.0, -90500.0, 1e6, 123456789.123, 4.5e15]
    xs += [9.2e18, -7.7e17, 9223372036854774784.0]  # just below 2^63
    while len(xs) < 300:
        x = rng.uniform(1.0, 2.0) * 2.0 ** rng.uniform(16.46, 63)
        if 90112.0 <= x < 2.0**63:
            xs.append(x if rng.random() < 0.5 else -x)
    return xs


@pytest.fixture(scope="module")
def driver(tmp_path_factory):
    tmp = tmp_path_factory.mktemp("sincos")
    (tmp / "driver.c").write_text(DRIVER)
    exe = tmp / "driver"
    subprocess.run(
        [
            "cc",
            "-O2",
            f"-I{ROOT / 'include'}",
            str(tmp / "driver.c"),
            str(ROOT / "src" / "fpmath" / "sincos.c"),
            "-lm",
            "-o",
            str(exe),
        ],
        check=True,
    )
    return exe


def test_large_arguments_match_x87_model(driver):
    xs = _inputs()
    out = subprocess.run(
        [str(driver), *(x.hex() for x in xs)],
        capture_output=True,
        text=True,
        check=True,
    ).stdout.split("\n")
    mismatches = []
    for x, line in zip(xs, out):
        got = [float.fromhex(v) for v in line.split()]
        want = _x87_model(x)
        if got != want:
            mismatches.append(f"x={x!r}: got {got}, want {want}")
    assert not mismatches, "\n".join(mismatches[:20])

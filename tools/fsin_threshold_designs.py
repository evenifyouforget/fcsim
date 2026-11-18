#!/usr/bin/env python3
"""Generate FC test designs that pin down the s794 fsin switch point (#13).

Experiment only, not for merging. Usage:

    scons run_single_design_xml_fsin
    python3 tools/fsin_threshold_designs.py LOW HIGH [--count 10] [--min-diff 1]

LOW and HIGH are the current bounds on the switch point T (LOW < T <= HIGH).
Each design puts some bodies at angles near a random A in (LOW, HIGH). Of all
the angles they reach during the run, A_min and A_max are the lowest and
highest inside (LOW, HIGH). Each design comes with two predicted solve ticks,
computed with the fsin emulation:

    below: what FC shows if T <= A_min
    above: what FC shows if T >  A_max

A design is kept only if both predictions hold for every T in their half of
(LOW, HIGH] (checked at both ends) and they differ by at least --min-diff ticks.
Running one in FC halves the interval: matching "below" gives HIGH = A_min,
matching "above" gives LOW = A_max.

Templates are tried simplest first, --attempts times each, before moving on.
Angles and layouts come from the OS random source, so separate runs with the
same arguments don't repeat each other's work.
"""

import argparse
import math
import os
import random
import re
import subprocess
import sys
import urllib.parse
import urllib.request
from pathlib import Path

rng = random.SystemRandom()
BINARY = Path(__file__).resolve().parent.parent / "run_single_design_xml_fsin"


# ---- design XML ----


def blk(tag, x, y, w, h, rot=0.0, goal=False, bid=None):
    i = f' id="{bid}"' if bid is not None else ""
    return (
        f"<{tag}{i}><rotation>{rot!r}</rotation>"
        f"<position><x>{x!r}</x><y>{y!r}</y></position>"
        f"<width>{w!r}</width><height>{h!r}</height>"
        f"<goalBlock>{str(goal).lower()}</goalBlock><joints/></{tag}>"
    )


def design(level, player, build, goal):
    """player[0] must be the body with the large angle"""
    area = "<position><x>{!r}</x><y>{!r}</y></position><width>{!r}</width><height>{!r}</height>"
    return (
        '<?xml version="1.0"?><retrieveLevel><levelId>-1</levelId><level>'
        "<levelBlocks>" + "".join(level) + "</levelBlocks>"
        "<playerBlocks>" + "".join(player) + "</playerBlocks>"
        "<start>" + area.format(*build) + "</start>"
        "<end>" + area.format(*goal) + "</end>"
        "</level></retrieveLevel>"
    )


def u(lo, hi):
    return rng.uniform(lo, hi)


# ---- templates, simplest first ----


def hill(a):
    """A goal ball rolls down one slope onto a floor and into the goal area."""
    slope = u(0.15, 0.5)
    r = u(10, 25)
    x0 = u(-280, -150)
    y0 = math.tan(slope) * x0 - (10 / math.cos(slope)) - r - 1
    return design(
        [
            blk("StaticRectangle", 0, 0, 600, 20, slope),
            blk("StaticRectangle", 600, 320, 1400, 20),
        ],
        [blk("NoSpinWheel", x0, y0, 2 * r, 2 * r, a, True, 0)],
        (-300, -200, 300, 300),
        (u(700, 1100), 310 - 60, 160, 120),
    )


def pinball(a):
    """A goal ball falls through a board of static pegs into a narrow bin."""
    rows = rng.randint(4, 8)
    gap = u(45, 70)
    pegs = []
    for row in range(rows):
        for c in range(row + 2):
            pegs.append(
                blk(
                    "StaticCircle",
                    (c - (row + 1) / 2) * gap + u(-3, 3),
                    -150 + row * gap * 0.9,
                    u(5, 12),
                    u(5, 12),
                )
            )
    floor = -150 + rows * gap * 0.9 + 120
    r = u(8, 14)
    return design(
        pegs + [blk("StaticRectangle", 0, floor + 10, 1400, 20)],
        [blk("NoSpinWheel", u(-8, 8), -260, 2 * r, 2 * r, a, True, 0)],
        (0, -260, 300, 200),
        (u(-150, 150), floor - 40, 60, 80),
    )


def heap(n):
    """A wheel drops onto a heap of n dynamic circles on a shelf; the heap
    spills down three ramps, carrying a goal ball to the goal area. The
    static layout follows FC test level 687738 (design 12707232)."""

    def make(a):
        statics = [
            blk("StaticRectangle", -220, 20, 440, 40),
            blk("StaticRectangle", -420, 150, 40, 900),
            blk("StaticRectangle", 0, 70, 400, 40, 0.0175),
            blk("StaticRectangle", 100, 300, 800, 40, -0.1745),
            blk("StaticRectangle", -40, 590, 800, 40, 0.0175),
        ]
        wheel = (-371 + u(-5, 5), -348, 20)
        ball = (-20 + u(-15, 15), -10, u(8, 12))
        placed = [wheel, ball]
        circles = []
        for _ in range(n * 50):
            if len(circles) == n:
                break
            r = u(8, 30)
            c = (u(-395 + r, 40 - r), u(-300 + r, -r), r)
            if all(math.dist(c[:2], p[:2]) > c[2] + p[2] + 1 for p in placed):
                placed.append(c)
                circles.append(blk("DynamicCircle", c[0], c[1], r, r))
        return design(
            statics + circles,
            [
                blk("NoSpinWheel", wheel[0], wheel[1], 40, 40, a, False, 0),
                blk(
                    "NoSpinWheel",
                    ball[0],
                    ball[1],
                    2 * ball[2],
                    2 * ball[2],
                    0.0,
                    True,
                    1,
                ),
            ],
            (-205, -145, 405, 305),
            (300, 430, 105, 325),
        )

    make.__doc__ = heap.__doc__
    return make


def seesaws(a, turn=0.0):
    """Many tiny goal balls, all at the large angle, fall through layers of
    seesaws (thin dynamic planks on three static circles) between ramps; the
    level only solves once every ball is in the goal area. After the
    Seesaw4 test levels (e.g. FC level 690676), with randomised ball sizes
    and small jitter on the layout. TURN is added to the rotation of every
    part that never moves (floor, walls and ramps)."""
    level = [
        blk("StaticRectangle", 0, 2000, 4000, 2600, turn),
        blk("StaticRectangle", -1500, 0, 1000, 4000, turn - 0.0349),
        blk("StaticRectangle", 1500, 0, 1000, 4000, turn + 0.0349),
    ]
    for y in (-800, -600, -400, -200, 0, 200):
        # ramps: zero-height dynamic rects, which Box2D makes static
        level += [
            blk(
                "DynamicRectangle", -2000, y, 3400, 0, turn + 0.0524 + u(-0.003, 0.003)
            ),
            blk("DynamicRectangle", 2000, y, 3400, 0, turn - 0.0524 + u(-0.003, 0.003)),
        ]
        if y == -800:
            continue
        # seesaw: plank resting on a central pivot, two side supports below
        jx = u(-5, 5)
        level += [
            blk("DynamicRectangle", jx, y, 800, 2),
            blk("StaticCircle", jx, y + 10, 20, 20),
            blk("StaticCircle", jx + 200 + u(-10, 10), y + 30, 20, 20),
            blk("StaticCircle", jx - 200 + u(-10, 10), y + 30, 20, 20),
        ]
    x0 = u(-120, 60)
    balls = [
        blk(
            "NoSpinWheel",
            x0 + 30 * i,
            -900 - 30 * j,
            *[rng.randint(1, 10)] * 2,
            a,
            True,
            7 * i + j,
        )
        for i in range(7)
        for j in range(7)
    ]
    return design(level, balls, (0, 0, 2000, 1400), (0, 1000, 2000, 1000))


def seesaws_fixed(a):
    """The seesaws layout with every ball at angle 0. Instead, the parts that
    never move are stored at their usual tilt plus k whole turns, with 2*pi*k
    close to A. Box2D computes a body's rotation once, when it creates the
    body, so these angles never change: each design tests the switch point
    within about 0.11 rad."""
    return seesaws(0.0, 2 * math.pi * round(a / (2 * math.pi)))


_designs = {}


def fc_design(design_id):
    """A design from FC with its goal piece at angle A. Saved under its own
    level, so it can be saved back to FC from fcsim with Open file."""

    def make(a):
        if design_id not in _designs:
            req = urllib.request.Request(
                "http://www.fantasticcontraption.com/retrieveLevel.php",
                data=urllib.parse.urlencode(
                    {"id": design_id, "loadDesign": 1}
                ).encode(),
            )
            with urllib.request.urlopen(req, timeout=30) as r:
                _designs[design_id] = r.read().decode()
        xml = _designs[design_id]
        i, j = xml.index("<playerBlocks>"), xml.index("</playerBlocks>")
        player, n = re.subn(
            r'(<(\w+) id="\d+">\s*<rotation>)[^<]*(</rotation>(?:(?!</\2>).)*?<goalBlock>true)',
            lambda m: m[1] + repr(a) + m[3],
            xml[i:j],
            flags=re.S,
        )
        assert n == 1, f"design {design_id} has {n} goal pieces"
        return xml[:i] + player + xml[j:]

    return make


# (name, template, min ticks per run)
TEMPLATES = [
    ("hill", hill, 0),
    ("pinball", pinball, 0),
    ("snail", fc_design(12728932), 0),
    ("seesaws_fixed", seesaws_fixed, 0),
    ("layercake", fc_design(12728960), 20000),
    ("seesaws", seesaws, 0),
    ("tugofwar", fc_design(12728937), 20000),
] + [(f"heap{n}", heap(n), 0) for n in (20, 50, 110)]


# ---- simulation ----


def run(xml, threshold, ticks, trace=False):
    """(solve tick or -1, if trace: every angle reached by any player block)"""
    env = dict(os.environ, FSIN_THRESHOLD=repr(threshold))
    env.pop("FCSIM_TRACE", None)
    if trace:
        env["FCSIM_TRACE"] = "1"
    p = subprocess.run(
        [str(BINARY), str(ticks)], input=xml, capture_output=True, text=True, env=env
    )
    if p.returncode != 0:
        raise RuntimeError(f"{BINARY} exited {p.returncode}")
    solve = int(p.stdout.split()[0])
    angles = [float(a) for a in re.findall(r"Ang = ([^,]+),", p.stderr)]
    return solve, angles


def level_angles(xml):
    """the level blocks' angles; these templates only give large angles to
    level blocks that never move, so these are all the angles they reach"""
    level = xml[xml.index("<levelBlocks>") : xml.index("</levelBlocks>")]
    return [float(r) for r in re.findall(r"<rotation>([^<]*)</rotation>", level)]


def attempt(make, low, high, ticks, min_diff):
    a = u(low, high)
    xml = make(a)
    below, ang_lo = run(xml, low, ticks, trace=True)
    above, ang_hi = run(xml, high, ticks, trace=True)
    if below < 0 or above < 0 or abs(below - above) < min_diff:
        return None
    # angles outside (low, high) take the same path for every T in (low, high]
    inside = [
        abs(x) for x in ang_lo + ang_hi + level_angles(xml) if low < abs(x) < high
    ]
    if not inside:
        return None
    a_min, a_max = min(inside), max(inside)
    # a design whose angles cover most of (low, high) would hardly narrow it
    if a_max - a_min > (high - low) / 2:
        return None
    # the other end of each half: T just at a_min, T just above a_max
    if run(xml, a_min, ticks)[0] != below:
        return None
    if run(xml, math.nextafter(a_max, math.inf), ticks)[0] != above:
        return None
    return a, a_min, a_max, below, above, xml


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    ap.add_argument("low", type=float, help="known: switch point > LOW")
    ap.add_argument("high", type=float, help="known: switch point <= HIGH")
    ap.add_argument("--count", type=int, default=10, help="designs to write (10)")
    ap.add_argument(
        "--min-diff", type=int, default=1, help="min |below - above| ticks (1)"
    )
    ap.add_argument("--attempts", type=int, default=40, help="tries per template (40)")
    ap.add_argument(
        "--ticks",
        type=int,
        default=6000,
        help="max ticks per run (6000; more for some templates)",
    )
    ap.add_argument("--out", default="fsin_designs", help="output directory")
    args = ap.parse_args()
    if not 0 <= args.low < args.high:
        ap.error("need 0 <= LOW < HIGH")
    if not BINARY.exists():
        sys.exit(f"{BINARY} not found; build it with: scons run_single_design_xml_fsin")

    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    found = 0
    for name, make, min_ticks in TEMPLATES:
        hits = 0
        for _ in range(args.attempts):
            if found == args.count:
                break
            r = attempt(
                make, args.low, args.high, max(args.ticks, min_ticks), args.min_diff
            )
            if not r:
                continue
            a, a_min, a_max, below, above, xml = r
            tag = f"{name}_A{a:.6f}_{rng.getrandbits(32):08x}"
            (out / f"{tag}.xml").write_text(xml)
            line = (
                f"{tag}.xml\tangle={a!r}\trange=[{a_min!r}, {a_max!r}]\t"
                f"below={below}\tabove={above}"
            )
            with open(out / "designs.tsv", "a") as f:
                f.write(line + "\n")
            print(
                f"{tag}.xml: FC {below} -> switch point <= {a_min:.6f}; "
                f"FC {above} -> switch point > {a_max:.6f}"
            )
            found += 1
            hits += 1
        print(f"[{name}: {hits} of {args.attempts} attempts kept]", file=sys.stderr)
        if found == args.count:
            break
    print(f"{found} designs in {out}/ (index: designs.tsv)", file=sys.stderr)


if __name__ == "__main__":
    main()

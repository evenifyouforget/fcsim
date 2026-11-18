"""Binary design file (#179, README §Design File), through run_single_design_xml,
which loads XML or a design file from stdin and, given a second argument,
saves what it loaded as a design file."""

import math
import struct
import subprocess
from pathlib import Path

import pytest

BINARY = Path(__file__).parent.parent / "run_single_design_xml"

HEADER = struct.Struct("<4sHi8dI")
BLOCK = struct.Struct("<B5dHHB")
NONE = 0xFFFF

# level: a static floor, a static circle, a dynamic rectangle
# player: 0 goal circle of diameter 60 (so it has 4 inner joints), 1 water rod
# from its inner joint at (20, 0), 2 wood rod hanging from rod 1's right end,
# 3 wheel on rod 2's lower end, 4 goal rectangle
XML = """<?xml version="1.0"?><retrieveLevel><levelId>31</levelId><level>
<levelBlocks>
<StaticRectangle><rotation>0</rotation><position><x>0</x><y>300</y></position>
<width>800</width><height>40</height><goalBlock>false</goalBlock><joints/></StaticRectangle>
<StaticCircle><rotation>0</rotation><position><x>-300</x><y>200</y></position>
<width>30</width><height>30</height><goalBlock>false</goalBlock><joints/></StaticCircle>
<DynamicRectangle><rotation>0.3</rotation><position><x>300</x><y>200</y></position>
<width>50</width><height>20</height><goalBlock>false</goalBlock><joints/></DynamicRectangle>
</levelBlocks>
<playerBlocks>
<NoSpinWheel id="0"><rotation>0</rotation><position><x>0</x><y>0</y></position>
<width>60</width><height>60</height><goalBlock>true</goalBlock><joints/></NoSpinWheel>
<HollowRod id="1"><rotation>0</rotation><position><x>70</x><y>0</y></position>
<width>100</width><height>4</height><goalBlock>false</goalBlock>
<joints><jointedTo>0</jointedTo></joints></HollowRod>
<SolidRod id="2"><rotation>1.5707963267948966</rotation><position><x>120</x><y>50</y></position>
<width>100</width><height>8</height><goalBlock>false</goalBlock>
<joints><jointedTo>1</jointedTo></joints></SolidRod>
<NoSpinWheel id="3"><rotation>0</rotation><position><x>120</x><y>100</y></position>
<width>40</width><height>40</height><goalBlock>false</goalBlock>
<joints><jointedTo>2</jointedTo></joints></NoSpinWheel>
<JointedDynamicRectangle id="4"><rotation>0.2</rotation><position><x>-150</x><y>0</y></position>
<width>40</width><height>30</height><goalBlock>true</goalBlock><joints/></JointedDynamicRectangle>
</playerBlocks>
<start><position><x>0</x><y>0</y></position><width>600</width><height>400</height></start>
<end><position><x>0</x><y>-300</y></position><width>100</width><height>100</height></end>
</level></retrieveLevel>"""

# file indices: 3 level blocks come first
CIRCLE, ROD1, ROD2, WHEEL, BOX = 3, 4, 5, 6, 7


def run(data, tmp_path, ticks=200):
    """Load data, run `ticks` ticks, save. Returns (solve, end, saved bytes)."""
    out = tmp_path / "out.fcd"
    result = subprocess.run(
        [str(BINARY), str(ticks), str(out)],
        input=data.encode() if isinstance(data, str) else data,
        capture_output=True,
        timeout=60,
    )
    assert result.returncode == 0, result.stderr.decode()
    solve, end = (int(v) for v in result.stdout.split())
    return solve, end, out.read_bytes()


def parse(data):
    magic, version, level_id, *rest = HEADER.unpack_from(data)
    areas, n = tuple(rest[:8]), rest[8]
    assert magic == b"FCDS" and version == 1
    assert len(data) == HEADER.size + n * BLOCK.size
    blocks = [BLOCK.unpack_from(data, HEADER.size + i * BLOCK.size) for i in range(n)]
    return level_id, areas, blocks


def pack(level_id, areas, blocks, version=1, magic=b"FCDS"):
    return HEADER.pack(magic, version, level_id, *areas, len(blocks)) + b"".join(
        BLOCK.pack(*b) for b in blocks
    )


@pytest.fixture
def saved(tmp_path):
    return run(XML, tmp_path)


def test_header_and_blocks(saved):
    level_id, areas, blocks = parse(saved[2])
    assert level_id == 31
    assert areas == (0, 0, 600, 400, 0, -300, 100, 100)
    assert [b[0] for b in blocks] == [0, 1, 2, 5, 11, 12, 8, 4]
    # rods are stored by their endpoints
    assert blocks[ROD1][1:6] == pytest.approx((20, 0, 120, 0, 0))


def test_joint_references(saved):
    blocks = parse(saved[2])[2]

    def refs(b):
        return (b[6], b[8] & 15), (b[7], b[8] >> 4)

    # rod 1's left end is the goal circle's first inner joint (index 5)
    assert refs(blocks[ROD1]) == ((CIRCLE, 5), (NONE, 0))
    # rod 2's top end is rod 1's right end (index 1)
    assert refs(blocks[ROD2]) == ((ROD1, 1), (NONE, 0))
    # the wheel's centre is rod 2's lower end
    assert refs(blocks[WHEEL]) == ((ROD2, 1), (NONE, 0))
    for i in (0, 1, 2, CIRCLE, BOX):
        assert refs(blocks[i]) == ((NONE, 0), (NONE, 0))


def test_round_trip_is_exact(saved, tmp_path):
    solve, end, first = saved
    again = run(first, tmp_path)
    assert again[2] == first
    assert again[:2] == (solve, end)


def test_joint_index_beats_closest_joint(saved, tmp_path):
    # rod 1's left end lies on the inner joint (20, 0), but the reference
    # says circumference joint 1, at (30, 0). The reference wins.
    level_id, areas, blocks = parse(saved[2])
    rod = list(blocks[ROD1])
    rod[8] = 1
    blocks[ROD1] = tuple(rod)
    loaded = parse(run(pack(level_id, areas, blocks), tmp_path)[2])[2]
    assert loaded[ROD1][1:3] == (30, 0)
    assert (loaded[ROD1][6], loaded[ROD1][8] & 15) == (CIRCLE, 1)


def corrupt(blocks, index, **fields):
    names = ["type", "x", "y", "w", "h", "angle", "ref0", "ref1", "joints"]
    block = list(blocks[index])
    for name, value in fields.items():
        block[names.index(name)] = value
    return blocks[:index] + [tuple(block)] + blocks[index + 1 :]


INVALID = {
    "bad version": lambda lid, a, b: pack(lid, a, b, version=2),
    "truncated": lambda lid, a, b: pack(lid, a, b)[:-1],
    "trailing byte": lambda lid, a, b: pack(lid, a, b) + b"\0",
    "level id below -1": lambda lid, a, b: pack(-2, a, b),
    "unknown type": lambda lid, a, b: pack(lid, a, corrupt(b, BOX, type=13)),
    "forward reference": lambda lid, a, b: pack(lid, a, corrupt(b, ROD1, ref0=ROD2)),
    "self reference": lambda lid, a, b: pack(lid, a, corrupt(b, ROD1, ref0=ROD1)),
    "level block reference": lambda lid, a, b: pack(lid, a, corrupt(b, ROD1, ref0=0)),
    "joint index too big": lambda lid, a, b: pack(lid, a, corrupt(b, WHEEL, joints=2)),
    "joint index without block": lambda lid, a, b: pack(
        lid, a, corrupt(b, ROD1, ref1=NONE, joints=0x15)
    ),
    "wheel second slot": lambda lid, a, b: pack(lid, a, corrupt(b, WHEEL, ref1=ROD1)),
    "box slot": lambda lid, a, b: pack(lid, a, corrupt(b, BOX, ref0=CIRCLE)),
    "infinite": lambda lid, a, b: pack(lid, a, corrupt(b, BOX, x=math.inf)),
    "nan": lambda lid, a, b: pack(lid, a, corrupt(b, BOX, angle=math.nan)),
}


@pytest.mark.parametrize("case", INVALID)
def test_invalid_files_load_as_empty(saved, tmp_path, case):
    level_id, areas, blocks = parse(saved[2])
    solve, end, data = run(INVALID[case](level_id, areas, blocks), tmp_path, 5)
    assert (solve, end) == (-1, 5)
    assert parse(data)[2] == []

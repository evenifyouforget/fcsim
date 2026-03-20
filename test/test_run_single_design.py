import subprocess
from pathlib import Path

BINARY = Path(__file__).parent.parent / 'run_single_design'

# Each case: (description, stdin_str, expected_solve_tick, expected_end_tick)
# max_ticks, num_blocks, [type_id, index, x, y, w, h, angle, joint1, joint2], build_area(x,y,w,h), goal_area(x,y,w,h)
CASES = [
    (
        'goal_rect_inside_goal_area',
        '10 1  4 1 0 0 20 20 0 -1 -1  0 0 10000 10000  0 0 10000 10000',
        1, 1,
    ),
    (
        'goal_rect_outside_goal_area',
        '10 1  4 1 5000 0 20 20 0 -1 -1  0 0 10000 10000  0 0 400 400',
        -1, 10,
    ),
    (
        'goal_rect_too_big_for_goal_area',
        '10 1  4 1 0 0 200 200 0 -1 -1  0 0 10000 10000  0 0 1 1',
        -1, 10,
    ),
]


def pytest_generate_tests(metafunc):
    if 'case' in metafunc.fixturenames:
        metafunc.parametrize('case', CASES, ids=[c[0] for c in CASES])


def test_run_single_design(case):
    description, stdin_str, expected_solve_tick, expected_end_tick = case
    result = subprocess.run(
        [str(BINARY)],
        input=stdin_str,
        capture_output=True,
        text=True,
        timeout=10,
    )
    assert result.returncode == 0, (
        f'run_single_design exited with code {result.returncode}\n'
        f'stderr: {result.stderr}'
    )
    lines = result.stdout.strip().splitlines()
    assert len(lines) == 2, f'Expected 2 output lines, got: {result.stdout!r}'
    solve_tick, end_tick = int(lines[0]), int(lines[1])
    assert solve_tick == expected_solve_tick, (
        f'solve_tick: expected {expected_solve_tick}, got {solve_tick}'
    )
    assert end_tick == expected_end_tick, (
        f'end_tick: expected {expected_end_tick}, got {end_tick}'
    )

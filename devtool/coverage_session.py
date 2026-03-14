"""
Coverage improvement session manager for stl_mock sources.

Usage:
    python devtool/coverage_session.py <command> [options]

Commands:
    clear     Reset session; measure current coverage as the new baseline.
    pick      Suggest a single uncovered region to target next.
    grade     Check whether current changes improve on the baseline.
    snapshot  Lock in gains: bump thresholds, update baseline, auto-commit.
    revert    Discard uncommitted changes, returning to the last snapshot.
    export    Print a GitHub Markdown audit log of all session improvements.

Typical workflow:
    python devtool/coverage_session.py clear
    python devtool/coverage_session.py pick
    # ... edit test file ...
    python devtool/coverage_session.py grade
    python devtool/coverage_session.py snapshot
    python devtool/coverage_session.py pick
    # ... repeat ...
    python devtool/coverage_session.py export
"""

import argparse
import json
import re
import shutil
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

# ── paths ──────────────────────────────────────────────────────────────────────

ROOT = Path(__file__).resolve().parent.parent
STATE_DIR = ROOT / ".cov-state"
BASELINE_FILE = STATE_DIR / "baseline.json"
LOG_FILE = STATE_DIR / "log.jsonl"
PICKED_FILE = STATE_DIR / "picked.json"

TEST_COV_PY = ROOT / "test" / "test_coverage.py"
PROFDATA = ROOT / "coverage_profiles" / "merged.profdata"
SUMMARY_TXT = ROOT / "coverage_profiles" / "summary.txt"
BINARY = ROOT / "stl_test_cov"

SOURCES = sorted(
    [
        *ROOT.glob("src/stl_mock*.cpp"),
        *ROOT.glob("arch/wasm/include/stl_mock*.h"),
    ]
)

METRICS = ("region", "function", "line", "branch")

# ── llvm tool discovery ────────────────────────────────────────────────────────


def _find_llvm_tool(name):
    r = subprocess.run(["clang", "--version"], capture_output=True, text=True)
    m = re.search(r"version (\d+)", r.stdout)
    if m:
        versioned = f"{name}-{m.group(1)}"
        if shutil.which(versioned):
            return versioned
    if shutil.which(name):
        return name
    for v in range(25, 13, -1):
        versioned = f"{name}-{v}"
        if shutil.which(versioned):
            return versioned
    raise RuntimeError(f"Could not find {name} or any versioned variant")


# ── build / test ───────────────────────────────────────────────────────────────


def _build():
    print("── building stl_test_cov ─────────────────────────────────────────────")
    r = subprocess.run(["scons", "stl_test_cov"], cwd=ROOT)
    if r.returncode != 0:
        sys.exit("Build failed.")


def _run_coverage_tests():
    print("── running coverage tests ────────────────────────────────────────────")
    r = subprocess.run(["pytest", "test/test_coverage.py", "-v"], cwd=ROOT)
    if r.returncode != 0:
        sys.exit("Coverage tests failed.")


def _measure():
    """Build, run tests, return parsed coverage numbers."""
    _build()
    _run_coverage_tests()
    return _parse_summary()


def _parse_summary():
    text = SUMMARY_TXT.read_text()
    total = next(l for l in text.splitlines() if l.startswith("TOTAL"))
    pcts = [float(x) for x in re.findall(r"(\d+\.\d+)%", total)]
    if len(pcts) < 4:
        sys.exit("Could not parse coverage percentages from summary.txt.")
    return dict(zip(METRICS, pcts))


# ── state helpers ──────────────────────────────────────────────────────────────


def _ensure_state_dir():
    STATE_DIR.mkdir(exist_ok=True)


def _read_baseline():
    if not BASELINE_FILE.exists():
        sys.exit("No session baseline found. Run `clear` first.")
    return json.loads(BASELINE_FILE.read_text())


def _write_baseline(numbers):
    _ensure_state_dir()
    BASELINE_FILE.write_text(json.dumps(numbers, indent=2) + "\n")


def _read_log():
    if not LOG_FILE.exists():
        return []
    return [json.loads(l) for l in LOG_FILE.read_text().splitlines() if l.strip()]


def _append_log(entry):
    _ensure_state_dir()
    with LOG_FILE.open("a") as f:
        f.write(json.dumps(entry) + "\n")


def _read_picked():
    if not PICKED_FILE.exists():
        return []
    return json.loads(PICKED_FILE.read_text())


def _write_picked(picked):
    _ensure_state_dir()
    PICKED_FILE.write_text(json.dumps(picked, indent=2) + "\n")


# ── threshold editing ──────────────────────────────────────────────────────────


def _bump_thresholds(numbers):
    """Rewrite the THRESHOLDS dict in test_coverage.py to match numbers."""
    text = TEST_COV_PY.read_text()
    for metric, value in numbers.items():
        text = re.sub(
            rf'("{metric}":\s*)[\d.]+',
            rf"\g<1>{value:.2f}",
            text,
        )
    TEST_COV_PY.write_text(text)


# ── coverage number utilities ──────────────────────────────────────────────────


def _delta(old, new):
    return {m: round(new[m] - old[m], 2) for m in METRICS}


def _is_nondecreasing(baseline, current):
    return all(current[m] >= baseline[m] for m in METRICS)


def _fmt_numbers(numbers):
    return "  ".join(f"{m}: {numbers[m]:.2f}%" for m in METRICS)


def _fmt_delta(d):
    parts = []
    for m in METRICS:
        sign = "+" if d[m] >= 0 else ""
        parts.append(f"{m}: {sign}{d[m]:.2f}pp")
    return "  ".join(parts)


# ── uncovered block discovery ──────────────────────────────────────────────────

# Lines within this many covered lines of each other are merged into one block.
_BLOCK_GAP = 3


def _get_uncovered_blocks():
    """
    Return a list of uncovered blocks across all source files, sorted by size
    descending (largest gap first, so PICK targets the highest-impact region).

    Each block is a dict:
        file  -- str, path relative to repo root
        start -- int, first uncovered line number (1-indexed)
        end   -- int, last uncovered line number (1-indexed)
        lines -- list of (lineno, source_text) for uncovered lines in the block
    """
    if not PROFDATA.exists():
        sys.exit(
            "No profile data found. Run `clear` first (or `snapshot` after a previous session)."
        )

    llvm_cov = _find_llvm_tool("llvm-cov")
    all_blocks = []

    for source in SOURCES:
        rel = str(source.relative_to(ROOT))
        result = subprocess.run(
            [llvm_cov, "show", str(BINARY), f"-instr-profile={PROFDATA}", str(source)],
            capture_output=True,
            text=True,
            cwd=ROOT,
        )

        # llvm-cov show line format: "   lineno|   count|source text"
        # Non-executable lines have an empty/whitespace count field.
        zero_lines = []
        for raw in result.stdout.splitlines():
            m = re.match(r"\s*(\d+)\|\s*([\d.]+[kKmMbBgGtT]*)\|(.*)$", raw)
            if not m:
                continue
            lineno = int(m.group(1))
            count_str = m.group(2).strip()
            src_text = m.group(3)
            if count_str == "0":
                zero_lines.append((lineno, src_text))

        if not zero_lines:
            continue

        # Group contiguous zero-count lines into blocks, tolerating small gaps
        # of covered lines between them (helps group related uncovered branches).
        blocks = []
        cur = [zero_lines[0]]
        for prev, item in zip(zero_lines, zero_lines[1:]):
            if item[0] - prev[0] <= _BLOCK_GAP + 1:
                cur.append(item)
            else:
                blocks.append(cur)
                cur = [item]
        blocks.append(cur)

        for block in blocks:
            all_blocks.append(
                {
                    "file": rel,
                    "start": block[0][0],
                    "end": block[-1][0],
                    "lines": block,
                }
            )

    all_blocks.sort(key=lambda b: len(b["lines"]), reverse=True)
    return all_blocks


# ── git helpers ────────────────────────────────────────────────────────────────


def _git(*args, **kwargs):
    return subprocess.run(["git", *args], cwd=ROOT, **kwargs)


def _git_is_dirty():
    # Only check tracked file modifications; untracked files are unaffected by
    # `git restore .` and shouldn't block CLEAR.
    staged = _git("diff", "--cached", "--quiet", capture_output=True)
    unstaged = _git("diff", "--quiet", capture_output=True)
    return staged.returncode != 0 or unstaged.returncode != 0


def _git_head_sha():
    r = _git("rev-parse", "HEAD", capture_output=True, text=True)
    return r.stdout.strip() if r.returncode == 0 else None


# ── commands ───────────────────────────────────────────────────────────────────


def cmd_clear(args):
    if _git_is_dirty() and not args.force:
        sys.exit(
            "Working tree is dirty.\n"
            "Commit or stash your changes, or use --force to discard them."
        )
    if _git_is_dirty():
        print("Discarding local changes...")
        _git("restore", ".")

    numbers = _measure()
    _bump_thresholds(numbers)
    _ensure_state_dir()
    _write_baseline(numbers)
    LOG_FILE.write_text("")
    _write_picked([])

    print("\n── session cleared ───────────────────────────────────────────────────")
    print(f"Baseline: {_fmt_numbers(numbers)}")


def cmd_pick(args):
    picked = _read_picked()
    picked_keys = {(p["file"], p["start"]) for p in picked}
    blocks = _get_uncovered_blocks()

    remaining = [b for b in blocks if (b["file"], b["start"]) not in picked_keys]
    if not remaining:
        print("All uncovered regions have already been suggested.")
        print("Either coverage is complete, or use `revert` to reset suggestions.")
        return

    choice = remaining[0]
    picked.append({"file": choice["file"], "start": choice["start"]})
    _write_picked(picked)

    n_uncovered = len(choice["lines"])
    print("\n── pick ──────────────────────────────────────────────────────────────")
    print(f"File:   {choice['file']}")
    print(
        f"Region: lines {choice['start']}–{choice['end']} ({n_uncovered} uncovered)\n"
    )

    source_lines = (ROOT / choice["file"]).read_text().splitlines()
    ctx_start = max(1, choice["start"] - 3)
    ctx_end = min(len(source_lines), choice["end"] + 3)
    uncovered_linenos = {ln for ln, _ in choice["lines"]}

    for lineno in range(ctx_start, ctx_end + 1):
        marker = ">>>" if lineno in uncovered_linenos else "   "
        print(f"  {marker} {lineno:4d}  {source_lines[lineno - 1]}")
    print()


def cmd_grade(args):
    baseline = _read_baseline()
    numbers = _measure()
    d = _delta(baseline, numbers)
    ok = _is_nondecreasing(baseline, numbers)

    print("\n── grade ─────────────────────────────────────────────────────────────")
    print(f"Baseline: {_fmt_numbers(baseline)}")
    print(f"Current:  {_fmt_numbers(numbers)}")
    print(f"Delta:    {_fmt_delta(d)}")

    if ok:
        if any(d[m] > 0 for m in METRICS):
            print("\nPASS — coverage improved. Run `snapshot` to lock it in.")
        else:
            print("\nPASS (no change) — coverage held. No new improvement to snapshot.")
    else:
        regressions = [m for m in METRICS if numbers[m] < baseline[m]]
        print(f"\nFAIL — regression in: {', '.join(regressions)}")
        sys.exit(1)


def cmd_snapshot(args):
    baseline = _read_baseline()
    numbers = _measure()
    d = _delta(baseline, numbers)

    if not _is_nondecreasing(baseline, numbers):
        regressions = [m for m in METRICS if numbers[m] < baseline[m]]
        sys.exit(
            f"Coverage regressed in: {', '.join(regressions)}. "
            "Fix the regression or run `revert` before snapshotting."
        )

    if not any(d[m] > 0 for m in METRICS):
        sys.exit("No improvement over baseline. Nothing to snapshot.")

    # Compute a delta string for the commit message.
    delta_str = ", ".join(f"+{d[m]:.2f}pp {m}" for m in METRICS if d[m] > 0)
    message = args.message or delta_str
    commit_msg = f"Coverage: {message}"

    _bump_thresholds(numbers)
    _write_baseline(numbers)

    _git("add", "-u")
    r = _git("commit", "-m", commit_msg)
    if r.returncode != 0:
        sys.exit("git commit failed.")

    sha = _git_head_sha()

    log_entry = {
        "n": len(_read_log()) + 1,
        "timestamp": datetime.now(timezone.utc).isoformat(),
        "baseline_before": baseline,
        "numbers": numbers,
        "delta": d,
        "message": message,
        "commit": sha,
    }
    _append_log(log_entry)
    _write_picked([])  # suggestions are stale after coverage changes

    print("\n── snapshot ──────────────────────────────────────────────────────────")
    print(f"Delta:   {_fmt_delta(d)}")
    print(f"Current: {_fmt_numbers(numbers)}")
    print(f"Commit:  {sha[:7] if sha else '?'}  {commit_msg}")


def cmd_revert(args):
    picked = _read_picked()
    if picked:
        picked.pop()
        _write_picked(picked)

    _git("restore", ".")
    print("Reverted uncommitted changes.")
    print("Working tree restored to last snapshot (HEAD).")


def cmd_export(args):
    log = _read_log()
    if not log:
        print("No snapshots recorded yet. Nothing to export.")
        return

    def pp(d, m):
        v = d[m]
        if v > 0:
            return f"+{v:.2f}pp"
        if v < 0:
            return f"{v:.2f}pp"
        return "—"

    lines = [
        "## Coverage Improvements",
        "",
        "| # | Commit | Region | Function | Line | Branch | Notes |",
        "|---|--------|--------|----------|------|--------|-------|",
    ]
    for entry in log:
        n = entry["n"]
        sha = entry.get("commit") or ""
        sha_str = sha[:7] if sha else "—"
        d = entry["delta"]
        msg = entry.get("message", "")
        row = (
            f"| {n} | `{sha_str}` "
            f"| {pp(d, 'region')} | {pp(d, 'function')} "
            f"| {pp(d, 'line')} | {pp(d, 'branch')} "
            f"| {msg} |"
        )
        lines.append(row)

    first_baseline = log[0]["baseline_before"]
    last_numbers = log[-1]["numbers"]
    total_d = _delta(first_baseline, last_numbers)

    lines += [
        "",
        "### Total improvement this session",
        "",
        _fmt_delta(total_d),
    ]

    output = "\n".join(lines)
    if args.output:
        Path(args.output).write_text(output + "\n")
        print(f"Written to {args.output}")
    else:
        print(output)


# ── CLI ────────────────────────────────────────────────────────────────────────


def main():
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    sub = parser.add_subparsers(dest="command", required=True)

    p_clear = sub.add_parser("clear", help="Reset session state.")
    p_clear.add_argument(
        "--force",
        action="store_true",
        help="Discard local changes without prompting.",
    )

    sub.add_parser("pick", help="Suggest a single uncovered region to target.")

    sub.add_parser("grade", help="Check if current changes improve coverage.")

    p_snap = sub.add_parser("snapshot", help="Lock in gains and auto-commit.")
    p_snap.add_argument(
        "message",
        nargs="?",
        default=None,
        help="Optional commit message (default: auto-generated from delta).",
    )

    sub.add_parser(
        "revert", help="Discard uncommitted changes, return to last snapshot."
    )

    p_export = sub.add_parser("export", help="Print GitHub Markdown audit log.")
    p_export.add_argument(
        "--output",
        metavar="FILE",
        help="Write to FILE instead of stdout.",
    )

    args = parser.parse_args()
    {
        "clear": cmd_clear,
        "pick": cmd_pick,
        "grade": cmd_grade,
        "snapshot": cmd_snapshot,
        "revert": cmd_revert,
        "export": cmd_export,
    }[args.command](args)


if __name__ == "__main__":
    main()

# Contributing to fcsim

This guide covers the developer workflow. For architecture, terminology and the data model, see [README.md](README.md). This file links to it rather than repeating it.

## Environment setup

Only Linux is supported. Dependencies are installed with `apt`, so Ubuntu and distros based on it work out of the box. See [README § Build](README.md#build).

```sh
bash install_requirements.sh   # apt packages from requirements.system, prettier via npm, Python venv in .venv
source .venv/bin/activate      # do this in every new shell before building or testing
```

`install_requirements.sh` installs these Python tools into `.venv`: `scons`, `pytest`, `pytest-xdist`, `ruff` and `prek`.

## Building

`scons` with no arguments builds every target. You can also name targets to build only those, for example `scons fcsim html/fcsim.wasm`.

| Target                  | What it is                                                                                                   |
| ----------------------- | ------------------------------------------------------------------------------------------------------------ |
| `html/fcsim.wasm`       | The web build (clang → wasm32, linked with `wasm-ld`). This is the main product. Serve `html/` to run it (README § Running the web build locally). |
| `fcsim`                 | Native X11/GL GUI build, `794-stable`                                                                         |
| `fcsim-fpatan`          | Native GUI build with `USE_FPATAN` (x86 `fpatan` for `atan2`)                                                 |
| `run_single_design`     | Headless CLI that reads a design from stdin and prints the solve tick and end tick. ftlib uses it.           |
| `run_single_design_xml` | Headless CLI that takes an XML design                                                                         |
| `stl_test_asan`         | Mock-STL tests under AddressSanitizer + UBSan                                                                 |
| `stl_test_msan`         | Mock-STL tests under MemorySanitizer + UBSan                                                                  |
| `stl_test_cov`          | Mock-STL tests with llvm-cov instrumentation                                                                  |

`scons -c` cleans. If the build behaves oddly, clean and rebuild.

`fcsim` and `fcsim-fpatan` differ in intent: the first targets the portable `794-stable` spectre, the second moves closer to `794-target`. The reasons are in [README § `fcsim` vs `fcsim-fpatan` and others](README.md#fcsim-vs-fcsim-fpatan-and-others). A change that alters simulation results in either build changes a spectre, so call it out in the PR.

The multi-branch web deployment (`build_subdirs.sh`, `deploy.json`) is described in [README § Multi-branch build](README.md#multi-branch-build).

## Testing

All tests run through pytest from the repository root, after `scons` has built the binaries they use.

```sh
pytest test/test_stl.py -n auto            # mock STL under ASan+UBSan and MSan+UBSan (needs stl_test_asan, stl_test_msan)
pytest test/test_coverage.py               # mock STL coverage, fails below the floors in the file; writes coverage_html/ (needs stl_test_cov)
pytest test/test_run_single_design.py -v   # headless simulation smoke tests (needs run_single_design)
```

`test_stl.py` runs every registered mock-STL test case in both sanitizer binaries. When coverage improves, raise the floors in `test/test_coverage.py`, as the PR template asks. There is no separate valgrind step in this repo. Valgrind is installed for ftlib's test suite in the reverse-integration job below.

The browser is the only place to check the web build's custom `malloc` and memory use. See [README § Memory](README.md#memory).

### Reverse integration (ftlib)

[ftlib](https://github.com/evenifyouforget/ftlib) runs a large corpus of real designs against `run_single_design` and compares solve results with the original game. On every PR, `.github/workflows/reverse-integration-sanity.yml` does the following:

1. Checks out ftlib.
2. Replaces ftlib's `fcsim` submodule with a symlink to your PR's checkout.
3. Runs `pytest test --all`.
4. Fails if the pass rate is below `min_pass_rate`.

That file holds the authoritative value, currently **80.7%**. The job also fails on pytest collection or session errors, or if the fcsim backend tests did not run.

The pass rate is not expected to be 100%. Some designs depend on behaviour fcsim does not reproduce yet. If your change raises the pass rate, bump `min_pass_rate` in the same PR so the gain is locked in. If it lowers the rate, explain why in the PR. The PR template asks for this.

To run it locally, check out ftlib next to fcsim, replace `ftlib/fcsim` with a symlink to your fcsim checkout, and run `python3 -m pytest -n auto test --all` in ftlib.

## Dev tools (`devtool/`)

| Script                     | Purpose                                                                                                                                                                                    |
| -------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `add_guard.py`             | "Check if the specified files have an include guard, add it if not present. Might sometimes get it wrong for difficult cases."                                                            |
| `find_unused_warnings.py`  | "Run scons and find warnings that are not triggered but exist in gcc." Use it to find warnings that could be promoted to `-Werror` at no cost.                                             |
| `delete_line_and_test.py`  | Line-deletion minimiser: deletes one line at a time from a file and keeps each deletion that doesn't increase the failure count. Run it on a clean git tree and review `git diff` afterwards, because it also removes comments and blank lines. |
| `coverage_session.py`      | Session manager for raising mock-STL coverage (`clear`, `pick`, `grade`, `snapshot`, `revert`, `export`). Run with `--help` for the workflow.                                           |

## Code style

Style is enforced by [prek](https://github.com/j178/prek), a pre-commit-compatible runner, using `.pre-commit-config.yaml`:

- **C/C++:** `clang-format`. There is no `.clang-format` file, so it uses clang-format's default (LLVM) style. Don't hand-format; let the tool decide.
- **JS/HTML/CSS/JSON:** `prettier`
- **Python:** `ruff` (lint, with `--fix`) and `ruff-format`
- Trailing whitespace, end-of-file newline, YAML syntax, and large-file checks

```sh
prek run --all-files             # or: prek run --files <changed files>
```

Formatters rewrite files in place. Run prek again until it passes, and commit the result. CI (`lint.yml`) runs `prek run --all-files` and fails if any hook would change a file.

The build uses `-Werror` for a selected set of warnings (see `SConstruct`). New code must not trigger them.

## Pull requests

- Target `main`. Pushes to `main` deploy the web build to GitHub Pages (`static.yml`).
- Fill in `pull_request_template.md`: the issues closed (`Closes #N`), the test checklist, and AI-usage transparency.
- Label the PR appropriately.
- One PR per issue is preferred, unless the changes are intrinsically linked.
- Stacked PRs should be used when there is a dependency.

CI must pass before merge, and this is enforced. These are the checks on a PR:

| Workflow                         | Checks                                                                                  |
| -------------------------------- | --------------------------------------------------------------------------------------- |
| `lint.yml`                       | `prek run --all-files` makes no changes                                                 |
| `scons-sanity.yml`               | Full `scons` build succeeds, and `test/test_run_single_design.py` passes                 |
| `stl-test.yml`                   | `test/test_stl.py` under ASan/MSan, plus a coverage report posted as a PR comment       |
| `reverse-integration-sanity.yml` | ftlib pass rate ≥ `min_pass_rate` (see [Reverse integration](#reverse-integration-ftlib)) |

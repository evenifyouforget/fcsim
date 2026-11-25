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

That file holds the authoritative value, currently **86.0%** (#197). It sits just under what `main` scored on the CI runner when it was set (about 86.8%, lower than ftlib's own reference snapshot, which scores about 90%). The job also fails on pytest collection or session errors, or if the fcsim backend tests did not run.

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
- Label the PR appropriately, and apply every label that fits.
- Put the PR in the milestone of the release it is meant for, if you can. If the work can't be finished in that milestone, cancel it or move it to a later one. **Why:** Keeps GitHub's tracker accurate and lets reviewers navigate.
- One PR per issue is preferred, unless the changes are intrinsically linked.
- Stacked PRs should be used when there is a dependency.
- Keep the diff minimal. PRs with superfluous edits (reformatting, unrelated cleanups) will not be accepted. **Why:** Superfluous edits strain reviewers.
- A PR that is not meant to be merged is marked Draft and has "DO NOT MERGE" in the title. **Why:** Prevents wasting reviewer time.
- A human reads every PR, including the diff, and approves it by a review, a comment, or by enabling auto-merge before it merges. **Why:** Maintains quality standards.
- A PR that is no longer relevant should be closed. **Why:** Reduces clutter.
- Do not have more than 5 PRs open at once. This applies to PRs opened from now on; PRs already open when the rule was introduced are not counted against you. **Why:** More PRs than that will be bottlenecked on human review anyway, and it creates clutter.
- A PR for fcsim and a PR for ftlib that depend on each other must not be open at the same time.

CI must pass before merge, and this is enforced. These are the checks on a PR:

| Workflow                         | Checks                                                                                  |
| -------------------------------- | --------------------------------------------------------------------------------------- |
| `lint.yml`                       | `prek run --all-files` makes no changes                                                 |
| `scons-sanity.yml`               | Full `scons` build succeeds, and `test/test_run_single_design.py` passes                 |
| `stl-test.yml`                   | `test/test_stl.py` under ASan/MSan, plus a coverage report posted as a PR comment       |
| `reverse-integration-sanity.yml` | ftlib pass rate ≥ `min_pass_rate` (see [Reverse integration](#reverse-integration-ftlib)) |

## Issues and communication

1. When you identify new work, open an Issue first. **Why:** Informs collaborators that there is work to be done, and invites them to take it.
2. When you start work, comment on the Issue with what you plan to deliver (a PR, or something else) and an internal deadline. **Why:** Prevents duplicated effort, and tells collaborators who to offer help to.
3. A PR that closes an Issue says so with `Closes #N`.
4. If the Issue is still open after the PR merges, close it manually with a comment.

We are stricter about this for AI than for humans.

- Use Discord for long or urgent conversations, since it's a chat platform. Use GitHub for everything else. **Why:** Comments there last and are easy to search and reference.
- If your work would conflict with someone else's, work with them to pick one. Offer to drop yours before asking them to drop theirs. **Why:** Two people supporting one line of work beats two PRs where the first merge invalidates or conflicts with the second.
- Measurable claims must actually be measured before you make them publicly. **Why:** Reduces misinformation.
- Do not check off a template box you did not do. **Why:** Honesty matters more than checkmarks.
- To call a bug debunked, state clearly that the same conditions fail to reproduce it, or that it is already fixed, and include the evidence in the comment that closes the Issue.
- Never push sensitive data such as account login tokens. If you do by accident, delete it as soon as possible.
- Issues are triaged by the maintainer. Do not triage old Issues or assign them milestones unless asked. **Why:** Scoping decisions usually leave the next 2 or 3 releases full of work already.

## AI-assisted contributions

AI-written work is welcome under the same rules as everyone's, plus these. AI agents often post from a human's account, so they have to be easy to tell apart.

- Sign every Issue, PR and comment at the bottom: `Posted by AI (model) (nickname)`. If no nickname was given, pick one and keep using it for that user (in the agent's memory or a gitignored file), or per session if that isn't possible. **Why:** Shows what is AI-authored even on a shared account, and tells collaborators when someone runs several agents that don't share context.
- Apply the `written-and-posted-by-ai` label to every Issue and PR it opens.
- AI must not merge a PR, close a human-authored Issue or PR, manually trigger a workflow or CI run (the automatic runs on a PR are fine), or make a release.
- A PR an AI opens as Draft is not yet asking for human review. The AI marks it ready when it is blocked on a human decision, including when it thinks the PR is safe to merge. If a human wants further edits they say so in a review or comment, and if they posted from the same account they mark the PR Draft again.
- If CI fails for reasons unrelated to the PR, the AI comments why on the PR and on the relevant Issue (creating one if needed), says the PR is blocked, and stops working on it.
- The AI instructions are kept in a local, untracked `AGENTS.md` (listed in `.gitignore`); this section is what contributors can rely on. See #193 for whether to commit one.

## Branches

Branches created by AI that have not been touched for 2 weeks are still needed only if a variant build, an open Issue or an open PR uses them. Any others are listed and offered to the maintainer to delete, and nothing is deleted without their approval. If a branch's artifacts are still useful, keep a copy locally or move them onto a current branch, so the remote branch can be deleted. The same standard is not applied to humans' branches. **Why:** Those are sometimes historically relevant, hold artifacts that don't belong on `main`, or are referenced in Discord.

Before deleting a local branch, check whether it holds anything useful. **Why:** A PR sometimes merges first and a later commit never reaches `main`.

## Releases

Official releases on GitHub are made by evenifyouforget.

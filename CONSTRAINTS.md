# Constraints

This file is the quality bar for Codicier. Each change must meet it. Do not weaken a rule to make a change pass. Fix the code, or add an exception with an owner and an end date.

Last reviewed: 2026-10-18.

## How to run the checks

| Command | When | What it runs |
|---|---|---|
| `sh tools/check.sh fast` | After each edit | Build and `ctest` |
| `sh tools/check.sh task` | Before each commit | `fast`, the floor guard, coverage, and `clang-tidy` |

CI runs `tools/check.sh task` on each push to `main` and on each pull request. A failed check blocks the change. A task check must finish in 90 seconds or less on the dev machine. A slower check runs in CI only.

## Floor

These rules apply to every change. `tools/floor_guard.py` checks them in the diff.

- No new suppressions: `NOLINT`, `clang-format off`, `qmllint disable`, `#pragma ... diagnostic ignored`, or `-Wno-` flags.
- No stubs: `TODO`, `FIXME`, `Q_UNIMPLEMENTED`, "not implemented", or an empty `catch` block.
- No skipped tests: `QSKIP`, `QEXPECT_FAIL`, `skip()`, `expectFail()`, or a disabled test.
- No deleted tests and no removed assertions in a test that stays.
- No secrets in source. The guard reports the file, never the value.
- No change to a rule in this file in the same commit as the code that fails it.
- No network code in the app. The app works offline and sends no data.

## Enforced with numbers

| Dimension | Rule | Checked by | Runs at |
|---|---|---|---|
| Compiler warnings | Zero warnings with `-Wall -Wextra` | `cmake --build build` | each edit |
| Unit and QML tests | All tests pass | `ctest --test-dir build` | each edit |
| Offline | No network class in `CMakeLists.txt`, `src`, or `qml`. Every non-local URL is blocked. | `check_offline`, `tst_offline` | each edit |
| C++ format | Matches `.clang-format` | `check_format` | each edit |
| QML lint | Zero `qmllint` warnings | `check_qmllint` | each edit |
| Static analysis | Zero `clang-tidy` findings | `clang-tidy -p build src/*.cpp tests/*.cpp` | each commit |
| Coverage | At least 80% of changed executable lines in `src` have a test | `tools/coverage_diff.py build-cov` | each commit |
| Editor speed | A key press shows on screen in 16 ms or less at the 95th percentile, in a chapter of 100,000 words | `check_editor_speed` | each edit |
| Scroll speed | A scroll step shows on screen in 16 ms or less at the 95th percentile, in a chapter of 100,000 words | `check_editor_speed` | each edit |

Reasons for the numbers:

- **Zero warnings and zero findings.** The codebase starts clean. A count above zero grows and then nobody reads it.
- **80% coverage of changed lines.** This number forces a test for each new function. It still allows a line that no test can reach. `src/main.cpp` only starts the app, so the check does not measure it.
- **16 ms per key press.** A 60 Hz screen shows a new frame every 16.7 ms. A key press that takes longer misses a frame, and the writer can feel the delay. `spike/probe` types 300 keys into a chapter of 100,000 words and times each key to the next frame. It turns off vsync, so the time is the work and not the wait for the screen.
- **16 ms per scroll step.** The same frame limit applies to scroll, so scroll runs at 60 frames per second. The probe scrolls 300 steps of 40 pixels in the same chapter.

## Planned

These rules have a number but no check yet. Each one becomes enforced in the task that adds its check.

| Dimension | Rule | Check added in |
|---|---|---|

No rule is planned now.

## Exceptions

| ID | Rule | Path | Reason | Owner | Expires |
|---|---|---|---|---|---|
| E4 | assertion-removed | tests/tst_backup.cpp | The pruning now keeps only zips of the same project, so the test uses real zips in place of empty fake files. | Orpheus-21 | 2026-10-27 |
| E5 | assertion-removed | tests/tst_projectlist.cpp | The project list gets an author role, so the role count goes from 11 to 12. | Orpheus-21 | 2026-10-28 |
| E6 | assertion-removed | tests/qml/tst_theme.qml | Three new themes (night, mist, contrast), so the list of theme names grows from three to six. | Orpheus-21 | 2026-10-29 |
| E7 | assertion-removed | tests/tst_scrivener.cpp | The import adds the note "Imported from Scrivener" before the research notes, so the indices of the research items in the test move up by one. | Orpheus-21 | 2026-10-30 |
| E8 | assertion-removed | tests/qml/tst_editor.qml | Rarely used commands have no default key now (the keys clashed with input methods and desktops), so "every command has a key" is no longer the rule; a new test checks that no key clashes. | Orpheus-21 | 2026-11-06 |
| E9 | assertion-removed | tests/qml/tst_editor.qml | The status bar counts now end in "words" (local #41), so the selection count reads "Selection 4 words". | Orpheus-21 | 2026-11-06 |

An exception lasts 90 days at most.

The floor guard reads this table. A row allows the findings of one guard rule, such as `assertion-removed`, for the files that match its path, until the end of its expiry date. The rules that a row can name are `silenced-checker`, `unfinished-work`, `test-made-easier`, `test-deleted`, and `assertion-removed`. A secret and a change to a rule in this file can never have an exception. A row with no owner, with no valid date, or with more than 90 days left is itself a violation.

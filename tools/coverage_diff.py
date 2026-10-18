#!/usr/bin/env python3
"""Check that the changed lines in src/ are covered by tests.

Build with --coverage and run ctest first. Then:
Usage: coverage_diff.py BUILD_DIR [--base REF] [--min PERCENT]
Exit codes: 0 pass, 1 coverage below the minimum, 2 the check could not run.
"""
import argparse
import json
import os
import pathlib
import re
import subprocess
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from floor_guard import bail, collect_diff, git

HUNK = re.compile(r"^@@ -\d+(?:,\d+)? \+(\d+)(?:,(\d+))? @@")
# main.cpp only starts the app. Tests cannot run it, so it is not measured.
APP_SOURCE = re.compile(r"^src/(?!main\.cpp$).*\.(cpp|h)$")


def changed_lines(diff):
    """Return {path: set of new line numbers} for app sources in a unified diff."""
    changed = {}
    path = None
    for line in diff.splitlines():
        if line.startswith("+++ "):
            name = re.sub(r"^b/", "", line[4:])
            path = name if APP_SOURCE.match(name) else None
        elif path and (match := HUNK.match(line)):
            start, count = int(match[1]), int(match[2] or 1)
            changed.setdefault(path, set()).update(range(start, start + count))
    return changed


def line_counts(build_dir):
    """Return {absolute path: {line: count}}, summed over all object files.

    Each .gcno file counts, so a source that no test runs shows 0, not nothing.
    """
    counts = {}
    for gcno in pathlib.Path(build_dir).rglob("*.gcno"):
        result = subprocess.run(["gcov", "--json-format", "--stdout", str(gcno)],
                                cwd=build_dir, capture_output=True, text=True)
        if result.returncode != 0:
            bail(f"gcov failed on {gcno}")
        for doc in filter(None, result.stdout.splitlines()):
            for source in json.loads(doc)["files"]:
                lines = counts.setdefault(source["file"], {})
                for entry in source["lines"]:
                    number = entry["line_number"]
                    lines[number] = lines.get(number, 0) + entry["count"]
    return counts


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("build_dir")
    parser.add_argument("--base", default="origin/main")
    parser.add_argument("--min", type=float, default=80.0)
    args = parser.parse_args()

    build_dir = os.path.abspath(args.build_dir)
    top = git("rev-parse", "--show-toplevel")
    if top is None:
        bail("not inside a git work tree")
    os.chdir(top.strip())

    counts = line_counts(build_dir)
    if not counts:
        bail(f"no coverage data in {build_dir}; build with --coverage and run ctest")

    executable = covered = 0
    missed = []
    for path, numbers in sorted(changed_lines(collect_diff(args.base)).items()):
        lines = counts.get(os.path.abspath(path), {})
        for number in sorted(numbers & lines.keys()):
            executable += 1
            if lines[number] > 0:
                covered += 1
            else:
                missed.append(f"{path}:{number}")

    if executable == 0:
        print("coverage: no changed executable lines in src/")
        return 0
    percent = 100.0 * covered / executable
    print(f"coverage: {covered} of {executable} changed lines covered ({percent:.1f}%)")
    if percent >= args.min:
        return 0
    print(f"coverage: below the minimum of {args.min:.0f}%. Lines with no test:", file=sys.stderr)
    for location in missed:
        print(f"  {location}", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())

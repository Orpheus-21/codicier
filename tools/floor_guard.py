#!/usr/bin/env python3
"""Check a diff for moves that lower the quality bar in CONSTRAINTS.md.

Usage: floor_guard.py [--base REF]   (default base: origin/main)
An active row in the Exceptions table of CONSTRAINTS.md allows a finding for
one rule and one path pattern until its expiry date.
Exit codes: 0 clean, 1 violation found, 2 the guard could not run.
"""
import argparse
import datetime
import fnmatch
import os
import re
import subprocess
import sys

SUPPRESSION = re.compile(
    r"NOLINT|clang-format off|qmllint disable|#pragma (GCC|clang) diagnostic ignored|-Wno-")
STUB = re.compile(
    r"\bTODO\b|\bFIXME\b|catch\s*\([^)]*\)\s*\{\s*\}|Q_UNIMPLEMENTED|[Nn]ot implemented")
SKIP = re.compile(r"QSKIP|QEXPECT_FAIL|\bskip\(|expectFail\(|\bDISABLED\b")
ASSERTION = re.compile(r"QCOMPARE|QVERIFY|QTRY_|\bcompare\(|\bverify\(|\btryCompare\(")
SECRET = re.compile(
    r"-----BEGIN [A-Z ]*PRIVATE KEY-----|gh[pousr]_[A-Za-z0-9]{20,}|AKIA[0-9A-Z]{16}"
    r"|xox[abprs]-[A-Za-z0-9-]{10,}")

# The user git config can change the diff prefixes, so set them here.
PREFIXES = ("--src-prefix=a/", "--dst-prefix=b/")

# These files hold the patterns or the rules as text, so pattern checks skip them.
PATTERN_EXEMPT = {"tools/floor_guard.py", "tests/tst_floor_guard.py", "CONSTRAINTS.md"}


def git(*args, ok_codes=(0,)):
    result = subprocess.run(["git", *args], capture_output=True, text=True)
    if result.returncode not in ok_codes:
        return None
    return result.stdout


def bail(message):
    print(f"floor-guard: {message}", file=sys.stderr)
    sys.exit(2)


def collect_diff(base):
    merge_base = git("merge-base", base, "HEAD")
    if merge_base is None:
        bail(f"no merge base against {base}")
    tracked = git("diff", *PREFIXES, "--unified=0", merge_base.strip(), "--")
    if tracked is None:
        bail("could not diff against the merge base")
    untracked = git("ls-files", "--others", "--exclude-standard")
    if untracked is None:
        bail("could not list untracked files")
    parts = [tracked]
    for path in filter(None, untracked.splitlines()):
        # git diff --no-index exits with 1 when the files differ, which is normal here.
        part = git("diff", *PREFIXES, "--no-index", "--unified=0", "/dev/null", path,
                   ok_codes=(0, 1))
        if part is None:
            bail(f"could not diff the untracked file {path}")
        parts.append(part)
    return "\n".join(parts)


def parse(diff):
    added, removed, deleted = [], [], []
    path = old_path = ""
    in_header = False
    for line in diff.splitlines():
        if line.startswith("diff "):
            in_header = True
        elif line.startswith("@@"):
            in_header = False
        elif in_header:
            if line.startswith("--- "):
                old_path = re.sub(r"^[ab]/", "", line[4:])
            elif line.startswith("+++ "):
                new_path = re.sub(r"^[ab]/", "", line[4:])
                path = old_path if new_path == "/dev/null" else new_path
                if new_path == "/dev/null":
                    deleted.append(path)
        elif line.startswith("+"):
            added.append((path, line[1:]))
        elif line.startswith("-"):
            removed.append((path, line[1:]))
    return added, removed, deleted


def is_test(path):
    return path.startswith("tests/")


def is_rule(text):
    return text.lstrip().startswith(("- ", "|"))


def table_rows(text):
    """Map each table row in CONSTRAINTS.md text to its section and its cells."""
    section, rows = "", {}
    for line in text.splitlines():
        if line.startswith("## "):
            section = line[3:].strip()
        elif line.startswith("|"):
            rows[line] = (section, [cell.strip() for cell in line.strip("|").split("|")])
    return rows


# The findings that an exception row can allow. A secret or a weaker rule
# can never be allowed.
EXCEPTABLE = {"silenced-checker", "unfinished-work", "test-made-easier", "test-deleted",
              "assertion-removed"}
MAX_EXCEPTION_DAYS = 90


def read_constraints(base):
    """Return CONSTRAINTS.md at the merge base and in the work tree."""
    merge_base = git("merge-base", base, "HEAD")
    if merge_base is None:
        bail(f"no merge base against {base}")
    old = git("show", f"{merge_base.strip()}:CONSTRAINTS.md", ok_codes=(0, 128)) or ""
    try:
        new = open("CONSTRAINTS.md", encoding="utf-8").read()
    except FileNotFoundError:
        new = ""
    return old, new


def allowed_removals(old, new):
    """Return the removed rule rows that make the bar stricter, not weaker.

    A Planned row that moved to Enforced with the same rule text is a
    promotion. A removed Exceptions row ends an exception.
    """
    enforced = {tuple(cells[:2]) for section, cells in table_rows(new).values()
                if section.startswith("Enforced")}
    allowed = set()
    for line, (section, cells) in table_rows(old).items():
        if section == "Planned" and tuple(cells[:2]) in enforced:
            allowed.add(line)
        elif section == "Exceptions" and cells and cells[0] not in ("ID", "") \
                and not set(cells[0]) <= {"-"}:
            allowed.add(line)
    return allowed


def exceptions(new, today):
    """Return the active exception rows as (rule, path pattern), and the
    findings for rows that break the rules for exceptions."""
    active, problems = [], []
    for line, (section, cells) in table_rows(new).items():
        if section != "Exceptions" or not cells or cells[0] == "ID" or set(cells[0]) <= {"-"}:
            continue
        if len(cells) != 6:
            problems.append(("bad-exception", "CONSTRAINTS.md", line))
            continue
        _, rule, path, _, owner, expires = cells
        rule, path = rule.strip("`"), path.strip("`")
        try:
            end = datetime.date.fromisoformat(expires)
        except ValueError:
            problems.append(("bad-exception", "CONSTRAINTS.md", f"{line} (expiry is not a date)"))
            continue
        if rule not in EXCEPTABLE:
            problems.append(("bad-exception", "CONSTRAINTS.md", f"{line} (rule cannot be excepted)"))
        elif not owner:
            problems.append(("bad-exception", "CONSTRAINTS.md", f"{line} (no owner)"))
        elif (end - today).days > MAX_EXCEPTION_DAYS:
            problems.append(("bad-exception", "CONSTRAINTS.md", f"{line} (longer than 90 days)"))
        elif end >= today:
            active.append((rule, path))
    return active, problems


def excepted(finding, active):
    rule, path, _ = finding
    return any(rule == r and fnmatch.fnmatch(path, p) for r, p in active)


def check(added, removed, deleted, promoted=frozenset()):
    findings = []
    for path, text in added:
        if SECRET.search(text):
            findings.append(("secret", path, "[value redacted]"))
        if path in PATTERN_EXEMPT:
            continue
        if SUPPRESSION.search(text):
            findings.append(("silenced-checker", path, text))
        if STUB.search(text):
            findings.append(("unfinished-work", path, text))
        if is_test(path) and SKIP.search(text):
            findings.append(("test-made-easier", path, text))
    for path in deleted:
        if is_test(path):
            findings.append(("test-deleted", path, "file deleted"))
    for path, text in removed:
        if is_test(path) and path not in deleted and ASSERTION.search(text):
            findings.append(("assertion-removed", path, text))
        # ponytail: any changed rule line is loud, tightening too; add a direction parser
        # if tightening commits become common.
        if path == "CONSTRAINTS.md" and is_rule(text) and text not in promoted:
            findings.append(("constraint-changed", path, text))
    return findings


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--base", default="origin/main")
    # For tests: the date that decides if an exception is still active.
    parser.add_argument("--today")
    args = parser.parse_args()

    top = git("rev-parse", "--show-toplevel")
    if top is None:
        bail("not inside a git work tree")
    os.chdir(top.strip())

    today = datetime.date.fromisoformat(args.today) if args.today else datetime.date.today()
    old, new = read_constraints(args.base)
    active, problems = exceptions(new, today)
    findings = check(*parse(collect_diff(args.base)), allowed_removals(old, new))
    findings = [f for f in findings if not excepted(f, active)] + problems
    if not findings:
        print("floor-guard: clean")
        return 0
    print(f"floor-guard: {len(findings)} violation(s):", file=sys.stderr)
    for rule, path, text in findings:
        print(f"  [{rule}] {path}: {text.strip()[:120]}", file=sys.stderr)
    print("Fix the code, or add a tracked exception to CONSTRAINTS.md.", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())

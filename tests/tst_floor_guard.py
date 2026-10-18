#!/usr/bin/env python3
"""Run floor_guard.py on small git repos with planted violations."""
import pathlib
import subprocess
import sys
import tempfile

GUARD = pathlib.Path(__file__).resolve().parent.parent / "tools" / "floor_guard.py"
FAKE_TOKEN = "ghp_" + "a" * 36

BASE_FILES = {
    "src/a.cpp": "int a() { return 1; }\n",
    "tests/tst_a.cpp": "QCOMPARE(a(), 1);\nQVERIFY(true);\n",
    "CONSTRAINTS.md": (
        "- Coverage: at least 80%\n\n"
        "## Enforced with numbers\n\n"
        "| Dimension | Rule | Checked by |\n|---|---|---|\n| Tests | All pass | ctest |\n\n"
        "## Planned\n\n"
        "| Dimension | Rule | Check added in |\n|---|---|---|\n| Speed | 16 ms | T7 |\n\n"
        "## Exceptions\n\n| ID | Rule | Path | Reason | Owner | Expires |\n|---|---|---|---|---|---|\n"
        "| E0 | test-deleted | tests/old.cpp | Old | someone | 2026-11-01 |\n"),
}
PLANNED_ROW = "| Speed | 16 ms | T7 |\n"
ENFORCED_ROW = "| Tests | All pass | ctest |\n"


def promote(rule):
    def change(root):
        replace("CONSTRAINTS.md", PLANNED_ROW, "")(root)
        replace("CONSTRAINTS.md", ENFORCED_ROW, ENFORCED_ROW + f"| Speed | {rule} | probe |\n")(root)
    return change


def run(*args, cwd):
    return subprocess.run(args, cwd=cwd, capture_output=True, text=True)


def make_repo(root):
    run("git", "init", "-q", "-b", "main", cwd=root)
    run("git", "config", "user.email", "test@example.com", cwd=root)
    run("git", "config", "user.name", "Test", cwd=root)
    for name, text in BASE_FILES.items():
        path = root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
    run("git", "add", ".", cwd=root)
    run("git", "commit", "-q", "-m", "base", cwd=root)


# A fixed date, so the expiry checks give the same result on every day.
TODAY = "2026-10-18"


def guard(root):
    return run(sys.executable, str(GUARD), "--base", "main", "--today", TODAY, cwd=root)


EXCEPTION_ROW = "| E0 | test-deleted | tests/old.cpp | Old | someone | 2026-11-01 |\n"


def with_exception(row):
    """Removes an assertion and adds an exception row."""
    def change(root):
        replace("tests/tst_a.cpp", "QVERIFY(true);\n", "")(root)
        append("CONSTRAINTS.md", row)(root)
    return change


def case(name, change, expected_code, expected_rule=None):
    with tempfile.TemporaryDirectory() as tmp:
        root = pathlib.Path(tmp)
        make_repo(root)
        change(root)
        result = guard(root)
        assert result.returncode == expected_code, (
            f"{name}: exit {result.returncode}, wanted {expected_code}\n{result.stderr}")
        if expected_rule:
            assert f"[{expected_rule}]" in result.stderr, f"{name}: no {expected_rule}"
        assert FAKE_TOKEN not in result.stdout + result.stderr, f"{name}: secret leaked"
    print(f"PASS {name}")


def append(name, text):
    return lambda root: (root / name).open("a").write(text)


def replace(name, old, new):
    def change(root):
        path = root / name
        path.write_text(path.read_text().replace(old, new))
    return change


case("clean change", append("src/a.cpp", "int b() { return 2; }\n"), 0)
case("new file is checked", lambda r: (r / "src/b.cpp").write_text("// NOLINT\n"), 1,
     "silenced-checker")
case("suppression", append("src/a.cpp", "int c(); // NOLINT\n"), 1, "silenced-checker")
case("stub", append("src/a.cpp", "// TODO write this\n"), 1, "unfinished-work")
case("empty catch", append("src/a.cpp", "try { a(); } catch (...) {}\n"), 1, "unfinished-work")
case("skipped test", append("tests/tst_a.cpp", 'QSKIP("later");\n'), 1, "test-made-easier")
case("assertion removed", replace("tests/tst_a.cpp", "QVERIFY(true);\n", ""), 1,
     "assertion-removed")
case("test deleted", lambda r: (r / "tests/tst_a.cpp").unlink(), 1, "test-deleted")
case("constraint changed", replace("CONSTRAINTS.md", "80%", "50%"), 1, "constraint-changed")
case("planned rule enforced", promote("16 ms"), 0)
case("planned rule dropped", replace("CONSTRAINTS.md", PLANNED_ROW, ""), 1, "constraint-changed")
case("planned rule weakened as it moves", promote("32 ms"), 1, "constraint-changed")
case("exception allows the finding",
     with_exception("| E1 | assertion-removed | tests/tst_a.cpp | Moved | someone | 2026-10-25 |\n"), 0)
case("exception with a glob path",
     with_exception("| E1 | `assertion-removed` | `tests/*.cpp` | Moved | someone | 2026-10-25 |\n"), 0)
case("exception for another path",
     with_exception("| E1 | assertion-removed | tests/other.cpp | Moved | someone | 2026-10-25 |\n"), 1,
     "assertion-removed")
case("expired exception",
     with_exception("| E1 | assertion-removed | tests/tst_a.cpp | Moved | someone | 2026-10-17 |\n"), 1,
     "assertion-removed")
case("exception longer than 90 days",
     with_exception("| E1 | assertion-removed | tests/tst_a.cpp | Moved | someone | 2027-03-01 |\n"), 1,
     "bad-exception")
case("exception without owner",
     with_exception("| E1 | assertion-removed | tests/tst_a.cpp | Moved |  | 2026-10-25 |\n"), 1,
     "bad-exception")
case("exception with a bad date",
     with_exception("| E1 | assertion-removed | tests/tst_a.cpp | Moved | someone | soon |\n"), 1,
     "bad-exception")
case("exception with too few cells",
     with_exception("| E1 | assertion-removed | tests/tst_a.cpp |\n"), 1, "bad-exception")
case("secret cannot be excepted",
     lambda r: (append("src/a.cpp", f'auto t = "{FAKE_TOKEN}";\n')(r),
                append("CONSTRAINTS.md", "| E1 | secret | src/a.cpp | No | someone | 2026-10-25 |\n")(r)),
     1, "secret")
case("ending an exception is allowed", replace("CONSTRAINTS.md", EXCEPTION_ROW, ""), 0)
case("secret redacted", append("src/a.cpp", f'auto t = "{FAKE_TOKEN}";\n'), 1, "secret")

with tempfile.TemporaryDirectory() as tmp:
    result = run(sys.executable, str(GUARD), cwd=tmp)
    assert result.returncode == 2, f"not a repo: exit {result.returncode}"
    print("PASS not a repo gives exit 2")

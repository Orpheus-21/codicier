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
    "CONSTRAINTS.md": "- Coverage: at least 80%\n",
}


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


def guard(root):
    return run(sys.executable, str(GUARD), "--base", "main", cwd=root)


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
case("secret redacted", append("src/a.cpp", f'auto t = "{FAKE_TOKEN}";\n'), 1, "secret")

with tempfile.TemporaryDirectory() as tmp:
    result = run(sys.executable, str(GUARD), cwd=tmp)
    assert result.returncode == 2, f"not a repo: exit {result.returncode}"
    print("PASS not a repo gives exit 2")

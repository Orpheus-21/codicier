#!/usr/bin/env python3
"""Check how coverage_diff.py reads changed lines from a diff."""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent / "tools"))
from coverage_diff import changed_lines

DIFF = """\
diff --git a/src/a.cpp b/src/a.cpp
--- a/src/a.cpp
+++ b/src/a.cpp
@@ -3,0 +4,2 @@ int a()
+int b() { return 2; }
+int c() { return 3; }
@@ -10 +12 @@ int d()
-    return 4;
+    return 5;
@@ -20,2 +22,0 @@ int e()
-    gone();
-    gone();
diff --git a/src/main.cpp b/src/main.cpp
--- a/src/main.cpp
+++ b/src/main.cpp
@@ -1,0 +2 @@
+// main.cpp is not measured
diff --git a/tests/tst_a.cpp b/tests/tst_a.cpp
--- a/tests/tst_a.cpp
+++ b/tests/tst_a.cpp
@@ -1,0 +2 @@
+QVERIFY(true);
diff --git a/src/old.h b/src/old.h
--- a/src/old.h
+++ /dev/null
@@ -1 +0,0 @@
-int old();
"""

result = changed_lines(DIFF)
# Lines 4 and 5 are added, line 12 is changed. A pure deletion adds no line.
assert result == {"src/a.cpp": {4, 5, 12}}, result
print("PASS changed_lines")

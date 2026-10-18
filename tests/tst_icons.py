#!/usr/bin/env python3
"""Check the SVG conversion in tools/icons.py."""
import pathlib
import sys
import xml.etree.ElementTree as ET

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent / "tools"))
import icons

# A path that starts with "m" must start with an absolute move after a join.
for d, want in [
    ("m6 6 12 12", "M6 6l12 12"),
    ("M18 6 6 18", "M18 6 6 18"),
    ("m15 18-6-6 6-6", "M15 18l-6-6 6-6"),
    ("m2 2h4", "M2 2h4"),
    ("m.5 1.5.5 1", "M.5 1.5l.5 1"),
]:
    got = icons.absolute_start(d)
    assert got == want, f"{d!r}: got {got!r}, wanted {want!r}"
print("PASS absolute start")


def element(text):
    return ET.fromstring(f'<svg xmlns="http://www.w3.org/2000/svg">{text}</svg>')[0]


assert icons.to_path(element('<circle cx="12" cy="12" r="3" fill="currentColor"/>')) == \
    "M9 12a3 3 0 1 0 6 0a3 3 0 1 0 -6 0"
assert icons.to_path(element('<rect x="2" y="4" width="10" height="6"/>')) == \
    "M2 4h10v6h-10z"
assert icons.to_path(element('<rect x="2" y="4" width="10" height="6" rx="2"/>')) == \
    "M4 4h6a2 2 0 0 1 2 2v2a2 2 0 0 1 -2 2h-6a2 2 0 0 1 -2 -2v-2a2 2 0 0 1 2 -2z"
print("PASS circle and rect")

try:
    icons.to_path(element('<ellipse cx="1" cy="1" rx="1" ry="2"/>'))
except ValueError:
    print("PASS unsupported element fails")
else:
    raise AssertionError("an unsupported element must raise ValueError")

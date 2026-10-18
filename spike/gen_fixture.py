#!/usr/bin/env python3
"""Write a Markdown chapter of a fixed number of words for the editor probe.

Usage: gen_fixture.py OUT [--words 100000]
The seed is fixed, so each run writes the same file.
"""
import argparse
import random

WORDS = (
    "the a of and to in was he she it that with his her for on as at by "
    "from had not but they were said all there one into could would when "
    "night river lantern harbour silence letter window winter garden stone "
    "remembered whispered carried listened waited followed opened turned "
    "quiet narrow golden broken distant heavy pale bright cold ancient"
).split()


def paragraph(rng, count, note):
    words = [rng.choice(WORDS) for _ in range(count)]
    words[0] = words[0].capitalize()
    i = rng.randrange(1, count - 1)
    words[i] = f"*{words[i]}*"
    j = rng.randrange(1, count - 1)
    if rng.random() < 0.3 and j != i:
        words[j] = f"**{words[j]}**"
    words[-1] += f".[^{note}]" if note else "."
    return " ".join(words)


def chapter_text(total):
    rng = random.Random(7)
    parts, words, chapter, notes = [], 0, 0, 0
    while words < total:
        if words >= chapter * 5000:
            chapter += 1
            parts.append(f"## Chapter {chapter}")
            words += 2
        count = min(rng.randrange(60, 150), total - words)
        if count < 3:
            break
        note = 0
        if rng.random() < 0.1:
            notes += 1
            note = notes
        parts.append(paragraph(rng, count, note))
        words += count
    return "\n\n".join(parts) + "\n"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("out")
    parser.add_argument("--words", type=int, default=100_000)
    args = parser.parse_args()
    with open(args.out, "w", encoding="utf-8") as f:
        f.write(chapter_text(args.words))


if __name__ == "__main__":
    main()

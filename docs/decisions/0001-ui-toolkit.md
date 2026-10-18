# 1. UI toolkit: Qt 6 Quick

Status: proposed. The decision is go or no-go at Checkpoint B.

Date: 2026-10-18.

## Context

Codicier is a writing app for long fiction and non-fiction. The toolkit must meet these needs:

- The app is native. It has no web view and no browser engine.
- The app works offline. It has no network code.
- The editor stays fast in a chapter of 100,000 words.
- The looks are the top priority. The toolkit must give full control of type, colour, and motion.
- Typing works through input methods and the compose key.
- The app runs on Linux first. Windows and macOS come later.

The editor does not need full rich text. Each chapter is a Markdown file. The editor shows the Markdown in its final style, and the marks stay in the text.

## Options

| Option | Result of the research |
|---|---|
| Qt 6 Quick (QML) with C++ | A mature text engine, input methods, accessibility, and Wayland support. Chosen for the spike. |
| Slint | No good editor for long text was found. Kept as the fallback. |
| GPUI | A code editor component, but no rich text. No confirmed Linux support was found. |
| Flutter | The editor packages are before version 1.0, and Linux support is weak. |
| GTK 4 | The look follows GNOME. Support on Windows and macOS is weaker. |
| iced and egui | No good editor for long text was found. |
| Tauri | It uses a web view, so it is not native. |

## The spike

The `spike/` folder tests the Qt editor before any real screen exists.

- `spike/gen_fixture.py` writes a Markdown chapter of 100,000 words with headings, strong text, emphasis, and footnote marks.
- `spike/EditorProbe.qml` shows the chapter in a QML `TextEdit` with wrap, in a column 66 characters wide.
- `spike/MarkdownHighlighter.cpp` styles headings, strong text, emphasis, and footnote marks with a `QSyntaxHighlighter`.
- `spike/probe.cpp` types 300 keys, then scrolls 300 steps of 40 pixels. It times each step from the event to the next frame.
- `spike/typecheck.cpp` opens an empty editor and writes the typed text to a file.

The probe renders on the GUI thread with no vsync. So each time is the work of one frame, not the wait for the screen. The first 20 steps of each phase are not counted.

## Results

The test machine ran Arch Linux, Hyprland 0.56.2 on Wayland, Qt 6.11.2, GCC 16.2.1, and fcitx5 5.1.22. The chapter had 560,567 characters.

| Measure (p95) | Editor setup | Budget | Wayland window | Offscreen |
|---|---|---|---|---|
| Key press to frame | Plain text, system font | 16 ms | 7.5 ms | 2.2 ms |
| Key press to frame | Styles, system font | 16 ms | 7.6 ms | 2.4 ms |
| Key press to frame | Styles, Crimson Pro, 66 characters | 16 ms | 7.0 ms | 1.3 ms |
| Scroll step to frame | Styles, Crimson Pro, 66 characters | 16 ms | 9.0 ms | 1.1 ms |

Each Wayland value is the highest p95 of 2 or 3 runs. Each offscreen value is from one run.

The budget of 16 ms comes from a 60 Hz screen, which shows a new frame every 16.7 ms.

Other results:

- **Input methods.** Qt sends the keys to fcitx5 through its `fcitx` plugin. A script typed a test line with 7 compose letters (é ñ ü ç ß à ô) 6 times, and each result was exact. A person typed a test line with the compose key by hand, and the result was exact.
- **Font.** The spike bundles Crimson Pro under the SIL Open Font License 1.1. `FontLoader` loads it from the app files.
- **Line width.** A prose paragraph had 65.7 characters per line, for a target of 66.
- **CI.** The speed check runs on each push to `main` and on each pull request, in an Arch Linux container with no GPU. It passes there.

## What the spike did not test

- Chinese, Japanese, and Korean input. Task T98 tests it.
- Line height. QML `TextEdit` has no line height setting. The real editor must set it through `QTextBlockFormat`.
- Screen readers.
- Windows and macOS.
- Packages for users, such as Flatpak or AppImage.

## Decision

Use Qt 6 Quick for the UI and C++20 for the logic. The spike met every budget with more than 40% of each budget left on a real Wayland window.

If the decision is no-go, the plan stops, and a new plan uses Slint.

## Consequences

- The binary always holds `libQt6Network`, because `libQt6Qml` links it. The app blocks every non-local URL with `OfflineGuard`, and a check bans network classes in the app code.
- Qt 6 is under the LGPL version 3 and the GPL version 3. Codicier uses the GPL version 3 or any later version, which can use both.
- The build needs CMake, a C++20 compiler, and the Qt 6 base and declarative modules.
- `check_editor_speed` runs the probe on each edit, so a slower editor fails the checks.

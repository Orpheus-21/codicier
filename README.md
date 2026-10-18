# Codicier

Codicier is a desktop app for authors who write books, stories, and essays.

## What it does

Codicier keeps each writing project in a folder of plain Markdown files. A binder on the left shows the parts, chapters, scenes, notes, and research of the project. The page on the right shows the text of one scene, or all the scenes of a chapter as one scroll. The app saves the text one second after each edit.

The app also does these things:

- It counts the words of the scene, the chapter, and the book. It shows the count against the word goal of the project.
- It keeps snapshots of a scene. It shows the differences between a snapshot and the current text, and it restores a snapshot.
- It writes a zip backup of a project when the project closes.
- It searches the full text of the project, also with regular expressions. It replaces words across the manuscript.
- It has a focus mode with typewriter scroll.

The app runs as a native window. It does not use a web view. It works offline: it sends no telemetry and needs no network connection. Typesetting, import, and export are not part of the app.

The project is in development. Not all features are complete.

## Requirements

- Linux. The app is built and tested on Arch Linux. Other systems are not tested.
- A C++20 compiler, such as GCC or Clang.
- CMake 3.21 or later.
- Qt 6.8 or later, with the Qt Base and Qt Declarative modules.
- toml++ 3 or later.

On Arch Linux, install the requirements with this command:

```
sudo pacman -S --needed gcc make cmake qt6-base qt6-declarative tomlplusplus
```

The fonts Literata and Inter are in the repo. The app has them inside the program file.

## Install

1. Get the source code:

   ```
   git clone https://github.com/Orpheus-21/codicier.git
   cd codicier
   ```

2. Configure the build:

   ```
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
   ```

3. Build the app:

   ```
   cmake --build build -j "$(nproc)"
   ```

4. Start the app:

   ```
   build/codicier
   ```

To run the tests, use this command after step 3:

```
ctest --test-dir build
```

## Usage

At the first start, choose a folder for your writing. This folder is a workspace. Each project in it is a folder. Click "New project" to make a project. Choose one of these kinds: short story, novella, novel, series, essay, essay collection, or non-fiction book. Each kind starts with its own structure of items.

### Keys in the project

| Key | Action |
|---|---|
| Ctrl+N | Make a new scene. |
| Ctrl+K | Split the scene at the cursor. |
| Ctrl+Shift+S | Take a snapshot of the open text. |
| Ctrl+I | Show or hide the inspector. |
| Ctrl+Shift+F | Search the project. |
| Ctrl+H | Replace across the manuscript. |
| F11 | Start or stop focus mode. |
| Escape | Stop focus mode, or close the search panel. |
| Ctrl+W | Close the project and go back to the workspace. |

### Keys in the binder

| Key | Action |
|---|---|
| Up, Down | Select the item above or below. |
| Left, Right | Close or open the item. |
| Enter | Open the item and move to the page. |
| Alt+Up, Alt+Down | Move the item up or down. |
| F2 | Rename the item. |
| Delete | Move the item to the trash. |

You can also drag an item to a new place in the binder. A right click on an item opens a menu with more actions, such as "Merge with next scene" and "Restore".

### The inspector

The inspector shows the data of the selected item: synopsis, status, point of view, place, time, word target, tags, and notes. The "Manuscript" choice leaves an item out of the word count of the book. The inspector also lists the snapshots of a scene. Click "Compare" to see the differences, or "Restore" to go back to a snapshot.

### Search and replace

Press Ctrl+Shift+F and type. The results show the title of each item and the text near the match. The search ignores case and accents. Click a result to open it with the match selected.

Click the `.*` button, or press Alt+R, to search with a regular expression. Click the bookmark button to save the search in the binder. A saved search shows its current results when you click it.

Press Ctrl+H to replace. Type the old words, press Tab, and type the new words. The panel shows each match with the change before the app changes a file. Click the replace button to make the change. Before the change, the app takes a snapshot named "Before replace" of each scene with a match.

## Configuration

Click the settings button on the workspace screen to change these settings:

| Setting | Default | What it does |
|---|---|---|
| Backup folder | `~/.local/share/codicier/backups` | The folder for the zip backups. Each project has its own folder in it. |
| Backups to keep for each project | 10 | The number of zip backups to keep. The app deletes older backups. The value is from 1 to 1000. |

The app writes the settings and the list of workspaces to `~/.config/codicier/settings.ini`.

The colours follow the light or dark setting of the system.

## How it works

The code has three parts:

- `src/model` reads and writes projects. It holds the binder, the file names, the file watcher, the snapshots, the differences, and the backups. It uses only Qt Core.
- `src/index` keeps the search index. It is an SQLite database with full text search in `.codicier/index.db` in the project folder.
- `src/app` and `qml` make the window. The C++ classes in `src/app` give the project to the QML views.

A project folder holds `project.toml`, `binder.toml`, and the Markdown files. `binder.toml` holds the structure of the binder and the data of each item. A Markdown file holds only the prose. The app writes each file through a temporary file, so a crash cannot leave half a file. The app can delete the `.codicier` folder and build it again at any time. `docs/format.md` describes the format in full.

When another program changes a file of an open project, the app loads the new text. If the text in the app has changes that are not saved, the app asks which text to keep.

## License

Codicier is licensed under the GNU General Public License, version 3 or any later version. See the `LICENSE` file.

# Codicier

Codicier is a desktop app for authors who write books, stories, and essays.

## What it does

Codicier keeps each writing project in a folder of plain Markdown files. A binder on the left shows the parts, chapters, scenes, notes, and research of the project. The page in the middle shows the text of one scene, or all the scenes of a chapter as one scroll. The inspector on the right can show the data of the selected item. The app saves the text one second after the last edit.

The app also does these things:

- It counts the words of the scene, the chapter, and the book. It shows the count against the word goal of the project.
- It keeps snapshots of a scene. It shows the differences between a snapshot and the current text, and it restores a snapshot.
- It writes a zip backup of a project when the project closes.
- It searches the full text of the project, also with regular expressions. It replaces words across the manuscript.
- It has a focus mode with typewriter scroll.
- It has a codex of the characters, places, items, factions, lore, and terms of the story, with links from the text and a list of mentions.
- It has a command palette that finds every command and every item by name.
- It keeps the layout of the panes for each project. A key map file sets the key of each command.

The app runs as a native window. It does not use a web view. It works offline: it sends no telemetry and needs no network connection. Typesetting, import, and export are not part of the app.

The project is in development. Not all features are complete.

## Requirements

- Linux. The app is built and tested on Arch Linux. Other systems are not tested.
- A C++20 compiler, such as GCC or Clang.
- CMake 3.21 or later.
- Qt 6.8 or later, with the Qt Base and Qt Declarative modules.
- toml++ 3 or later.
- Hunspell and pkg-config. For the spell check, also a Hunspell dictionary, such as `hunspell-en_us` or `hunspell-en_gb`.

On Arch Linux, install the requirements with this command:

```
sudo pacman -S --needed gcc make cmake pkgconf qt6-base qt6-declarative tomlplusplus hunspell hunspell-en_us
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

### The command palette

Press Ctrl+Shift+P and type a part of a name. The palette lists the commands and the items of the binder that match. Up and Down choose a line. Enter runs the command or opens the item.

### Keys in the project

These are the default keys. The key map file can change each key. See "Configuration".

| Key | Action |
|---|---|
| Ctrl+Shift+P | Find a command or an item. |
| F6 | Move to the next pane: the binder, the page, then the inspector. |
| Ctrl+N | Make a new scene. |
| Ctrl+Shift+N | Make a new chapter. |
| Ctrl+Alt+N | Make a new part. |
| Ctrl+Alt+M | Make a new note. |
| Ctrl+Alt+P | Make a new page. |
| F2 | Rename the item. |
| Ctrl+K | Split the scene at the cursor. |
| Ctrl+Shift+J | Merge the scene with the next scene. |
| Ctrl+Shift+Del | Move the item to the trash. |
| Ctrl+Shift+S | Take a snapshot of the open text. |
| Ctrl+Shift+E | Open the codex. |
| Ctrl+Shift+F | Search the project. |
| Ctrl+H | Replace across the manuscript. |
| Ctrl+Shift+B | Show or hide the binder. |
| Ctrl+I | Show or hide the inspector. |
| F11 | Start or stop focus mode. |
| Escape | Stop focus mode, or close the search panel. |
| Ctrl+W | Close the project and go back to the workspace. |

Tab types a tab character on the page. Use F6 to move the keyboard focus to the page and away from the page.

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

Drag the line between two panes to change the width of a pane. The buttons at the top right of the page show or hide the binder and the inspector. The app saves the open item, the open panes, and their widths for each project.

### The inspector

The inspector shows the data of the selected item: synopsis, status, point of view, place, time, word target, tags, and notes. The "Manuscript" choice leaves an item out of the word count of the book. The inspector also lists the snapshots of a scene. Click "Compare" to see the differences, or "Restore" to go back to a snapshot.

### The codex

The codex holds the entities of the story: characters, places, items, factions, lore, and terms. Each entity has a page for its description, like a scene. Press Ctrl+Shift+E to open the codex panel. The panel groups the entities by type and has a filter field. Use its + button to add an entity.

The inspector of an entity shows these parts:

- Aliases: other names of the entity, split by commas.
- A field for each field of its type.
- Mentions: each text of the manuscript that names the entity by its title or an alias, with the count of mentions. Click a line to open the text.

To add fields to a type, or to make a new type such as "Ships", choose "Types and fields" in the + menu of the codex panel. The app saves the types and their fields in `project.toml`.

To link to an entity, type `[[` on the page. A list of entities opens under the cursor and filters as you type. Press Enter or Tab to insert the link, such as `[[Mara Vell]]`. Press Escape to close the list. Ctrl+click a link to open the entity. A link also finds an entity by an alias.

### Spelling and style

The page underlines each word that the dictionary does not know. The app uses the Hunspell dictionary for the `language` of the project, and works offline. Right-click an underlined word to see suggestions, or to add the word to the project dictionary, `dictionary.txt` in the project folder. The names and aliases of the codex count as known words.

The settings can also mark repeated words, filter words such as "just" and "seemed", adverbs that end in -ly, and passive voice. Each mark is a soft tint. These marks are off by default.

### Search and replace

Press Ctrl+Shift+F and type. The results show the title of each item and the text near the match. The search ignores case and accents. Click a result to open it with the match selected.

Click the `.*` button, or press Alt+R, to search with a regular expression. Click the bookmark button to save the search in the binder. A saved search shows its current results when you click it.

Press Ctrl+H to replace. Type the old words, press Tab, and type the new words. The panel shows each match with the change before the app changes a file. Click the replace button to make the change. Before the change, the app takes a snapshot named "Before replace" of each scene with a match.

## Configuration

Click the settings button on the workspace screen to change these settings. Each change applies at once.

| Setting | Default | What it does |
|---|---|---|
| Theme | System | System, Light, Dark, or Sepia. System follows the light or dark setting of the system. |
| Font of the text | Literata | The font of the page. The list shows the fonts of the computer. |
| Font of the menus | Inter | The font of the binder, the inspector, and the menus. |
| Size of the text | 19 px | The size of the text on the page, from 14 px to 28 px. |
| Width of the page | 66 characters | The width of the text column, from 45 to 90 characters. |
| Animations | On | Off stops all motion in the app. |
| Save the text after a pause of | 1 s | The wait after the last edit before the app saves: 0.5 s, 1 s, 2 s, or 5 s. |
| Backup folder | `~/.local/share/codicier/backups` | The folder for the zip backups. Each project has its own folder in it. |
| Backups to keep for each project | 10 | The number of zip backups to keep. The app deletes older backups. The value is from 1 to 1000. |

The app writes the settings and the list of workspaces to `~/.config/codicier/settings.ini`.

### The key map

The file `~/.config/codicier/keys.ini` holds the key of each command. The app writes the file with the default keys when a project opens. To change a key, edit the line of the command:

```
[keys]
scene.new=Ctrl+N
focus=F11
```

An empty value turns the key of the command off. The app reads the file when a project opens. If two commands have the same key, only one of the two commands uses the key, and the status bar shows a warning. A key that is not valid also shows a warning.

### The layout of a project

The app writes the layout of each project to `.codicier/state.toml` in the project folder. Delete the file to go back to the default layout.

## How it works

The code has three parts:

- `src/model` reads and writes projects. It holds the binder, the file names, the file watcher, the snapshots, the differences, and the backups. It uses only Qt Core.
- `src/index` keeps the search index. It is an SQLite database with full text search in `.codicier/index.db` in the project folder.
- `src/app` and `qml` make the window. The C++ classes in `src/app` give the project, the settings, and the key map to the QML views.

A project folder holds `project.toml`, `binder.toml`, and the Markdown files. `binder.toml` holds the structure of the binder and the data of each item. A Markdown file holds only the prose. The app writes each file through a temporary file, so a crash cannot leave half a file. The app can delete the `.codicier` folder and build it again at any time. `docs/format.md` describes the format in full.

When another program changes a file of an open project, the app loads the new text. If the text in the app has changes that are not saved, the app asks which text to keep.

## License

Codicier is licensed under the GNU General Public License, version 3 or any later version. See the `LICENSE` file.

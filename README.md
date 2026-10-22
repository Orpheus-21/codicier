# Codicier

Codicier is a desktop app for authors who write books, stories, and essays.

## What it does

Codicier keeps each writing project in a folder of plain Markdown files. A binder on the left shows the parts, chapters, scenes, notes, and research of the project. The page in the middle shows the text of one scene, or all the scenes of a chapter as one scroll. The inspector on the right can show the data of the selected item. The app saves the text one second after the last edit.

The app also does these things:

- It counts the words of the scene, the chapter, and the book. It shows the count against the word goal of the project.
- It keeps snapshots of a scene. It shows the differences between a snapshot and the current text, and it restores a snapshot.
- It writes a zip backup of a project when the project closes. The home screen shows the date of the last backup.
- It searches the full text of the project, also with regular expressions. It replaces words across the manuscript.
- It has a focus mode with typewriter scroll.
- It shows the work as index cards, an outline table, a beat board, a plot grid, and a timeline in story time.
- It tracks a word target for each session, a goal and a deadline for the project, a history of the words of each day, and writing sprints.
- It has a codex of the characters, places, items, factions, lore, and terms of the story, with links from the text and a list of mentions.
- It has a command palette that finds every command and every item by name.
- It keeps the layout of the panes for each project. A key map file sets the key of each command.

The app runs as a native window. It does not use a web view. It works offline: it sends no telemetry and needs no network connection. Typesetting, import, and export are not part of the app.

The project is in development. Not all features are complete.

## Requirements

- Linux. The app is built and tested on Arch Linux. Other systems are not tested.
- A C++20 compiler, such as GCC or Clang.
- CMake 3.21 or later.
- Qt 6.12 or later, with the Qt Base and Qt Declarative modules.
- toml++ 3 or later.
- git, for the versions of a project.
- Hunspell and pkg-config. For the spell check, also a Hunspell dictionary, such as `hunspell-en_us` or `hunspell-en_gb`.
- Optional: a MyThes thesaurus, for synonyms, such as the `mythes-en` package. The app reads the `th_*.dat` files in `/usr/share/mythes`.

On Arch Linux, install the requirements with this command:

```
sudo pacman -S --needed gcc make cmake git pkgconf qt6-base qt6-declarative tomlplusplus hunspell hunspell-en_us
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

At the first start, choose a folder for your writing. This folder is a workspace. Each project in it is a folder. Click "New project" to make a project. Choose one of these kinds: blank, short story, novella, novel, series, essay, essay collection, or non-fiction book. Each kind starts with its own structure of items, and the dialog shows that structure before you create the project. A blank project has one scene and no word goal. "Create" opens the new project on its first scene.

In an empty workspace, click "Open a sample" to copy a short sample novel into the workspace. The sample has chapters, scenes, codex entries, plotlines, and dates in the story. Delete it like any other project.

The home screen shows each project with its author, its kind, its words, and the date of the last edit.

The first time you open a project, three hints point at the binder, the page, and the command palette. Press Escape or click a hint to see the next one.

In a project, the arrow at the top left goes back to the workspace. Hover over a button at the top right to see its name and its key.

### The command palette

Press Ctrl+Shift+P and type a part of a name. The palette lists the commands and the items of the binder that match. Up and Down choose a line. Enter runs the command or opens the item.

### Keys in the project

These are the default keys. The key map file can change each key. See "Configuration".

| Key | Action |
|---|---|
| Ctrl+Shift+P | Find a command or an item. |
| Ctrl+B | Make the selection, or the word at the cursor, bold. Press again to undo the bold. |
| Ctrl+I | Make the selection, or the word at the cursor, italic. Press again to undo the italic. |
| Ctrl+F | Find in the open scene. Enter goes to the next match, and Shift+Enter to the match before. |
| Ctrl+R | Find and replace in the open scene. |
| Ctrl+Shift+R | Track and review the changes of the open text. |
| Ctrl+Shift+H | Highlight the selection, or the word at the cursor, with `==` marks. Press again to take the highlight away. |
| Ctrl+= | Make the text of the page larger. |
| Ctrl+- | Make the text of the page smaller. |
| Ctrl+0 | Put the text of the page back to 19 px. |
| Alt+Left | Go back to the text that was open before, with the cursor where you left it. |
| Alt+Right | Go forward again. |
| Ctrl+\\ | Show the text before this one on a second page, or close the second page. |
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
| Ctrl+Shift+K | Show or hide the corkboard. |
| Ctrl+Shift+O | Show or hide the outliner. |
| Ctrl+Alt+B | Show or hide the beat board. |
| Ctrl+Alt+G | Show or hide the plot grid. |
| Ctrl+Alt+T | Show or hide the timeline. |
| Ctrl+Alt+C | Edit the calendar of the story. |
| Ctrl+Alt+R | Show or hide the relationship graph. |
| Ctrl+Alt+F | Show or hide the family tree. |
| Ctrl+Alt+I | Show or hide the insights. |
| Ctrl+Alt+D | Draft passes. |
| Ctrl+Alt+L | Show or hide the revision checklist. |
| Ctrl+Alt+K | Comment on the selected words. |
| Ctrl+Alt+O | Show the open comments. |
| Ctrl+Shift+G | Open goals and progress. |
| Ctrl+Shift+T | Start or stop a sprint. |
| Ctrl+Shift+F | Search the project. |
| Ctrl+H | Replace across the manuscript. |
| Ctrl+Shift+B | Show or hide the binder. |
| Ctrl+Shift+I | Show or hide the inspector. If your key map file still has the old default, Ctrl+I, the app moves it to Ctrl+Shift+I. |
| F11 | Start or stop focus mode. |
| Escape | Stop focus mode, or close the search panel. |
| Ctrl+W | Close the project and go back to the workspace. |
| F1 | Show every command and its key. |

Tab types a tab character on the page. Use F6 to move the keyboard focus to the page and away from the page.

The page hides the Markdown marks, such as the `#` of a heading and the stars of bold and italic, outside the paragraph with the cursor. The file keeps every mark, and a copy of the text includes them. The setting "Markdown marks" shows them everywhere.

As you type, the page changes straight quotes to curly quotes, `--` to an em dash, and `...` to an ellipsis. Press Ctrl+Z once to get back the characters that you typed. The settings turn each change off.

The status bar shows the words of the scene, the chapter, the book, and the session. With text selected, it also shows the words of the selection. At the right end, it shows "Saved", "Saving", or "Not saved". "Not saved" stays until a save works.

The inspector shows the reading time of the open item, and the insights show the reading time of each chapter and of the manuscript. The time uses 238 words a minute, the average speed of silent reading for adults.

In the binder, Ctrl+click adds an item to the selection, and Shift+click selects the items from the open item to the clicked one. The binder menu then sets the status, adds a tag, or moves to the trash on all selected items, and a drag moves all of them. The binder menu also gives an item a colour label, which the binder, the outliner, and the cards show, and duplicates an item with everything below it.

Ctrl+Alt+A makes names for characters: real names, or names invented in the style of a culture, for 14 cultures. Click a name to add it to the codex as a character. The name lists come from Wikidata, which is in the public domain (CC0).

On the page, the names and aliases of the codex have a quiet underline. A pause over a name shows the start of its entry, and Ctrl+click opens it. The corkboard can colour each card by its point of view character, and the inspector of a scene has optional fields for its goal, conflict, and outcome.

The corkboard shows the cards in a grid or free. On the free board, drag a card anywhere; a card dropped on another one forms a stack. The board keeps the place of each card.

"New idea canvas" in the add menu of the binder makes a canvas with the notes. Double-click the canvas for a note, add cards of the texts of the manuscript, and drag from the dot on the right of a node to another node to draw a line. Click a node and press Delete to remove it. The canvas saves to a file in `canvases/`.

Ctrl+Shift+R opens the changes panel. "Track changes" keeps the open text as it is now. From then on, the page underlines each insert and marks each deletion, and the panel lists every change with the removed words struck through. "Accept" keeps a change, and "Reject" puts the old words back. "Accept all and stop" ends the tracking.

The insights also list the phrases of three to six words that come back in other scenes, the names of the codex by their mentions, and the words per sentence of each chapter. For an English project they give the Flesch reading ease too.

Goals and progress (Ctrl+Shift+G) set a word goal for each day. The status bar then shows the words of today against it, and the days in a row. The dialog also shows the minutes of writing today, a history grid with month and weekday labels, and a chart of the words of the last 30 sessions. A session ends when a project closes. When a sprint ends, the window asks for attention.

The project menu turns on numbers for the chapters and the scenes in the binder and the outliner. The titles do not change.

Right-click a misspelled word on the page to see spelling suggestions. Right-click a correct word to see its synonyms from the thesaurus. Click a word in the list to put it in place of the word on the page.

The project menu, under the title of the project, chooses the spelling language from the Hunspell dictionaries on the computer. The settings list the dictionaries.

Right-click an item in the binder and choose "Open beside" to show it on a second page next to the main page. Each page saves its own text.

In focus mode, the paragraph with the cursor stays bright and the other paragraphs are dim. The settings choose the sentence in place of the paragraph, or no dimming.

Undo works across a restart: each save keeps the last 20 saved states of a text in `.codicier/undo/` in the project folder, and Ctrl+Z steps back through them when the text opens again. A change from another program starts a new history.

When you open a chapter or a part, its scenes show as one page. The project menu, under the title of the project, chooses the mark between two scenes: `* * *`, `#`, or a blank line.

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

### Views of the work

The buttons at the top right of the page, and the keys in the table above, change what the middle of the window shows.

- Corkboard: the items in the open chapter, part, or folder as index cards, with their synopsis. Drag a card onto another card to change the order in the binder. Click a card to open it.
- Outliner: the texts under the open item as a table, with status, POV, place, time, words, and custom fields. Click a header to sort the table.
- Beat board: choose a plot framework (three acts, Save the Cat, the Hero's Journey, Kishōtenketsu, or Snowflake), then drag scenes onto its beats.
- Plot grid: the scenes against the plotlines. Click a cell to put a scene on a plotline, or drag a mark to another plotline. Add plotlines in the inspector of a scene.
- Timeline: the scenes and entities with a date in the story, on a track for each POV character or each plotline.
- Relationship graph: the characters, and the other entities with a relation, with an arrow for each relation.
- Family tree: the people with a parent link, one row for each generation.
- Insights: charts of the manuscript. They show the words of each chapter and scene, the share of each POV and of dialogue in each chapter, the 30 most used words without common English words, and how often each character of the codex is named in each chapter.

Give a scene or an entity a date in its inspector, as year-month-day, such as `1847-03-04`. To use your own months, press Ctrl+Alt+C and write one month on each line with its number of days, such as `Frostmonth 40`.

### Goals and progress

Press Ctrl+Shift+G to set a word target for one session, and a word goal and a deadline for the project. The status bar shows the words of the session and the words to write each day to reach the goal. The dialog also shows a heat map of the words of each day, from `history.toml` in the project folder, and your streak of days in a row. Press Ctrl+Shift+T to start a writing sprint. The length is 25 minutes by default, and you can change it in the same dialog.

### The codex

The codex holds the entities of the story: characters, places, items, factions, lore, and terms. Each entity has a page for its description, like a scene. Press Ctrl+Shift+E to open the codex panel. The panel groups the entities by type and has a filter field. Use its + button to add an entity.

The inspector of an entity shows these parts:

- Aliases: other names of the entity, split by commas.
- A field for each field of its type.
- Mentions: each text of the manuscript that names the entity by its title or an alias, with the count of mentions. Click a line to open the text.

To add fields to a type, or to make a new type such as "Ships", choose "Types and fields" in the + menu of the codex panel. The app saves the types and their fields in `project.toml`.

To link two entities, open one, type the name of the relation in its inspector, such as "sister of", and choose the other entity in the "Link to" menu. Choose "Parent of" for a parent link of the family tree.

To add a map, choose "New map…" in the add menu, and choose an image. The app copies the image into the project. Right-click the map to pin a place of the codex. Click a pin to open its place, drag it to move it, or right-click it to remove it.

To link to an entity, type `[[` on the page. A list of entities opens under the cursor and filters as you type. Press Enter or Tab to insert the link, such as `[[Mara Vell]]`. Press Escape to close the list. Ctrl+click a link to open the entity. A link also finds an entity by an alias.

### Versions

A project can keep versions of all its files. Choose "Turn on versions" in the project menu, or press Ctrl+Alt+V. The app makes a git repository in the project folder, or uses the repository that already holds the folder, and saves a first version. You do not need to know git: the app runs it for you, and never uses the network.

- Press Ctrl+Alt+S to save a version with a short message. By default, the app also saves a version when a project with changes closes.
- Press Ctrl+Alt+H to see the versions of the project. The inspector of a text lists the versions of that text.
- Click "Compare" next to a version in the inspector to see the changes since that version, word by word.
- Restore a text from the compare view, or the whole project from the list of versions. Before a restore, the app saves a version of the state before, so you can undo a restore.

### Spelling and style

The page underlines each word that the dictionary does not know. The app uses the Hunspell dictionary for the `language` of the project, and works offline. Right-click an underlined word to see suggestions, or to add the word to the project dictionary, `dictionary.txt` in the project folder. The names and aliases of the codex count as known words.

The settings can also mark repeated words, filter words such as "just" and "seemed", adverbs that end in -ly, and passive voice. Each mark is a soft tint. These marks are off by default.

### Revision

A draft pass is one round of revision. Press Ctrl+Alt+D, name the pass, and start it. The app keeps a snapshot of each text at the start of each pass. With "Tint the passes" on, the page gives the words of each pass a soft background in the color of that pass.

Press Ctrl+Alt+L for the revision checklist. Choose a pass, add checks such as "Cut filter words", and tick each check for each chapter.

Select words on the page and press Ctrl+Alt+K to add a comment. A dotted line marks the commented words. Press Ctrl+Alt+O to see every open comment. Click a comment to open its text with the words selected, and click "Resolve" when it is done. The comments are in `binder.toml`, so the Markdown files stay prose only.

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
| Markdown marks | Hide outside the cursor | "Show" shows the marks in every paragraph. |
| Line spacing | Normal | Close, Normal, or Wide: 1.3, 1.5, or 1.8 lines. |
| Space between paragraphs | One line | Half a line, one line, or none with a first-line indent, as in a printed book. The file does not change. |
| Focus mode dims | Other paragraphs | Nothing, other paragraphs, or other sentences. |
| Daily reminder to write | Off | A time such as 09:30. Once a day at that time, while the app runs, a desktop notification reminds you to write. |
| Change as you type | All on | Curly quotes, `--` to an em dash, and `...` to an ellipsis. Click a choice to turn it on or off. |
| Labels on the top buttons | Icons only | Show puts the name of each button next to its icon. |
| First-run hints | On | The hints show once. "Show them again" shows them on the next project that opens. |
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

When another program changes a file of an open project, the app loads the new text. If the text in the app has changes that are not saved, a bar above the text asks which text to keep: "Keep mine" or "Load theirs". "Load theirs" first keeps a copy of the text of the app in `.codicier/recovered/`.

The app writes unsaved text to `.codicier/journal/` 200 ms after each edit, and a save removes it. If the app stops before a save, the next open keeps that text as a snapshot named "Recovered after a crash", and a bar offers to compare it.

One window at a time writes to a project. The first window takes the lock file `.codicier/lock`. A second window on the same project opens it read-only, and the status bar says why. A lock of a program that ended counts as free.

## License

Codicier is licensed under the GNU General Public License, version 3 or any later version. See the `LICENSE` file.

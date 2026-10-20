# Project folder format

Status: accepted on 2026-10-18. Format version 1.

A Codicier project is a normal folder of text files. You can read, edit, copy, back up, and put the folder in git without the app. The app adds nothing that you cannot read in a text editor.

## Rules for all files

- Text files use UTF-8 with no byte order mark.
- Lines end with a line feed (LF), and each file with text ends with one line feed. A new text item starts as an empty file.
- The two TOML files follow TOML 1.0.
- The app writes each file to a temporary file in the same folder, then renames the temporary file over the old file. A crash during a save cannot leave a half-written file.
- The app writes a file only when its content changes.
- The app never writes over a file that it could not read. It shows the error and the line number.

## The folder

```
the-salt-ledger/
    project.toml        the project: title, author, kind, goals
    binder.toml         the binder: the tree, the order, and the scene data
    manuscript/         one Markdown file for each scene
    matter/             one Markdown file for each page of front or back matter
    codex/              one Markdown file for each entity of the codex
    maps/               the images of the maps
    notes/              one Markdown file for each note
    research/           any files: images, PDF files, and other documents
    snapshots/          copies of a text at a moment, one folder for each item
    history.toml        the words that the author wrote on each day
    dictionary.txt      the words of the project for the spell check, one on each line
    .codicier/          app data that the app can build again or lose with no harm
```

The app makes the folders when it first needs them. A folder that has no files can be missing.

## project.toml

```toml
format = 1
title = "The Salt Ledger"
subtitle = ""
author = "A. Writer"
kind = "novel"
language = "en-GB"
created = 2026-10-18T07:40:00Z

[goals]
words = 90000
daily = 1000
deadline = 2027-03-01
```

| Key | Type | Required | Meaning |
|---|---|---|---|
| `format` | integer | yes | The format version. This document defines version 1. |
| `title` | string | yes | The title of the work. |
| `subtitle` | string | no | The subtitle. |
| `author` | string | no | The name on the work. |
| `kind` | string | yes | One of `short-story`, `novella`, `novel`, `series`, `essay`, `essay-collection`, or `non-fiction`. |
| `language` | string | no | A language tag, such as `en-GB`. The spell check uses it. |
| `created` | date and time | yes | The time the app made the project, in UTC. |
| `goals.words` | integer | no | The word goal for the whole work. |
| `goals.daily` | integer | no | The word goal for each day. |
| `goals.deadline` | date | no | The date to finish the work. |
| `framework` | string | no | The plot framework of the beat board: `three-act`, `save-the-cat`, `heros-journey`, `kishotenketsu`, or `snowflake`. |
| `calendar.month` | array of tables | no | The months of the calendar of the story, each with a `name` and `days` from 1 to 1000. With no months, the story uses the Gregorian calendar. |
| `pass` | array of tables | no | The draft passes, in order, each with a `name` and an optional `checks` list for the revision checklist. When a pass starts, the app takes a snapshot titled "Pass N start" of each text. |
| `plotline` | array of tables | no | The plotlines, each with an `id` and a `name`, in order. |
| `entity-type` | array of tables | no | The entity types of the author. See "Entity types". |

### Entity types

Each `[[entity-type]]` table adds fields to a type of the app, or defines a new type of entity for the codex:

```toml
[[entity-type]]
kind = "ships"
name = "Ships"
fields = ["Captain", "Home port"]
```

| Key | Type | Required | Meaning |
|---|---|---|---|
| `kind` | string | yes | The kind of the items of this type. A kind of the app, such as `character`, or a new kind with lowercase letters, digits, and hyphens, with a letter first. |
| `name` | string | no | The heading of the type in the codex, for a new kind. |
| `fields` | array of strings | no | The names of the fields of the type. |

An item of a new kind is an entity: it is in the `codex` section, and its file is in `codex/`.

## binder.toml

The binder is one flat list of items. Each item names its parent. The order of the items with the same parent is the order in the binder.

```toml
format = 1

[[item]]
id = "k3m9q2x7ab"
kind = "part"
section = "manuscript"
title = "Part Two"

[[item]]
id = "p8d2w5n1cq"
kind = "chapter"
parent = "k3m9q2x7ab"
title = "The March Accounts"

[[item]]
id = "t6v4h9r2mz"
kind = "scene"
parent = "p8d2w5n1cq"
title = "A Date She Did Not Know"
file = "manuscript/a-date-she-did-not-know.md"
status = "revised"
pov = "Mara"
place = "The river office"
time = "4 March, evening"
synopsis = "Mara finds a date in her mother's letter."
tags = ["letter", "mother", "accounts"]
target = 1500
notes = """
The figure in the column is a count of days, not money.
Do not say so yet."""
```

### Keys of an item

| Key | Type | For | Meaning |
|---|---|---|---|
| `id` | string | all | The item id. See "Item ids". |
| `kind` | string | all | The kind of item. See "Kinds of items". |
| `section` | string | top level only | `manuscript`, `front`, `back`, `codex`, `notes`, `research`, `searches`, or `trash`. |
| `parent` | string | all below the top level | The id of the parent item. |
| `title` | string | all | The title in the binder. |
| `file` | string | text and file items | The path of the file, relative to the project folder, with `/` between the parts. |
| `status` | string | scenes | `draft`, `revised`, or `final`. The default is `draft`. |
| `pov` | string | scenes | The point of view character. |
| `place` | string | scenes | Where the scene happens. |
| `time` | string | scenes | When the scene happens, in free text. |
| `date` | string | scenes and entities | The date in story time, written year-month-day, such as `1847-03-04`, in the calendar of the project. A year before year 0 has a minus sign. |
| `plotlines` | array of strings | scenes | The ids of the plotlines of the scene. |
| `beat` | string | scenes | The id of the beat of the framework that the scene is on. |
| `synopsis` | string | text items | A short summary. |
| `aliases` | array of strings | entities | Other names of the entity, such as a nickname or a title. |
| `fields` | table of strings | entities | The values of the fields of the entity type, by field name, such as `Captain = "Ahab"`. The app writes the table under the item as `[item.fields]`. |
| `tags` | array of strings | text items | Tags. |
| `target` | integer | text items | The word target. |
| `include` | boolean | text items | False leaves the item out of the manuscript: no total counts its words. The default is true, and the app writes the key only when it is false. |
| `query` | string | saved searches | The text of the search. |
| `regex` | boolean | saved searches | True makes the query a regular expression. The default is false, and the app writes the key only when it is true. |
| `notes` | string | all | Free notes for the item. |
| `restore` | string | items in the trash | The id of the parent before the move to the trash. |

An item at the top level has a `section` and no `parent`. An item below the top level has a `parent` and no `section`. It takes its section from the item at the top of its branch.

### Kinds of items

| Kind | Holds | Has a file | Section |
|---|---|---|---|
| `part` | chapters and scenes | no | `manuscript` |
| `chapter` | scenes | no | `manuscript` |
| `folder` | any items | no | any |
| `scene` | prose | yes, in `manuscript/` | `manuscript` |
| `page` | front or back matter, such as a dedication | yes, in `matter/` | `front` or `back` |
| `note` | a note | yes, in `notes/` | `notes` |
| `file` | a research file | yes, in `research/` | `research` |
| `search` | a saved search | no | `searches` |
| `character` | a person or a being of the story | yes, in `codex/` | `codex` |
| `place` | a place of the story | yes, in `codex/` | `codex` |
| `item` | an object of the story | yes, in `codex/` | `codex` |
| `faction` | a group, a family, or an institution | yes, in `codex/` | `codex` |
| `lore` | history, a custom, or a rule of the world | yes, in `codex/` | `codex` |
| `term` | a word of the story, for the glossary | yes, in `codex/` | `codex` |
| `map` | the image of a map: PNG, JPEG, WebP, GIF, or BMP | yes, in `maps/` | `codex` |

The kinds `character`, `place`, `item`, `faction`, `lore`, and `term` are the entities of the codex. The file of an entity holds its description as prose. Its data, such as `aliases`, `tags`, and `notes`, is in `binder.toml`, as for a scene.

A short story or an essay can have scenes at the top of the manuscript, with no parts or chapters.

### The trash

A move to the trash changes only `binder.toml`. The item goes to the top level with `section = "trash"`. Its `restore` key keeps the id of its old parent. The file stays where it is. When the author empties the trash, the app deletes the files of the items in the trash.

### Relations, comments, ticks, and pins

`binder.toml` can also hold these lists of tables, after the items:

| Table | Keys | Meaning |
|---|---|---|
| `[[relation]]` | `from`, `to`, `name`, and `parent` | A link between two entities, such as `name = "sister of"`. With `parent = true`, `from` is a parent of `to`, for the family tree. The app writes `parent` only when it is true. |
| `[[comment]]` | `item`, `quote`, `start`, `text`, and `resolved` | A comment on the words `quote` of a text, which were at the character `start` when the author wrote the comment. The app finds the quote at the place nearest `start`. The app writes `resolved` only when it is true. |
| `[[tick]]` | `pass`, `check`, and `item` | A tick of the revision checklist: the check of the pass with that number is done for that chapter. |
| `[[pin]]` | `map`, `place`, `x`, and `y` | An entity pinned to a map. `x` and `y` are parts of the width and the height of the image, from 0 to 1. |

### Keys that the app does not know

The app keeps every key that it does not know when it saves a file. A newer version of the app, or an author, can add keys with no data loss.

The app does not keep TOML comments when it saves `project.toml` or `binder.toml`. Put notes in the `notes` key, not in comments.

### The layout of a saved file

The app writes `project.toml` and `binder.toml` in one fixed layout:

1. The keys in the order of the tables in this document.
2. The other keys, in alphabetical order.
3. The tables and the lists of tables, each with its own header, such as `[goals]` or `[[item]]`.
4. One empty line before each header.

The items of the binder come in tree order: each item, then its children. A string with a line break is a multi-line string. A file in this layout does not change when the app loads it and saves it again. A file that an author edits by hand takes this layout at the next save.

## history.toml

The app writes the net words that the author added to the manuscript on each day. A day of cuts can have a negative value. The table has one line for each day, in date order:

```toml
[days]
2026-10-19 = 812
2026-10-20 = 435
```

The file is not in `.codicier/`, because the app cannot build it again. A file that the app cannot read counts as an empty history.

## Item ids

An item id has 10 characters from the lowercase Crockford base 32 alphabet: `0123456789abcdefghjkmnpqrstvwxyz`. The app makes each id at random and checks that the id is new in the project. An id never changes, so a link to an item stays correct after a move or a change of title.

## Markdown files

A text file holds only the text of its item. It has no front matter and no title line, because the title and all other data are in `binder.toml`. Typst and other tools can read the files with no change.

The text is CommonMark with these additions:

- Footnote marks, such as `[^1]`, and footnote text, such as `[^1]: The text.`
- Headings inside a scene are allowed, but the binder holds the titles of parts, chapters, and scenes.

## File names

The app makes the file name from the title when it makes the item:

1. Change the title to lowercase.
2. Change each run of characters that are not letters or digits to one hyphen.
3. Remove hyphens at the start and at the end.
4. Cut the name to 60 characters.
5. If the name is empty, use `untitled`.
6. If the name is already in use in the folder, add `-2`, `-3`, and so on.
7. Add `.md`.

Letters and digits from any script stay in the name.

The file name does not change when the title changes or when the item moves in the binder. A change of title therefore never moves a file.

## Snapshots

A snapshot is a copy of the file of a text item at one moment. It is in `snapshots/<item id>/`, and its name is the date and time, then an optional title: `snapshots/t6v4h9r2mz/2026-10-18T19-30-00 Before the rewrite.md`. A second snapshot in the same second adds `-2`, `-3`, and so on after the time. The file holds the text as it was, in the same format as the text file.

Snapshots are not in `.codicier/`, because the app cannot build them again. Git can track them, as it tracks the rest of the project.

## The .codicier folder

| Path | Meaning |
|---|---|
| `.codicier/index.db` | The search index. The app builds it again from the files when it is missing. |
| `.codicier/state.toml` | The state of the window: the open item and the panes. See "The state file". |
| `.codicier/recovered/` | Copies of unsaved text. When another program changes a file while the app has unsaved text, and the author chooses the version on disk, the app first writes its own version here. |
| `.codicier/.gitignore` | One line, `*`, so git ignores the folder. |

### The state file

`state.toml` has these keys. Each key is optional. The app uses the default for a missing key, and ignores a file that it cannot read.

| Key | Type | Meaning |
|---|---|---|
| `item` | string | The id of the open item. |
| `binder` | boolean | True if the binder pane is open. The default is true. |
| `binder-width` | integer | The width of the binder pane, in pixels. |
| `inspector` | boolean | True if the inspector pane is open. The default is false. |
| `inspector-width` | integer | The width of the inspector pane, in pixels. |

The author can delete `.codicier/` at any time. The app then builds the index again and opens with the default window state.

## Versions of the format

The `format` key in `project.toml` and `binder.toml` gives the version. If the version is newer than the version that the app knows, the app opens the project as read only and tells the author.

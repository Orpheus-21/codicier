# Project folder format

Status: accepted on 2026-10-18. Format version 1.

A Codicier project is a normal folder of text files. You can read, edit, copy, back up, and put the folder in git without the app. The app adds nothing that you cannot read in a text editor.

## Rules for all files

- Text files use UTF-8 with no byte order mark.
- Lines end with a line feed (LF), and each file ends with one line feed.
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
    notes/              one Markdown file for each note
    research/           any files: images, PDF files, and other documents
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
| `section` | string | top level only | `manuscript`, `front`, `back`, `notes`, `research`, or `trash`. |
| `parent` | string | all below the top level | The id of the parent item. |
| `title` | string | all | The title in the binder. |
| `file` | string | text and file items | The path of the file, relative to the project folder, with `/` between the parts. |
| `status` | string | scenes | `draft`, `revised`, or `final`. The default is `draft`. |
| `pov` | string | scenes | The point of view character. |
| `place` | string | scenes | Where the scene happens. |
| `time` | string | scenes | When the scene happens, in free text. |
| `synopsis` | string | text items | A short summary. |
| `tags` | array of strings | text items | Tags. |
| `target` | integer | text items | The word target. |
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

A short story or an essay can have scenes at the top of the manuscript, with no parts or chapters.

### The trash

A move to the trash changes only `binder.toml`. The item goes to the top level with `section = "trash"`. Its `restore` key keeps the id of its old parent. The file stays where it is. When the author empties the trash, the app deletes the files of the items in the trash.

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

## The .codicier folder

| Path | Meaning |
|---|---|
| `.codicier/index.db` | The search index. The app builds it again from the files when it is missing. |
| `.codicier/state.toml` | The state of the window: the open item, the place of the cursor, and the sizes of the panels. |
| `.codicier/.gitignore` | One line, `*`, so git ignores the folder. |

The author can delete `.codicier/` at any time. The app then builds the index again and opens with the default window state.

## Versions of the format

The `format` key in `project.toml` and `binder.toml` gives the version. If the version is newer than the version that the app knows, the app opens the project as read only and tells the author.

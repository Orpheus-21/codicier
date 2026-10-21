#pragma once

#include "BinderModel.h"
#include "Document.h"
#include "Project.h"
#include "Watcher.h"

#include <QObject>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QUrl>
#include <QVariantMap>

#include <QtQml/qqmlregistration.h>
#include <algorithm>
#include <memory>

class Calendar;
class History;
class SpellChecker;
class Index;

// The open project. Set folder, and the session loads the project and fills
// the binder.
class ProjectSession : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString folder READ folder WRITE setFolder NOTIFY loaded)
    Q_PROPERTY(QString title READ title NOTIFY loaded)
    Q_PROPERTY(QString kind READ kind NOTIFY loaded)
    Q_PROPERTY(bool readOnly READ readOnly NOTIFY loaded)
    // The first load error, or "" when the project loaded.
    Q_PROPERTY(QString error READ error NOTIFY loaded)
    Q_PROPERTY(BinderModel *binder READ binder CONSTANT)
    // The text of the selected item, if it has one.
    Q_PROPERTY(Document *document READ document CONSTANT)
    // The id of the selected binder item, or "".
    Q_PROPERTY(QString currentId READ currentId WRITE setCurrentId NOTIFY currentIdChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY edited)
    // Live word counts for the status bar. chapterWords is -1 when the
    // selected item is not in a chapter.
    Q_PROPERTY(int sceneWords READ sceneWords NOTIFY wordsChanged)
    Q_PROPERTY(int chapterWords READ chapterWords NOTIFY wordsChanged)
    Q_PROPERTY(int manuscriptWords READ manuscriptWords NOTIFY wordsChanged)
    // The words added to the manuscript since the project opened, 0 or more.
    Q_PROPERTY(int sessionWords READ sessionWords NOTIFY wordsChanged)
    // The days in a row with words, up to today.
    Q_PROPERTY(int streak READ streak NOTIFY wordsChanged)
    // True underlines the unknown words of the open texts.
    Q_PROPERTY(bool spellCheck READ spellCheck WRITE setSpellCheck NOTIFY spellChanged)
    // The name of the dictionary, such as "en_GB", or "" if none was found.
    Q_PROPERTY(QString dictionary READ dictionary NOTIFY spellChanged)
    // The style checks of the open texts, as MarkdownHighlighter flags.
    Q_PROPERTY(int styleChecks MEMBER checksOfStyle WRITE setStyleChecks NOTIFY spellChanged)
    // True hides the Markdown marks of the open texts outside the paragraph
    // with the cursor.
    Q_PROPERTY(bool hideMarks MEMBER marksHidden WRITE setHideMarks NOTIFY spellChanged)
    // True if a git repository holds the project, so it has versions.
    Q_PROPERTY(bool versionsOn READ versionsOn NOTIFY versionsChanged)
    // True tints the text of the open scene by the draft pass that wrote it.
    Q_PROPERTY(bool showPasses READ showPasses WRITE setShowPasses NOTIFY passesChanged)
    // The word goal from project.toml, or 0.
    Q_PROPERTY(int goalWords READ goalWords NOTIFY wordsChanged)
    // The deadline of the project as yyyy-MM-dd, or "".
    Q_PROPERTY(QString deadline READ deadline NOTIFY wordsChanged)
    // The words to write each day, from today to the deadline, to reach the
    // goal. -1 with no goal or no deadline, or after the deadline.
    Q_PROPERTY(int dailyNeeded READ dailyNeeded NOTIFY wordsChanged)
    // Where a zip backup goes when the project closes, and how many to keep.
    // An empty folder means no backup.
    Q_PROPERTY(QString backupFolder MEMBER backupFolder NOTIFY backupChanged)
    Q_PROPERTY(int backupsToKeep MEMBER backupsToKeep NOTIFY backupChanged)

public:
    explicit ProjectSession(QObject *parent = nullptr);
    ~ProjectSession() override;
    ProjectSession(const ProjectSession &) = delete;
    ProjectSession &operator=(const ProjectSession &) = delete;

    QString folder() const { return project.folder; }
    void setFolder(const QString &folder);
    QString title() const { return project.title; }
    QString kind() const { return project.kind; }
    bool readOnly() const { return project.readOnly; }
    QString error() const { return loadError; }
    BinderModel *binder() { return &binderModel; }
    Document *document() { return &text; }
    QString currentId() const { return current; }
    void setCurrentId(const QString &id);

    // Loads the project from disk again.
    Q_INVOKABLE void reload();
    // Saves all open text and, if a file changed since the project opened
    // or since the last backup, writes a zip backup. Returns the path of the
    // zip, or "". Closing the project does this too.
    Q_INVOKABLE QString backup();

    // Adds a new item of kind ("scene", "chapter", "part", "folder", "page",
    // or "note") near the selected item, saves the binder, and selects the
    // new item. Returns its id, or "" with an error in lastError.
    Q_INVOKABLE QString add(const QString &kind);
    // The entity types: the six of the app, then the types of project.toml.
    // Each has a kind, a name for the heading, its fields, and custom.
    Q_INVOKABLE QVariantList entityTypes() const;
    // The plot frameworks: the id, the name, and the beats of each, with the
    // id, name, description, and place of each beat.
    Q_INVOKABLE QVariantList frameworks() const;
    // The mark between two scenes of a chapter that shows as one page:
    // "***", "#", or "" for a blank line. A project with no choice has "***".
    Q_PROPERTY(QString sceneBreak READ sceneBreak NOTIFY edited)
    QString sceneBreak() const;
    Q_INVOKABLE bool setSceneBreak(const QString &mark);
    // The framework of the project, by id, or "".
    Q_PROPERTY(QString framework READ framework NOTIFY edited)
    QString framework() const;
    Q_INVOKABLE bool setFramework(const QString &id);
    // The beat board: a column for scenes with no beat, then a column for
    // each beat of the framework, with the id, name, description, and the
    // scenes (id and title) of each. Empty with no framework.
    Q_INVOKABLE QVariantList beatBoard() const;
    // The plotlines of the project, in order: the id and the name of each.
    Q_INVOKABLE QVariantList plotlines() const;
    // Adds a plotline with a name. Returns its new id, or "" on an error.
    Q_INVOKABLE QString addPlotline(const QString &name);
    // The scenes of the manuscript in binder order, for the plot grid: the
    // id, the title, and the plotline ids of each.
    Q_INVOKABLE QVariantList plotGrid() const;
    // The relations between entities, or only those of one item: the index
    // in binder.toml, from, to, their titles, the name, and parent. A
    // relation to an item in the trash does not show.
    Q_INVOKABLE QVariantList relations(const QString &id = QString()) const;
    // The figures of the manuscript for the insights view: chapters, a list
    // of the items that hold scenes in binder order, each with an id, a
    // title, its words, its scenes (id, title, and words of each), pov (the
    // words of each POV name), and dialogue (the share of words between
    // double quotes, 0 to 1); and povNames, the POV names by their words in
    // the whole manuscript, most first; and words, the 30 most used words of
    // the manuscript with a word and a count each, with no common English
    // word and no word of one or two letters; and characters, the
    // characters of the codex (id and title), in the order of the presence
    // list of each chapter, which counts the mentions of each.
    Q_INVOKABLE QVariantMap insights();
    // Copies a local image into maps/ and adds it as a map in the codex.
    // Returns the new id, or "" with the reason in lastError.
    Q_INVOKABLE QString addMap(const QUrl &image);
    // The URL of the file of an item, such as the image of a map.
    Q_INVOKABLE QUrl fileUrl(const QString &id) const;
    // The pins of a map: the index in binder.toml, the place, its title, and
    // x and y as parts of the width and height of the image, 0 to 1.
    Q_INVOKABLE QVariantList pins(const QString &mapId) const;
    // Pins an entity of the codex, such as a place, to a map.
    Q_INVOKABLE bool addPin(const QString &mapId, const QString &placeId, double x, double y);
    Q_INVOKABLE bool movePin(int index, double x, double y);
    Q_INVOKABLE bool removePin(int index);
    // Links two entities with a name, such as "sister of". With parent,
    // from is a parent of to, and the name is "parent of". Returns false on
    // an error.
    Q_INVOKABLE bool addRelation(const QString &from, const QString &to, const QString &name,
                                 bool parent);
    Q_INVOKABLE bool removeRelation(int index);
    // The family tree from the parent links: generations (a list of rows,
    // oldest first, each a list of people with an id and a title, in title
    // order) and links (each with from, the parent, and to, the child).
    Q_INVOKABLE QVariantMap familyTree() const;
    // The items with a date in story time, outside the trash, as tracks by
    // "pov" or by "plotline", in name order: the name and the events of
    // each track, by day. Each event has an id, title, kind, day number,
    // and a label of its date.
    Q_INVOKABLE QVariantList timeline(const QString &groupBy) const;
    // The months of the calendar of the project, with the name and days of
    // each, or none for the Gregorian calendar.
    Q_INVOKABLE QVariantList calendarMonths() const;
    // Sets the months of the calendar, each a map with a name and days from
    // 1 to 1000. No months sets the Gregorian calendar. Returns false and
    // changes nothing for a month that is not valid.
    Q_INVOKABLE bool setCalendar(const QVariantList &months);
    // Sets the name and the fields of a type in project.toml. An empty kind
    // makes a new type from the name. Returns the kind, or "" on an error.
    Q_INVOKABLE QString saveEntityType(const QString &kind, const QString &name,
                                       const QStringList &fields);
    // The entity with this title or alias, with no regard to case, or "".
    Q_INVOKABLE QString entityByName(const QString &name) const;
    // The entity of the [[link]] at position in text, or "".
    Q_INVOKABLE QString entityAt(const QString &text, int position) const;
    // The texts of the manuscript that name the entity, by its title or an
    // alias, as whole words with no regard to case, in binder order. Each
    // has an id, a title, and the count of mentions.
    Q_INVOKABLE QVariantList mentionsOf(const QString &id);
    // Sets the value of one field of an entity. An empty value removes it.
    Q_INVOKABLE bool setEntityField(const QString &id, const QString &field, const QString &value);
    // Gives the item a new title and saves the binder.
    Q_INVOKABLE bool rename(const QString &id, const QString &title);
    // Saves a search in the binder, in the searches section. Its title is
    // the query. Returns the new id, or "" on an error.
    Q_INVOKABLE QString saveSearch(const QString &query, bool regex);
    // Moves the item and its branch to the trash. No file leaves the disk.
    Q_INVOKABLE bool moveToTrash(const QString &id);
    // Moves an item from the trash back to its old parent, or to the top of
    // its section if the old parent is gone.
    Q_INVOKABLE bool restore(const QString &id);
    // Moves the item before or after target, or inside target as its last
    // child ("before", "after", or "inside"). Returns false, and changes
    // nothing, for a move that the format does not allow.
    Q_INVOKABLE bool move(const QString &id, const QString &target, const QString &position);
    // True if move() with the same values would succeed.
    Q_INVOKABLE bool canMove(const QString &id, const QString &target,
                             const QString &position) const;
    // Moves the item one place up (-1) or down (+1) among its siblings.
    Q_INVOKABLE bool moveBy(const QString &id, int delta);
    // Splits the scene at a position in its text: the text after it goes to
    // a new scene right after this one. Returns the id of the new scene.
    Q_INVOKABLE QString split(const QString &id, int position);
    // Joins the next scene in the same parent to the end of this scene, and
    // moves the next scene to the trash with its file.
    Q_INVOKABLE bool mergeWithNext(const QString &id);
    // The id of the next scene in the same parent, or "".
    Q_INVOKABLE QString nextScene(const QString &id) const;
    // The inspector fields of an item: title, kind, status, pov, place,
    // time, synopsis, tags, target, notes, and words.
    Q_INVOKABLE QVariantMap fields(const QString &id) const;
    // Sets one field and saves the binder. An empty value removes the key.
    // Returns false for an unknown key or a value that is not valid.
    Q_INVOKABLE bool setField(const QString &id, const QString &key, const QVariant &value);
    // The text items below the item, in binder order, as maps with id,
    // title, and file. For the composite view.
    Q_INVOKABLE QVariantList textsUnder(const QString &id) const;
    // The direct children of an item, in binder order: the id, title, kind,
    // synopsis, status, and words of each. For the corkboard and outliner.
    Q_INVOKABLE QVariantList childrenOf(const QString &id) const;
    // The texts of an item and of all items under it, in binder order: the
    // id, title, status, pov, place, time, words, and the values of custom
    // fields of each. For the outliner.
    Q_INVOKABLE QVariantList outlineOf(const QString &id) const;
    // Every item outside the trash, in binder order: the id, the title, the
    // kind, and the path of titles above it, as "Part One / The Letter".
    Q_INVOKABLE QVariantList items() const;
    // The state of the view in .codicier/state.toml, such as the open item
    // and the sizes of the panes. Values are text, true or false, or whole
    // numbers. A missing or bad file gives an empty map.
    Q_INVOKABLE QVariantMap viewState() const;
    Q_INVOKABLE bool saveViewState(const QVariantMap &state);
    // Connects a document of the composite view, for the item id, to the
    // watcher and to the live word counts, as the editor document is.
    Q_INVOKABLE void trackDocument(Document *document, const QString &id);
    // Saves the text of the item and copies it to snapshots/. Returns the
    // path of the snapshot, or "".
    Q_INVOKABLE QString takeSnapshot(const QString &id, const QString &title);
    // The snapshots of the item, newest first, as maps with path, time,
    // title, and label.
    Q_INVOKABLE QVariantList snapshots(const QString &id) const;
    // Writes a snapshot back to the item. The current text first becomes a
    // snapshot "Before restore".
    Q_INVOKABLE bool restoreSnapshot(const QString &id, const QString &path);
    // The word difference from a snapshot to the current text of the item:
    // a map with segments (each a map with kind "same", "added", or
    // "removed", and text), addedWords, and removedWords. snapshotPath can
    // also be "version:<hash>" for the text in a saved version.
    Q_INVOKABLE QVariantMap compare(const QString &id, const QString &snapshotPath) const;
    // The number of items in the trash, with the items below them.
    Q_INVOKABLE int trashCount() const;
    // Writes a backup, then deletes the files and snapshots of every item in
    // the trash and removes the items from the binder. Returns the number of
    // items deleted, or -1 on an error.
    Q_INVOKABLE int emptyTrash();
    // Full text search over the titles and the texts outside the trash, best
    // match first. Each result has an id, a title, and a snippet in rich text
    // with the match in bold. With regex, the query is a regular expression,
    // and the results come in binder order.
    Q_INVOKABLE QVariantList search(const QString &query, bool regex = false);
    // Why a regular expression is not valid, or "".
    Q_INVOKABLE QString patternError(const QString &pattern) const;
    // Where the first word of the query is in the text of an item, or -1.
    Q_INVOKABLE int firstMatch(const QString &id, const QString &query) const;
    // Every match of a replace in the manuscript, before the change: for
    // each scene the id, the title, the count, and the lines in rich text
    // with the old words struck through and the new words in bold. A plain
    // query matches as written, as a whole word.
    Q_INVOKABLE QVariantList replacePreview(const QString &query, const QString &replacement,
                                            bool regex);
    // Takes a snapshot of each scene with a match, then replaces. Returns
    // the number of replacements, or -1 on an error.
    Q_INVOKABLE int replaceAll(const QString &query, const QString &replacement, bool regex);
    // The first match of a search in the text of an item: a start and a
    // length. The start is -1 if there is no match.
    Q_INVOKABLE QVariantMap matchIn(const QString &id, const QString &query, bool regex) const;
    // The last error of an edit, or "".
    QString lastError() const { return editError; }
    int sceneWords() const { return text.file().isEmpty() ? 0 : text.words(); }
    int chapterWords() const;
    int manuscriptWords() const { return binderModel.manuscriptWords(); }
    int sessionWords() const { return std::max(0, manuscriptWords() - sessionStart); }
    int streak() const;
    bool versionsOn() const;
    // Turns on versions for the project and saves a first version. Returns
    // an error text, or "".
    Q_INVOKABLE QString turnOnVersions();
    bool showPasses() const { return passTints; }
    void setShowPasses(bool on);
    // The draft passes, in order: the number and the name of each.
    Q_INVOKABLE QVariantList passes() const;
    // Starts a new draft pass: a snapshot "Pass N start" of every text, and
    // a [[pass]] with the name in project.toml. Returns the pass number, or
    // 0 on an error.
    Q_INVOKABLE int startPass(const QString &name);
    // The parts of the text of an item that each pass wrote: the start, the
    // length, and the pass number of each.
    Q_INVOKABLE QVariantList passRanges(const QString &id) const;
    // Sets the checklist of a pass, by its number from 1.
    Q_INVOKABLE bool setPassChecks(int number, const QStringList &checks);
    // The checklist of a pass: checks, the list of checks, and chapters,
    // each with an id, a title, and ticked, one true or false for each check.
    Q_INVOKABLE QVariantMap checklist(int number) const;
    // Ticks a check of a pass for a chapter, or takes the tick away.
    Q_INVOKABLE bool setTick(int number, const QString &check, const QString &itemId, bool ticked);
    // The comments, of one item or of all items for an empty id, in order:
    // the index in binder.toml, the item, its title, the quote, the text,
    // resolved, and the start of the quote in the text now, or -1 when the
    // quote is gone. A comment of an item in the trash does not show.
    Q_INVOKABLE QVariantList comments(const QString &id = QString()) const;
    // Adds a comment on the quote at start in the text of an item.
    Q_INVOKABLE bool addComment(const QString &id, int start, const QString &quote,
                                const QString &text);
    Q_INVOKABLE bool resolveComment(int index, bool resolved);
    Q_INVOKABLE bool removeComment(int index);
    // Saves a version of the project with a message, or with the date and
    // time for an empty message. Returns an error text, or "".
    Q_INVOKABLE QString saveVersion(const QString &message);
    // The versions of the project, or of the text of an item, newest first:
    // the hash, the time as ISO 8601, a label of the time, and the message.
    Q_INVOKABLE QVariantList versions(const QString &id = QString()) const;
    // Puts the text of an item back to a saved version. A version of the
    // state before goes first, so the restore can be undone. Returns an
    // error text, or "".
    Q_INVOKABLE QString restoreVersion(const QString &id, const QString &hash);
    // Puts the whole project back to a saved version, with a version of the
    // state before first. Returns an error text, or "".
    Q_INVOKABLE QString restoreProject(const QString &hash);
    bool spellCheck() const { return checkSpelling; }
    void setSpellCheck(bool on);
    QString dictionary() const;
    // The spelling suggestions for a word, or none with no dictionary.
    Q_INVOKABLE QStringList suggestions(const QString &word) const;
    // True for a word that the dictionary or the project knows.
    Q_INVOKABLE bool isWordCorrect(const QString &word) const;
    // Adds a word to dictionary.txt of the project and checks the texts
    // again. Returns false on an error.
    Q_INVOKABLE bool addToDictionary(const QString &word);
    // The net words of the last count days, oldest first, up to today: the
    // date as yyyy-MM-dd and the words of each.
    Q_INVOKABLE QVariantList historyDays(int count) const;
    int goalWords() const;
    QString deadline() const;
    int dailyNeeded() const;
    // Sets the word goal (0 for none) and the deadline (yyyy-MM-dd, or "" for
    // none) in project.toml. Returns false for a date that is not valid.
    Q_INVOKABLE bool setGoal(int words, const QString &deadline);

signals:
    void loaded();
    void currentIdChanged();
    // The binder changed through an add, a rename, or a move.
    void edited();
    void wordsChanged();
    // A snapshot was taken or restored.
    void snapshotsChanged();
    void backupChanged();
    void spellChanged();
    void versionsChanged();
    void passesChanged();
    // A text file changed: the app saved it, or another program changed it.
    void textsChanged();

private:
    Project project;
    QString loadError;
    BinderModel binderModel;
    QString current;
    Document text;
    std::unique_ptr<Watcher> watcher;
    // The documents of the composite view.
    QList<QPointer<Document>> tracked;
    QString backupFolder;
    int backupsToKeep = 10;
    // A file changed since the project opened or since the last backup.
    bool changedSinceBackup = false;
    // The manuscript words when the project opened.
    int sessionStart = 0;
    // The writing history, and the manuscript words of its last count.
    std::unique_ptr<History> history;
    std::unique_ptr<SpellChecker> spell;
    bool checkSpelling = true;
    bool passTints = false;
    // Gives the pass ranges of the open text to its document.
    void applyPasses();
    // Gives the places of the open comments of the open text to its document.
    void applyComments();
    // The text of an item now, with the typing that is not saved yet.
    QString textOf(int index) const;
    int checksOfStyle = 0;
    void setStyleChecks(int checks);
    bool marksHidden = false;
    void setHideMarks(bool hide);
    // Gives the checker, or none, to the open text and the tracked texts.
    void applySpelling();
    // The calendar of story time of the project.
    Calendar calendar() const;
    // An item that holds scenes directly, such as a chapter.
    struct ChapterGroup
    {
        QString id;
        QString title;
        QList<int> scenes;
    };
    // The chapters of the manuscript in binder order. Scenes at the top of
    // the manuscript form one group, "Manuscript".
    QList<ChapterGroup> chapterGroups() const;
    // Teaches the checker the words of dictionary.txt and of the names of
    // the codex.
    void teachSpelling();
    int historyTotal = 0;
    // The search index builds at the first search after a load or a binder
    // edit. Between those, only the files that changed are read again.
    std::unique_ptr<Index> index;
    bool indexStale = true;
    QSet<QString> staleFiles;

    void watch();
    // Saves the open text and the texts of the composite view.
    void saveAll();
    // A file was written by the app.
    void noteSaved(const QString &file);
    int indexOf(const QString &id) const;
    void place(int index, int parent, int position, const QString &section);
    void detach(int index);
    // Where a move would put the item: the parent (-1 for the top level),
    // the position, and the section. Returns false if the move is not valid.
    bool resolveMove(int index, int target, const QString &position, int *parent, int *at,
                     QString *section) const;
    void setSection(int index, const QString &section);
    bool commit(const QStringList &newFiles);
    QString editError;
};

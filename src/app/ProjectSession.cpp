#include "ProjectSession.h"

#include "Backup.h"
#include "Calendar.h"
#include "Diff.h"
#include "Frameworks.h"
#include "History.h"
#include "Index.h"
#include "Naming.h"
#include "ProjectStore.h"
#include "Snapshots.h"
#include "SpellChecker.h"
#include "Thesaurus.h"
#include "Versions.h"

#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QLocale>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>

#include <array>
#include <functional>
#include <sstream>
#include <utility>

namespace {

// Defined below, next to the snapshots that use it most.
QString readTextFile(const QString &path, bool *ok);

} // namespace

ProjectSession::ProjectSession(QObject *parent) : QObject(parent)
{
    connect(&text, &Document::saved, this, [this](const QString &file) {
        changedSinceBackup = true;
        noteSaved(file);
        // The tints and the comments follow the text after each save.
        if (passTints)
            applyPasses();
        applyComments();
    });
    connect(this, &ProjectSession::currentIdChanged, this, [this] {
        if (passTints)
            applyPasses();
        applyComments();
    });
    connect(this, &ProjectSession::snapshotsChanged, this, [this] { changedSinceBackup = true; });
    // The live count of the open text goes into the binder, so the counts of
    // the chapter and the book follow it.
    connect(&text, &Document::wordsChanged, this, [this] {
        if (!text.file().isEmpty())
            binderModel.setItemWords(current, text.words());
        emit wordsChanged();
    });
    connect(this, &ProjectSession::currentIdChanged, this, &ProjectSession::wordsChanged);
    connect(this, &ProjectSession::edited, this, &ProjectSession::wordsChanged);
    connect(this, &ProjectSession::loaded, this, &ProjectSession::wordsChanged);
    // Each change of the manuscript words goes to the history of today.
    connect(this, &ProjectSession::wordsChanged, this, [this] {
        const int total = manuscriptWords();
        if (history && total != historyTotal)
            history->add(QDate::currentDate(), total - historyTotal);
        historyTotal = total;
    });
    connect(&text, &Document::dirtyChanged, this, [this] {
        if (watcher && !text.file().isEmpty())
            watcher->setUnsaved(text.file(), text.dirty());
    });
}

ProjectSession::~ProjectSession()
{
    backup();
}

void ProjectSession::noteSaved(const QString &file)
{
    if (history)
        history->save();
    staleFiles.insert(file);
    emit textsChanged();
    if (watcher)
        watcher->noteSaved(file);
}

QString ProjectSession::backup()
{
    saveAll();
    if (history)
        history->save();
    if (!changedSinceBackup || project.folder.isEmpty() || !loadError.isEmpty()
        || backupFolder.isEmpty())
        return { };
    QString error;
    const QString zip = Backup::write(project.folder, backupFolder, backupsToKeep, &error);
    if (zip.isEmpty())
        qWarning("Codicier: backup failed: %s", qPrintable(error));
    else
        changedSinceBackup = false;
    return zip;
}

void ProjectSession::setFolder(const QString &folder)
{
    if (folder == project.folder && loadError.isEmpty())
        return;
    // The project that closes gets its backup.
    if (folder != project.folder)
        backup();
    project = Project();
    project.folder = folder;
    changedSinceBackup = false;
    history.reset();
    reload();
    // The session and the history count the words added from now on.
    sessionStart = manuscriptWords();
    historyTotal = sessionStart;
    if (!folder.isEmpty() && loadError.isEmpty() && !project.readOnly)
        history = std::make_unique<History>(folder);
    // The dictionary of the project language.
    spell.reset();
    thesaurus.reset();
    if (!folder.isEmpty() && loadError.isEmpty()) {
        const QString language
            = QString::fromStdString(project.data["language"].value_or(std::string("en_US")));
        spell = std::make_unique<SpellChecker>(language);
        thesaurus = std::make_unique<Thesaurus>(language);
    }
    teachSpelling();
    knownNames = entityNames();
    applySpelling();
    emit spellChanged();
    emit versionsChanged();
    emit wordsChanged();
}

void ProjectSession::reload()
{
    const QString folder = project.folder;
    if (folder.isEmpty()) {
        setCurrentId(QString());
        project = Project();
        loadError.clear();
        binderModel.setProject(project);
        watcher.reset();
        index.reset();
        emit loaded();
        return;
    }
    const QString previous = current;
    setCurrentId(QString());
    LoadResult result = ProjectStore::load(folder);
    loadError = result.ok() ? QString() : result.errors.first().text();
    project = std::move(result.project);
    project.folder = folder;
    binderModel.setProject(project);
    watch();
    index.reset();
    indexStale = true;
    staleFiles.clear();
    emit loaded();
    // After a reload the same item stays selected, if it is still there.
    setCurrentId(previous);
}

// Outside edits: a clean text reloads, and unsaved text gets one conflict.
void ProjectSession::watch()
{
    watcher.reset();
    if (!loadError.isEmpty())
        return;
    watcher = std::make_unique<Watcher>(project);
    connect(watcher.get(), &Watcher::textChanged, this, [this](const QString &file) {
        staleFiles.insert(file);
        emit textsChanged();
        if (file == text.file())
            text.reloadIfClean();
        for (const QPointer<Document> &document : std::as_const(tracked)) {
            if (document && document->file() == file)
                document->reloadIfClean();
        }
    });
    connect(watcher.get(), &Watcher::conflict, this, [this](const QString &file) {
        if (file == text.file())
            text.markConflict();
        for (const QPointer<Document> &document : std::as_const(tracked)) {
            if (document && document->file() == file)
                document->markConflict();
        }
    });
    // Queued: reload() deletes this watcher, and the watcher must first leave
    // the scan that sent the signal.
    connect(watcher.get(), &Watcher::projectChanged, this, &ProjectSession::reload,
            Qt::QueuedConnection);
}

void ProjectSession::setCurrentId(const QString &id)
{
    if (id == current)
        return;
    // Only a known item can be selected.
    QString file;
    bool known = id.isEmpty();
    for (const ProjectItem &item : std::as_const(project.items)) {
        if (item.id != id)
            continue;
        known = true;
        if (!holdsNoText(item.kind) && !item.missingFile)
            file = item.file;
    }
    if (!known)
        return;
    current = id;
    text.open(project.folder, file);
    emit currentIdChanged();
}

namespace {

// What each kind can hold, and where a new item of a kind goes.
// The entity types that come with the app, with the heading of each.
constexpr std::array<std::pair<QStringView, QStringView>, 6> builtInTypes { {
    { u"character", u"Characters" },
    { u"place", u"Places" },
    { u"item", u"Items" },
    { u"faction", u"Factions" },
    { u"lore", u"Lore" },
    { u"term", u"Terms" },
} };

// Every other kind is an entity type: the app checks the kind against the
// types of the project when it loads or adds an item.
bool isEntity(const QString &kind)
{
    static const QStringList base { "part", "chapter", "folder", "scene", "page",
                                    "note", "file",    "search", "map" };
    return !kind.isEmpty() && !base.contains(kind);
}

// The [[entity-type]] tables of project.toml.
toml::array *typeTables(Project &project)
{
    return project.data.get_as<toml::array>("entity-type");
}

QStringList strings(const toml::node *node)
{
    QStringList list;
    if (const toml::array *array = node != nullptr ? node->as_array() : nullptr) {
        for (const toml::node &each : *array)
            list.append(QString::fromStdString(each.value_or(std::string())));
    }
    return list;
}

bool canHold(const QString &parentKind, const QString &kind)
{
    if (parentKind == QLatin1String("folder"))
        return true;
    if (parentKind == QLatin1String("part"))
        return kind == QLatin1String("chapter") || kind == QLatin1String("scene");
    if (parentKind == QLatin1String("chapter"))
        return kind == QLatin1String("scene");
    return false;
}

QString homeSection(const QString &kind)
{
    if (kind == QLatin1String("page"))
        return QStringLiteral("front");
    if (kind == QLatin1String("note"))
        return QStringLiteral("notes");
    if (kind == QLatin1String("search"))
        return QStringLiteral("searches");
    if (isEntity(kind))
        return QStringLiteral("codex");
    return QStringLiteral("manuscript");
}

QString folderOf(const QString &kind)
{
    if (kind == QLatin1String("scene"))
        return QStringLiteral("manuscript/");
    if (kind == QLatin1String("page"))
        return QStringLiteral("matter/");
    if (kind == QLatin1String("note"))
        return QStringLiteral("notes/");
    if (isEntity(kind))
        return QStringLiteral("codex/");
    return { };
}

QString newTitle(const QString &kind)
{
    return QStringLiteral("New ") + kind;
}

// The sections where an item of kind can be, as docs/format.md says.
bool fitsSection(const QString &kind, const QString &section)
{
    if (kind == QLatin1String("folder"))
        return true;
    if (kind == QLatin1String("page"))
        return section == QLatin1String("front") || section == QLatin1String("back");
    return section == homeSection(kind);
}

} // namespace

int ProjectSession::indexOf(const QString &id) const
{
    for (qsizetype i = 0; i < project.items.size(); ++i) {
        if (project.items.at(i).id == id)
            return static_cast<int>(i);
    }
    return -1;
}

void ProjectSession::setSection(int index, const QString &section)
{
    project.items[index].section = section;
    for (const int child : std::as_const(project.items.at(index).children))
        setSection(child, section);
}

// Takes the item out of its parent or out of the top level.
void ProjectSession::detach(int index)
{
    ProjectItem &item = project.items[index];
    if (item.parent >= 0)
        project.items[item.parent].children.removeOne(index);
    else
        project.roots.removeOne(index);
    item.parent = -1;
}

// Puts the item under parent (or at the top level for -1) at position.
void ProjectSession::place(int index, int parent, int position, const QString &section)
{
    QList<int> &list = parent >= 0 ? project.items[parent].children : project.roots;
    list.insert(std::clamp<qsizetype>(position, 0, list.size()), index);
    project.items[index].parent = parent;
    setSection(index, parent >= 0 ? project.items.at(parent).section : section);
}

bool ProjectSession::commit(const QStringList &newFiles)
{
    // The open text saves first, so the counts that the binder reads from the
    // files include the latest typing.
    text.save();
    changedSinceBackup = true;
    indexStale = true;
    const QList<LoadError> errors = ProjectStore::save(project);
    editError = errors.isEmpty() ? QString() : errors.first().text();
    if (watcher) {
        watcher->track(project);
        watcher->noteSaved(QStringLiteral("binder.toml"));
        for (const QString &file : newFiles)
            watcher->noteSaved(file);
    }
    binderModel.setProject(project);
    // A new or renamed entity is a word of the story.
    teachSpelling();
    // The pages underline the names of the codex.
    if (const QStringList names = entityNames(); names != knownNames) {
        knownNames = names;
        applySpelling();
    }
    emit edited();
    return errors.isEmpty();
}

QVariantList ProjectSession::plotlines() const
{
    QVariantList list;
    if (const toml::array *lines = project.data.get_as<toml::array>("plotline")) {
        for (const toml::node &node : *lines) {
            if (const toml::table *line = node.as_table())
                list.append(QVariantMap {
                    { QStringLiteral("id"),
                      QString::fromStdString((*line)["id"].value_or(std::string())) },
                    { QStringLiteral("name"),
                      QString::fromStdString((*line)["name"].value_or(std::string())) } });
        }
    }
    return list;
}

QString ProjectSession::addMap(const QUrl &image)
{
    editError.clear();
    const QFileInfo source(image.isLocalFile() ? image.toLocalFile() : image.toString());
    static const QStringList kinds { "png", "jpg", "jpeg", "webp", "gif", "bmp" };
    if (project.readOnly || !source.isFile() || !kinds.contains(source.suffix().toLower())) {
        editError = QStringLiteral("Choose an image file: PNG, JPEG, WebP, GIF, or BMP.");
        emit edited();
        return { };
    }
    // The image is copied into maps/, with a name that no other file has.
    const QDir dir(project.folder);
    dir.mkpath(QStringLiteral("maps"));
    const QString extension = u'.' + source.suffix().toLower();
    QString name = Naming::fileNameFor(source.completeBaseName(), { });
    name.chop(3);
    QString file = QStringLiteral("maps/") + name + extension;
    for (int n = 2; QFileInfo::exists(dir.filePath(file)); ++n)
        file = QStringLiteral("maps/%1-%2%3").arg(name).arg(n).arg(extension);
    if (!QFile::copy(source.absoluteFilePath(), dir.filePath(file))) {
        editError = QStringLiteral("Cannot copy the image into the project.");
        emit edited();
        return { };
    }
    QSet<QString> ids;
    for (const ProjectItem &each : std::as_const(project.items))
        ids.insert(each.id);
    ProjectItem item;
    item.id = Naming::newItemId(ids);
    item.kind = QStringLiteral("map");
    item.title = source.completeBaseName();
    item.file = file;
    project.items.append(item);
    place(static_cast<int>(project.items.size() - 1), -1, static_cast<int>(project.roots.size()),
          QStringLiteral("codex"));
    commit({ file });
    setCurrentId(item.id);
    return item.id;
}

QVariantList ProjectSession::pins(const QString &mapId) const
{
    QVariantList list;
    const toml::array *pins = project.binderData.get_as<toml::array>("pin");
    if (pins == nullptr)
        return list;
    for (std::size_t i = 0; i < pins->size(); ++i) {
        const toml::table *pin = (*pins)[i].as_table();
        if (pin == nullptr
            || QString::fromStdString((*pin)["map"].value_or(std::string())) != mapId)
            continue;
        const QString place = QString::fromStdString((*pin)["place"].value_or(std::string()));
        const int index = indexOf(place);
        // A pin of a place in the trash does not show.
        if (index < 0 || binderModel.isInTrash(place))
            continue;
        list.append(QVariantMap { { QStringLiteral("index"), static_cast<int>(i) },
                                  { QStringLiteral("place"), place },
                                  { QStringLiteral("title"), project.items.at(index).title },
                                  { QStringLiteral("x"), (*pin)["x"].value_or(0.0) },
                                  { QStringLiteral("y"), (*pin)["y"].value_or(0.0) } });
    }
    return list;
}

bool ProjectSession::addPin(const QString &mapId, const QString &placeId, double x, double y)
{
    const int map = indexOf(mapId);
    const int place = indexOf(placeId);
    if (project.readOnly || map < 0 || place < 0
        || project.items.at(map).kind != QLatin1String("map")
        || !isEntity(project.items.at(place).kind) || x < 0 || x > 1 || y < 0 || y > 1)
        return false;
    if (project.binderData.get_as<toml::array>("pin") == nullptr)
        project.binderData.insert_or_assign("pin", toml::array { });
    project.binderData.get_as<toml::array>("pin")->push_back(
        toml::table { { "map", mapId.toStdString() },
                      { "place", placeId.toStdString() },
                      { "x", x },
                      { "y", y } });
    return commit({ });
}

bool ProjectSession::movePin(int index, double x, double y)
{
    toml::array *pins = project.binderData.get_as<toml::array>("pin");
    if (project.readOnly || pins == nullptr || index < 0 || index >= static_cast<int>(pins->size())
        || x < 0 || x > 1 || y < 0 || y > 1)
        return false;
    toml::table *pin = (*pins)[static_cast<std::size_t>(index)].as_table();
    if (pin == nullptr)
        return false;
    pin->insert_or_assign("x", x);
    pin->insert_or_assign("y", y);
    return commit({ });
}

bool ProjectSession::removePin(int index)
{
    toml::array *pins = project.binderData.get_as<toml::array>("pin");
    if (project.readOnly || pins == nullptr || index < 0 || index >= static_cast<int>(pins->size()))
        return false;
    pins->erase(pins->begin() + index);
    if (pins->empty())
        project.binderData.erase("pin");
    return commit({ });
}

QUrl ProjectSession::fileUrl(const QString &id) const
{
    const int index = indexOf(id);
    if (index < 0 || project.items.at(index).file.isEmpty())
        return { };
    return QUrl::fromLocalFile(QDir(project.folder).filePath(project.items.at(index).file));
}

QList<ProjectSession::ChapterGroup> ProjectSession::chapterGroups() const
{
    // A group is an item that holds scenes directly, such as a chapter, in
    // binder order. Scenes at the top of the manuscript form one group.
    QList<ChapterGroup> groups;
    ChapterGroup loose { QString(), QStringLiteral("Manuscript"), { } };
    const std::function<void(int)> walk = [&](int at) {
        const ProjectItem &item = project.items.at(at);
        ChapterGroup group { item.id, item.title, { } };
        for (const int child : item.children) {
            if (project.items.at(child).kind == QLatin1String("scene"))
                group.scenes.append(child);
        }
        if (!group.scenes.isEmpty())
            groups.append(group);
        for (const int child : item.children)
            walk(child);
    };
    for (const int root : project.roots) {
        if (project.items.at(root).section != QLatin1String("manuscript"))
            continue;
        if (project.items.at(root).kind == QLatin1String("scene"))
            loose.scenes.append(root);
        walk(root);
    }
    if (!loose.scenes.isEmpty())
        groups.prepend(loose);
    return groups;
}

QVariantMap ProjectSession::insights()
{
    // The figures read the files, so the latest typing goes to disk first.
    saveAll();
    const QList<ChapterGroup> groups = chapterGroups();
    // Dialogue is text between straight or curly double quotes.
    static const QRegularExpression quoted(QStringLiteral("\"[^\"\\n]*\"|“[^”\\n]*”"));
    QHash<QString, int> povTotals;
    // The words of the manuscript, with the common English words left out.
    static const QRegularExpression wordPattern(QStringLiteral("\\p{L}+(?:['’]\\p{L}+)*"),
                                                QRegularExpression::UseUnicodePropertiesOption);
    static const QSet<QString> stopWords {
        "the",     "and",    "for",        "are",   "but",    "not",    "you",    "all",
        "any",     "can",    "her",        "was",   "one",    "our",    "out",    "his",
        "has",     "had",    "him",        "she",   "they",   "them",   "their",  "there",
        "then",    "than",   "that",       "this",  "these",  "those",  "with",   "from",
        "have",    "were",   "what",       "when",  "where",  "which",  "who",    "whom",
        "why",     "how",    "will",       "would", "could",  "should", "into",   "onto",
        "over",    "under",  "again",      "about", "after",  "before", "just",   "only",
        "very",    "more",   "most",       "some",  "such",   "said",   "says",   "say",
        "its",     "it's",   "i'm",        "don't", "didn't", "did",    "does",   "doing",
        "done",    "been",   "being",      "your",  "yours",  "mine",   "myself", "himself",
        "herself", "itself", "themselves", "here",  "now",    "also",   "too",    "off",
        "own",     "same",   "each",       "other", "both",   "few",    "nor",    "yet",
        "upon",    "back",   "down",       "like",  "know",   "knew",   "came",   "come",
        "went",    "get",    "got",        "let",   "may",    "might",  "must",   "shall",
        "still",   "even",   "ever",       "never", "much",   "many",   "well",   "way",
        "because", "while",  "though",
    };
    QHash<QString, int> frequency;
    // The characters, each with a pattern of its title and aliases as whole
    // words, as for the mentions.
    struct Character
    {
        QString id;
        QString title;
        QRegularExpression pattern;
    };
    QList<Character> characters;
    for (const ProjectItem &item : std::as_const(project.items)) {
        if (item.kind != QLatin1String("character") || binderModel.isInTrash(item.id))
            continue;
        QStringList names = strings(item.data.get("aliases"));
        names.prepend(item.title);
        QStringList escaped;
        for (const QString &name : std::as_const(names)) {
            if (!name.trimmed().isEmpty())
                escaped.append(QRegularExpression::escape(name.trimmed()));
        }
        std::ranges::sort(escaped,
                          [](const QString &a, const QString &b) { return a.size() > b.size(); });
        characters.append(
            { item.id, item.title,
              QRegularExpression(QStringLiteral("(?<!\\w)(") + escaped.join(u'|')
                                     + QStringLiteral(")(?!\\w)"),
                                 QRegularExpression::CaseInsensitiveOption
                                     | QRegularExpression::UseUnicodePropertiesOption) });
    }
    QVariantList chapters;
    for (const ChapterGroup &group : std::as_const(groups)) {
        QVariantList scenes;
        int words = 0;
        int dialogue = 0;
        QHash<QString, int> povWords;
        QVariantList present(characters.size(), 0);
        for (const int index : group.scenes) {
            const ProjectItem &scene = project.items.at(index);
            const int count = binderModel.wordsOf(scene.id);
            words += count;
            const QString pov
                = QString::fromStdString(scene.data["pov"].value_or(std::string())).trimmed();
            povWords[pov.isEmpty() ? QStringLiteral("No POV") : pov] += count;
            QFile file(QDir(project.folder).filePath(scene.file));
            if (file.open(QIODevice::ReadOnly)) {
                const QString body = QString::fromUtf8(file.readAll());
                QRegularExpressionMatchIterator it = quoted.globalMatch(body);
                while (it.hasNext())
                    dialogue += Index::countWords(it.next().captured());
                for (qsizetype c = 0; c < characters.size(); ++c) {
                    QRegularExpressionMatchIterator found
                        = characters.at(c).pattern.globalMatch(body);
                    int count = present.at(c).toInt();
                    while (found.hasNext()) {
                        found.next();
                        ++count;
                    }
                    present[c] = count;
                }
                QRegularExpressionMatchIterator words = wordPattern.globalMatch(body);
                while (words.hasNext()) {
                    const QString word = words.next().captured().toLower();
                    if (word.size() > 2 && !stopWords.contains(word))
                        ++frequency[word];
                }
            }
            scenes.append(QVariantMap { { QStringLiteral("id"), scene.id },
                                        { QStringLiteral("title"), scene.title },
                                        { QStringLiteral("words"), count } });
        }
        QVariantMap pov;
        for (auto it = povWords.cbegin(); it != povWords.cend(); ++it) {
            pov.insert(it.key(), it.value());
            povTotals[it.key()] += it.value();
        }
        chapters.append(QVariantMap {
            { QStringLiteral("id"), group.id },
            { QStringLiteral("title"), group.title },
            { QStringLiteral("words"), words },
            { QStringLiteral("scenes"), scenes },
            { QStringLiteral("pov"), pov },
            { QStringLiteral("dialogue"), words > 0 ? double(dialogue) / words : 0.0 },
            { QStringLiteral("presence"), present } });
    }
    // The POV names by their words in the whole manuscript, most first, so
    // each name keeps one color in every chapter.
    QStringList povNames = povTotals.keys();
    std::ranges::sort(povNames, [&povTotals](const QString &a, const QString &b) {
        return povTotals.value(a) != povTotals.value(b) ? povTotals.value(a) > povTotals.value(b)
                                                        : a < b;
    });
    // The most used words, most first, then in word order.
    QList<std::pair<QString, int>> ranked;
    for (auto it = frequency.cbegin(); it != frequency.cend(); ++it)
        ranked.append({ it.key(), it.value() });
    std::ranges::sort(ranked, [](const auto &a, const auto &b) {
        return a.second != b.second ? a.second > b.second : a.first < b.first;
    });
    QVariantList common;
    for (qsizetype i = 0; i < std::min<qsizetype>(30, ranked.size()); ++i)
        common.append(QVariantMap { { QStringLiteral("word"), ranked.at(i).first },
                                    { QStringLiteral("count"), ranked.at(i).second } });
    QVariantList people;
    for (const Character &character : std::as_const(characters))
        people.append(QVariantMap { { QStringLiteral("id"), character.id },
                                    { QStringLiteral("title"), character.title } });
    return { { QStringLiteral("chapters"), chapters },
             { QStringLiteral("povNames"), povNames },
             { QStringLiteral("words"), common },
             { QStringLiteral("characters"), people } };
}

QVariantList ProjectSession::relations(const QString &id) const
{
    QVariantList list;
    const toml::array *relations = project.binderData.get_as<toml::array>("relation");
    if (relations == nullptr)
        return list;
    for (std::size_t i = 0; i < relations->size(); ++i) {
        const toml::table *relation = (*relations)[i].as_table();
        if (relation == nullptr)
            continue;
        const QString from = QString::fromStdString((*relation)["from"].value_or(std::string()));
        const QString to = QString::fromStdString((*relation)["to"].value_or(std::string()));
        const int a = indexOf(from);
        const int b = indexOf(to);
        // A link to an item that is gone or in the trash does not show.
        if (a < 0 || b < 0 || binderModel.isInTrash(from) || binderModel.isInTrash(to))
            continue;
        if (!id.isEmpty() && from != id && to != id)
            continue;
        list.append(
            QVariantMap { { QStringLiteral("index"), static_cast<int>(i) },
                          { QStringLiteral("from"), from },
                          { QStringLiteral("to"), to },
                          { QStringLiteral("fromTitle"), project.items.at(a).title },
                          { QStringLiteral("toTitle"), project.items.at(b).title },
                          { QStringLiteral("name"),
                            QString::fromStdString((*relation)["name"].value_or(std::string())) },
                          { QStringLiteral("parent"), (*relation)["parent"].value_or(false) } });
    }
    return list;
}

bool ProjectSession::addRelation(const QString &from, const QString &to, const QString &name,
                                 bool parent)
{
    const int a = indexOf(from);
    const int b = indexOf(to);
    const QString clean = parent ? QStringLiteral("parent of") : name.trimmed();
    if (project.readOnly || a < 0 || b < 0 || from == to || clean.isEmpty()
        || !isEntity(project.items.at(a).kind) || !isEntity(project.items.at(b).kind))
        return false;
    if (project.binderData.get_as<toml::array>("relation") == nullptr)
        project.binderData.insert_or_assign("relation", toml::array { });
    toml::table relation { { "from", from.toStdString() },
                           { "to", to.toStdString() },
                           { "name", clean.toStdString() } };
    // The parent key is written only when true.
    if (parent)
        relation.insert("parent", true);
    project.binderData.get_as<toml::array>("relation")->push_back(std::move(relation));
    return commit({ });
}

QVariantMap ProjectSession::familyTree() const
{
    // The parent links, and every person in one.
    QList<std::pair<QString, QString>> links;
    QStringList people;
    for (const QVariant &each : relations()) {
        const QVariantMap relation = each.toMap();
        if (!relation.value(QStringLiteral("parent")).toBool())
            continue;
        const QString parent = relation.value(QStringLiteral("from")).toString();
        const QString child = relation.value(QStringLiteral("to")).toString();
        links.append({ parent, child });
        for (const QString &id : { parent, child }) {
            if (!people.contains(id))
                people.append(id);
        }
    }
    // A generation is one more than the deepest parent. The passes stop
    // after one pass for each person, so a loop of links cannot hang.
    QHash<QString, int> generation;
    for (const QString &id : std::as_const(people))
        generation.insert(id, 0);
    for (qsizetype pass = 0; pass < people.size(); ++pass) {
        bool changed = false;
        for (const auto &[parent, child] : std::as_const(links)) {
            if (generation.value(child) < generation.value(parent) + 1
                && generation.value(parent) < people.size()) {
                generation[child] = generation.value(parent) + 1;
                changed = true;
            }
        }
        if (!changed)
            break;
    }
    QList<QVariantList> rows;
    for (const QString &id : std::as_const(people)) {
        const int row = generation.value(id);
        while (rows.size() <= row)
            rows.append(QVariantList());
        rows[row].append(
            QVariantMap { { QStringLiteral("id"), id },
                          { QStringLiteral("title"), project.items.at(indexOf(id)).title } });
    }
    QVariantList generations;
    for (QVariantList &row : rows) {
        std::ranges::sort(row, [](const QVariant &a, const QVariant &b) {
            return a.toMap()
                       .value(QStringLiteral("title"))
                       .toString()
                       .localeAwareCompare(b.toMap().value(QStringLiteral("title")).toString())
                < 0;
        });
        generations.append(QVariant(row));
    }
    QVariantList edges;
    for (const auto &[parent, child] : std::as_const(links))
        edges.append(
            QVariantMap { { QStringLiteral("from"), parent }, { QStringLiteral("to"), child } });
    return { { QStringLiteral("generations"), generations }, { QStringLiteral("links"), edges } };
}

bool ProjectSession::removeRelation(int index)
{
    toml::array *relations = project.binderData.get_as<toml::array>("relation");
    if (project.readOnly || relations == nullptr || index < 0
        || index >= static_cast<int>(relations->size()))
        return false;
    relations->erase(relations->begin() + index);
    if (relations->empty())
        project.binderData.erase("relation");
    return commit({ });
}

QVariantList ProjectSession::plotGrid() const
{
    QVariantList scenes;
    const std::function<void(int)> walk = [&](int at) {
        const ProjectItem &item = project.items.at(at);
        if (item.kind == QLatin1String("scene"))
            scenes.append(QVariantMap {
                { QStringLiteral("id"), item.id },
                { QStringLiteral("title"), item.title },
                { QStringLiteral("plotlines"), strings(item.data.get("plotlines")) } });
        for (const int child : item.children)
            walk(child);
    };
    for (const int root : project.roots) {
        if (project.items.at(root).section == QLatin1String("manuscript"))
            walk(root);
    }
    return scenes;
}

QString ProjectSession::addPlotline(const QString &name)
{
    const QString clean = name.trimmed();
    if (project.readOnly || clean.isEmpty())
        return { };
    // The id comes from the name, with a number if the id is taken.
    QString base = clean.toLower();
    base.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("-"));
    base.remove(QRegularExpression(QStringLiteral("^-+|-+$")));
    if (base.isEmpty())
        base = QStringLiteral("plotline");
    QStringList taken;
    for (const QVariant &line : plotlines())
        taken.append(line.toMap().value(QStringLiteral("id")).toString());
    QString id = base;
    for (int n = 2; taken.contains(id); ++n)
        id = base + u'-' + QString::number(n);
    if (project.data.get_as<toml::array>("plotline") == nullptr)
        project.data.insert_or_assign("plotline", toml::array { });
    project.data.get_as<toml::array>("plotline")
        ->push_back(toml::table { { "id", id.toStdString() }, { "name", clean.toStdString() } });
    return commit({ }) ? id : QString();
}

QVariantList ProjectSession::timeline(const QString &groupBy) const
{
    const Calendar dates = calendar();
    QMap<QString, QVariantList> tracks;
    QHash<QString, QString> lineNames;
    for (const QVariant &line : plotlines())
        lineNames.insert(line.toMap().value(QStringLiteral("id")).toString(),
                         line.toMap().value(QStringLiteral("name")).toString());
    // The items with a valid date, outside the trash, by day.
    QList<std::pair<qint64, int>> dated;
    for (int i = 0; i < project.items.size(); ++i) {
        const ProjectItem &item = project.items.at(i);
        const auto day
            = dates.dayOf(QString::fromStdString(item.data["date"].value_or(std::string())));
        if (day && !binderModel.isInTrash(item.id))
            dated.append({ *day, i });
    }
    std::ranges::stable_sort(dated, { }, &std::pair<qint64, int>::first);
    for (const auto &[day, index] : std::as_const(dated)) {
        const ProjectItem &item = project.items.at(index);
        const QString date = QString::fromStdString(item.data["date"].value_or(std::string()));
        const QVariantMap event { { QStringLiteral("id"), item.id },
                                  { QStringLiteral("title"), item.title },
                                  { QStringLiteral("kind"), item.kind },
                                  { QStringLiteral("day"), day },
                                  { QStringLiteral("label"), dates.label(date) } };
        QStringList names;
        if (groupBy == QLatin1String("plotline")) {
            for (const QString &id : strings(item.data.get("plotlines")))
                names.append(lineNames.value(id, id));
            if (names.isEmpty())
                names.append(isEntity(item.kind) ? QStringLiteral("Codex")
                                                 : QStringLiteral("No plotline"));
        } else {
            const QString pov = QString::fromStdString(item.data["pov"].value_or(std::string()));
            names.append(!pov.isEmpty()            ? pov
                             : isEntity(item.kind) ? QStringLiteral("Codex")
                                                   : QStringLiteral("No POV"));
        }
        for (const QString &name : std::as_const(names))
            tracks[name].append(event);
    }
    QVariantList list;
    for (auto it = tracks.cbegin(); it != tracks.cend(); ++it)
        list.append(QVariantMap { { QStringLiteral("name"), it.key() },
                                  { QStringLiteral("events"), it.value() } });
    return list;
}

Calendar ProjectSession::calendar() const
{
    QList<Calendar::Month> months;
    if (const toml::array *list = project.data["calendar"]["month"].as_array()) {
        for (const toml::node &node : *list) {
            const toml::table *month = node.as_table();
            if (month == nullptr)
                continue;
            months.append({ QString::fromStdString((*month)["name"].value_or(std::string())),
                            static_cast<int>((*month)["days"].value_or(std::int64_t { 30 })) });
        }
    }
    return Calendar(months);
}

QVariantList ProjectSession::calendarMonths() const
{
    QVariantList list;
    // Keep the calendar alive while the loop reads its months.
    const Calendar own = calendar();
    for (const Calendar::Month &month : own.months())
        list.append(QVariantMap { { QStringLiteral("name"), month.name },
                                  { QStringLiteral("days"), month.days } });
    return list;
}

bool ProjectSession::setCalendar(const QVariantList &months)
{
    if (project.readOnly)
        return false;
    toml::array list;
    for (const QVariant &each : months) {
        const QVariantMap month = each.toMap();
        const QString name = month.value(QStringLiteral("name")).toString().trimmed();
        const int days = month.value(QStringLiteral("days")).toInt();
        if (name.isEmpty() || days < 1 || days > 1000)
            return false;
        list.push_back(toml::table { { "name", name.toStdString() }, { "days", days } });
    }
    // No months is the Gregorian calendar.
    if (list.empty()) {
        project.data.erase("calendar");
    } else {
        if (project.data.get_as<toml::table>("calendar") == nullptr)
            project.data.insert_or_assign("calendar", toml::table { });
        project.data.get_as<toml::table>("calendar")->insert_or_assign("month", std::move(list));
    }
    return commit({ });
}

bool ProjectSession::numbered() const
{
    return project.data["numbered"].value_or(false);
}

bool ProjectSession::setNumbered(bool on)
{
    if (project.readOnly)
        return false;
    // The default is off, so only true is written.
    if (on)
        project.data.insert_or_assign("numbered", true);
    else
        project.data.erase("numbered");
    return commit({ });
}

QVariantMap ProjectSession::numbers() const
{
    QVariantMap map;
    int chapter = 0;
    for (const ChapterGroup &group : chapterGroups()) {
        const bool loose = indexOf(group.id) < 0;
        if (!loose)
            map.insert(group.id, QString::number(++chapter));
        for (qsizetype i = 0; i < group.scenes.size(); ++i) {
            const QString scene = QString::number(i + 1);
            map.insert(project.items.at(group.scenes.at(i)).id,
                       loose ? scene : QString::number(chapter) + u'.' + scene);
        }
    }
    return map;
}

QString ProjectSession::sceneBreak() const
{
    return QString::fromStdString(project.data["scene-break"].value_or(std::string("***")));
}

bool ProjectSession::setSceneBreak(const QString &mark)
{
    if (project.readOnly
        || (mark != QLatin1String("***") && mark != QLatin1String("#") && !mark.isEmpty()))
        return false;
    project.data.insert_or_assign("scene-break", mark.toStdString());
    return commit({ });
}

QString ProjectSession::framework() const
{
    return QString::fromStdString(project.data["framework"].value_or(std::string()));
}

bool ProjectSession::setFramework(const QString &id)
{
    if (project.readOnly || std::ranges::none_of(Frameworks::all(), [&id](const auto &each) {
            return each.id == id;
        }))
        return false;
    project.data.insert_or_assign("framework", id.toStdString());
    return commit({ });
}

QVariantList ProjectSession::beatBoard() const
{
    QVariantList columns;
    const auto found = std::ranges::find_if(
        Frameworks::all(), [this](const auto &each) { return each.id == framework(); });
    if (found == Frameworks::all().end())
        return columns;
    // The scenes of the manuscript, in binder order, by beat.
    QHash<QString, QVariantList> byBeat;
    const std::function<void(int)> walk = [&](int at) {
        const ProjectItem &item = project.items.at(at);
        if (item.kind == QLatin1String("scene"))
            byBeat[QString::fromStdString(item.data["beat"].value_or(std::string()))].append(
                QVariantMap { { QStringLiteral("id"), item.id },
                              { QStringLiteral("title"), item.title } });
        for (const int child : item.children)
            walk(child);
    };
    for (const int root : project.roots) {
        if (project.items.at(root).section == QLatin1String("manuscript"))
            walk(root);
    }
    QSet<QString> known;
    for (const Frameworks::Beat &beat : found->beats) {
        known.insert(beat.id);
        columns.append(QVariantMap { { QStringLiteral("id"), beat.id },
                                     { QStringLiteral("name"), beat.name },
                                     { QStringLiteral("description"), beat.description },
                                     { QStringLiteral("scenes"), byBeat.value(beat.id) } });
    }
    // A scene with no beat, or with a beat of another framework.
    QVariantList loose;
    for (auto it = byBeat.cbegin(); it != byBeat.cend(); ++it) {
        if (!known.contains(it.key()))
            loose += it.value();
    }
    columns.prepend(
        QVariantMap { { QStringLiteral("id"), QString() },
                      { QStringLiteral("name"), QStringLiteral("No beat") },
                      { QStringLiteral("description"),
                        QStringLiteral("Scenes that are on no beat of this framework.") },
                      { QStringLiteral("scenes"), loose } });
    return columns;
}

QVariantList ProjectSession::frameworks() const
{
    QVariantList list;
    for (const Frameworks::Framework &framework : Frameworks::all()) {
        QVariantList beats;
        for (const Frameworks::Beat &beat : framework.beats)
            beats.append(QVariantMap { { QStringLiteral("id"), beat.id },
                                       { QStringLiteral("name"), beat.name },
                                       { QStringLiteral("description"), beat.description },
                                       { QStringLiteral("at"), beat.at } });
        list.append(QVariantMap { { QStringLiteral("id"), framework.id },
                                  { QStringLiteral("name"), framework.name },
                                  { QStringLiteral("beats"), beats } });
    }
    return list;
}

QVariantList ProjectSession::entityTypes() const
{
    QVariantList list;
    const toml::array *tables = project.data.get_as<toml::array>("entity-type");
    const auto tableOf = [tables](const QString &kind) -> const toml::table * {
        if (tables == nullptr)
            return nullptr;
        for (const toml::node &node : *tables) {
            const toml::table *table = node.as_table();
            if (table != nullptr
                && QString::fromStdString((*table)["kind"].value_or(std::string())) == kind)
                return table;
        }
        return nullptr;
    };
    const auto append = [&list, &tableOf](const QString &kind, const QString &name, bool custom) {
        const toml::table *table = tableOf(kind);
        const QString own = table != nullptr
            ? QString::fromStdString((*table)["name"].value_or(std::string()))
            : QString();
        list.append(QVariantMap {
            { QStringLiteral("kind"), kind },
            { QStringLiteral("name"), own.isEmpty() ? name : own },
            { QStringLiteral("fields"),
              table != nullptr ? strings(table->get("fields")) : QStringList() },
            { QStringLiteral("custom"), custom },
        });
    };
    for (const auto &[kind, name] : builtInTypes)
        append(kind.toString(), name.toString(), false);
    if (tables != nullptr) {
        for (const toml::node &node : *tables) {
            const toml::table *table = node.as_table();
            const QString kind = table != nullptr
                ? QString::fromStdString((*table)["kind"].value_or(std::string()))
                : QString();
            if (!kind.isEmpty() && std::ranges::none_of(builtInTypes, [&kind](const auto &type) {
                    return type.first == kind;
                }))
                append(kind, kind, true);
        }
    }
    return list;
}

QString ProjectSession::saveEntityType(const QString &kind, const QString &name,
                                       const QStringList &fields)
{
    editError.clear();
    if (project.readOnly || !loadError.isEmpty())
        return { };
    QString type = kind;
    if (type.isEmpty()) {
        // A new type takes its kind from its name: "Ships" becomes "ships".
        type = name.trimmed().toLower();
        type.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("-"));
        type.remove(QRegularExpression(QStringLiteral("^[-0-9]+|-+$")));
        for (const QVariant &each : entityTypes()) {
            if (each.toMap().value(QStringLiteral("kind")).toString() == type)
                type.clear();
        }
        if (type.isEmpty() || !isEntity(type)) {
            editError = QStringLiteral("Choose a name that no other type has.");
            emit edited();
            return { };
        }
    }
    QStringList clean;
    for (const QString &field : fields) {
        if (!field.trimmed().isEmpty() && !clean.contains(field.trimmed()))
            clean.append(field.trimmed());
    }
    if (typeTables(project) == nullptr)
        project.data.insert_or_assign("entity-type", toml::array { });
    toml::array &tables = *typeTables(project);
    toml::table *table = nullptr;
    for (toml::node &node : tables) {
        if (toml::table *each = node.as_table(); each != nullptr
            && QString::fromStdString((*each)["kind"].value_or(std::string())) == type)
            table = each;
    }
    if (table == nullptr) {
        tables.push_back(toml::table { { "kind", type.toStdString() } });
        table = tables.back().as_table();
    }
    const bool builtIn = std::ranges::any_of(
        builtInTypes, [&type](const auto &each) { return each.first == type; });
    if (!builtIn && !name.trimmed().isEmpty())
        table->insert_or_assign("name", name.trimmed().toStdString());
    toml::array list;
    for (const QString &field : std::as_const(clean))
        list.push_back(field.toStdString());
    if (clean.isEmpty())
        table->erase("fields");
    else
        table->insert_or_assign("fields", std::move(list));
    // A built-in type with no fields left needs no table.
    if (builtIn && clean.isEmpty())
        tables.erase(std::ranges::find_if(
            tables, [table](const toml::node &node) { return node.as_table() == table; }));
    if (tables.empty())
        project.data.erase("entity-type");
    commit({ });
    return type;
}

QString ProjectSession::entityByName(const QString &name) const
{
    const QString wanted = name.trimmed();
    if (wanted.isEmpty())
        return { };
    for (const ProjectItem &item : project.items) {
        if (!isEntity(item.kind) || item.section == QLatin1String("trash"))
            continue;
        QStringList names = strings(item.data.get("aliases"));
        names.prepend(item.title);
        if (names.contains(wanted, Qt::CaseInsensitive))
            return item.id;
    }
    return { };
}

QString ProjectSession::entityAt(const QString &text, int position) const
{
    static const QRegularExpression link(QStringLiteral("\\[\\[([^\\[\\]\\n]+)\\]\\]"));
    QRegularExpressionMatchIterator it = link.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        if (position >= match.capturedStart() && position <= match.capturedEnd())
            return entityByName(match.captured(1));
    }
    // A plain name of the codex, with no brackets.
    if (knownNames.isEmpty())
        return { };
    it = namePattern(knownNames).globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        if (position >= match.capturedStart() && position < match.capturedEnd())
            return entityByName(match.captured(1));
    }
    return { };
}

QStringList ProjectSession::entityNames() const
{
    QStringList names;
    for (const ProjectItem &item : project.items) {
        if (!isEntity(item.kind) || item.section == QLatin1String("trash"))
            continue;
        names.append(item.title.trimmed());
        for (const QString &alias : strings(item.data.get("aliases")))
            names.append(alias.trimmed());
    }
    names.removeAll(QString());
    names.removeDuplicates();
    return names;
}

QRegularExpression ProjectSession::namePattern(const QStringList &names)
{
    QStringList escaped;
    for (const QString &name : names)
        escaped.append(QRegularExpression::escape(name));
    // The longest name first, so "Mara Vell" wins over "Mara".
    std::ranges::sort(escaped,
                      [](const QString &a, const QString &b) { return a.size() > b.size(); });
    return QRegularExpression(
        QStringLiteral("(?<!\\w)(") + escaped.join(u'|') + QStringLiteral(")(?!\\w)"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
}

QString ProjectSession::summaryOf(const QString &id) const
{
    const int index = indexOf(id);
    if (index < 0 || project.items.at(index).file.isEmpty())
        return { };
    const QString paragraph
        = textOf(index).trimmed().section(QStringLiteral("\n\n"), 0, 0).simplified();
    return paragraph.size() > 200 ? paragraph.left(199).trimmed() + u'\u2026' : paragraph;
}

QVariantList ProjectSession::mentionsOf(const QString &id)
{
    QVariantList list;
    const int index = indexOf(id);
    if (index < 0 || !isEntity(project.items.at(index).kind))
        return list;
    QStringList names = strings(project.items.at(index).data.get("aliases"));
    names.prepend(project.items.at(index).title);
    QStringList escaped;
    for (const QString &name : std::as_const(names)) {
        if (!name.trimmed().isEmpty())
            escaped.append(QRegularExpression::escape(name.trimmed()));
    }
    // The longest name first, so "Mara Vell" wins over "Mara".
    std::ranges::sort(escaped,
                      [](const QString &a, const QString &b) { return a.size() > b.size(); });
    const QRegularExpression pattern(
        QStringLiteral("(?<!\\w)(") + escaped.join(u'|') + QStringLiteral(")(?!\\w)"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
    // The texts on disk must hold the latest typing.
    saveAll();
    // ponytail: reads every manuscript file at each call; use the search
    // index if a very long book makes this slow.
    for (const int root : std::as_const(project.roots)) {
        if (project.items.at(root).section != QLatin1String("manuscript"))
            continue;
        const std::function<void(int)> visit = [&](int at) {
            const ProjectItem &item = project.items.at(at);
            if (!item.file.isEmpty()) {
                int count = 0;
                QFile file(QDir(project.folder).filePath(item.file));
                const QString text = file.open(QIODevice::ReadOnly)
                    ? QString::fromUtf8(file.readAll())
                    : QString();
                QRegularExpressionMatchIterator it = pattern.globalMatch(text);
                while (it.hasNext()) {
                    it.next();
                    ++count;
                }
                if (count > 0)
                    list.append(QVariantMap { { QStringLiteral("id"), item.id },
                                              { QStringLiteral("title"), item.title },
                                              { QStringLiteral("count"), count } });
            }
            for (const int child : item.children)
                visit(child);
        };
        visit(root);
    }
    return list;
}

bool ProjectSession::setEntityField(const QString &id, const QString &field, const QString &value)
{
    const int index = indexOf(id);
    if (index < 0 || project.readOnly || field.trimmed().isEmpty())
        return false;
    toml::table &data = project.items[index].data;
    if (data.get_as<toml::table>("fields") == nullptr)
        data.insert_or_assign("fields", toml::table { });
    toml::table &fields = *data.get_as<toml::table>("fields");
    const QString text = value.trimmed();
    if (text.isEmpty())
        fields.erase(field.trimmed().toStdString());
    else
        fields.insert_or_assign(field.trimmed().toStdString(), text.toStdString());
    if (fields.empty())
        data.erase("fields");
    return commit({ });
}

QString ProjectSession::add(const QString &kind)
{
    editError.clear();
    // An entity needs a type of the project.
    if (isEntity(kind) && std::ranges::none_of(entityTypes(), [&kind](const QVariant &type) {
            return type.toMap().value(QStringLiteral("kind")).toString() == kind;
        }))
        return { };
    if (project.readOnly || !loadError.isEmpty()) {
        editError = QStringLiteral("This project cannot change.");
        emit edited();
        return { };
    }
    // Into the selected item if it can hold the kind, else after it.
    int parent = -1;
    int position = static_cast<int>(project.roots.size());
    QString section = homeSection(kind);
    const int selected = indexOf(current);
    if (selected >= 0 && project.items.at(selected).section != QLatin1String("trash")) {
        const ProjectItem &item = project.items.at(selected);
        if (canHold(item.kind, kind)) {
            parent = selected;
            position = static_cast<int>(item.children.size());
        } else if (item.parent >= 0 && canHold(project.items.at(item.parent).kind, kind)) {
            parent = item.parent;
            position = static_cast<int>(project.items.at(parent).children.indexOf(selected)) + 1;
        } else if (item.parent < 0 && fitsSection(kind, item.section)) {
            section = item.section;
            position = static_cast<int>(project.roots.indexOf(selected)) + 1;
        }
    }

    QSet<QString> ids;
    for (const ProjectItem &item : std::as_const(project.items))
        ids.insert(item.id);
    ProjectItem item;
    item.id = Naming::newItemId(ids);
    item.kind = kind;
    item.title = newTitle(kind);
    QStringList newFiles;
    const QString folder = folderOf(kind);
    if (!folder.isEmpty()) {
        const QDir dir(project.folder);
        dir.mkpath(folder);
        const QStringList names = QDir(dir.filePath(folder)).entryList(QDir::Files);
        item.file
            = folder + Naming::fileNameFor(item.title, QSet<QString>(names.begin(), names.end()));
        QSaveFile file(dir.filePath(item.file));
        if (!file.open(QIODevice::WriteOnly) || !file.commit()) {
            editError = QStringLiteral("Cannot make %1.").arg(item.file);
            emit edited();
            return { };
        }
        newFiles.append(item.file);
    }
    project.items.append(item);
    place(static_cast<int>(project.items.size() - 1), parent, position, section);
    commit(newFiles);
    setCurrentId(item.id);
    return item.id;
}

QString ProjectSession::duplicate(const QString &id)
{
    const int source = indexOf(id);
    if (source < 0 || project.readOnly || !loadError.isEmpty()
        || project.items.at(source).section == QLatin1String("trash"))
        return { };
    // The copies read the files, so the open text saves first.
    text.save();
    QSet<QString> ids;
    for (const ProjectItem &item : std::as_const(project.items))
        ids.insert(item.id);
    const QDir dir(project.folder);
    QStringList newFiles;
    bool failed = false;
    const std::function<int(int, int, int, const QString &, const QString &)> copy =
        [&](int from, int parent, int position, const QString &section, const QString &title) {
            // A copy, not a reference: the append below can move the items.
            ProjectItem item = project.items.at(from);
            const QList<int> children = item.children;
            item.id = Naming::newItemId(ids);
            ids.insert(item.id);
            item.title = title;
            item.children.clear();
            if (!item.file.isEmpty()) {
                const QString folder = item.file.section(u'/', 0, -2);
                const QString prefix = folder.isEmpty() ? QString() : folder + u'/';
                const QStringList names = QDir(dir.filePath(folder)).entryList(QDir::Files);
                const QString file = prefix
                    + Naming::fileNameFor(title, QSet<QString>(names.begin(), names.end()));
                if (!item.missingFile && !QFile::copy(dir.filePath(item.file), dir.filePath(file)))
                    failed = true;
                item.file = file;
                newFiles.append(file);
            }
            project.items.append(item);
            const int index = static_cast<int>(project.items.size() - 1);
            place(index, parent, position, section);
            for (qsizetype i = 0; i < children.size(); ++i)
                copy(children.at(i), index, static_cast<int>(i), section,
                     project.items.at(children.at(i)).title);
            return index;
        };
    const ProjectItem &original = project.items.at(source);
    const int parent = original.parent;
    const int position = static_cast<int>(
        (parent >= 0 ? project.items.at(parent).children : project.roots).indexOf(source) + 1);
    const int top = copy(source, parent, position, original.section,
                         original.title + QStringLiteral(" (copy)"));
    if (failed) {
        editError = QStringLiteral("Cannot copy all the texts.");
        emit edited();
    }
    commit(newFiles);
    const QString copyId = project.items.at(top).id;
    setCurrentId(copyId);
    return copyId;
}

QString ProjectSession::saveSearch(const QString &query, bool regex)
{
    if (project.readOnly || !loadError.isEmpty() || query.trimmed().isEmpty())
        return { };
    QSet<QString> ids;
    for (const ProjectItem &each : std::as_const(project.items))
        ids.insert(each.id);
    ProjectItem item;
    item.id = Naming::newItemId(ids);
    item.kind = QStringLiteral("search");
    item.title = query.trimmed();
    item.data.insert_or_assign("query", query.toStdString());
    // The key is written only when it is true, as include only when false.
    if (regex)
        item.data.insert_or_assign("regex", true);
    project.items.append(item);
    place(static_cast<int>(project.items.size() - 1), -1, static_cast<int>(project.roots.size()),
          QStringLiteral("searches"));
    if (!commit({ }))
        return { };
    return item.id;
}

bool ProjectSession::rename(const QString &id, const QString &title)
{
    const int index = indexOf(id);
    const QString name = title.trimmed();
    if (index < 0 || name.isEmpty() || project.readOnly)
        return false;
    if (project.items.at(index).title == name)
        return true;
    project.items[index].title = name;
    return commit({ });
}

int ProjectSession::keyboardModifiers()
{
    return static_cast<int>(QGuiApplication::keyboardModifiers().toInt());
}

bool ProjectSession::moveToTrash(const QString &id)
{
    const int index = indexOf(id);
    if (index < 0 || project.readOnly || project.items.at(index).section == QLatin1String("trash"))
        return false;
    text.save();
    ProjectItem &item = project.items[index];
    if (item.parent >= 0)
        item.data.insert_or_assign("restore", project.items.at(item.parent).id.toStdString());
    else
        item.data.erase("restore");
    detach(index);
    place(index, -1, static_cast<int>(project.roots.size()), QStringLiteral("trash"));
    // The selection clears if the selected item went to the trash with the
    // branch.
    const int selected = indexOf(current);
    if (selected >= 0 && project.items.at(selected).section == QLatin1String("trash"))
        setCurrentId(QString());
    return commit({ });
}

bool ProjectSession::restore(const QString &id)
{
    const int index = indexOf(id);
    if (index < 0 || project.readOnly || project.items.at(index).section != QLatin1String("trash"))
        return false;
    ProjectItem &item = project.items[index];
    const int parent
        = indexOf(QString::fromStdString(item.data["restore"].value_or(std::string())));
    item.data.erase("restore");
    detach(index);
    if (parent >= 0 && project.items.at(parent).section != QLatin1String("trash"))
        place(index, parent, static_cast<int>(project.items.at(parent).children.size()), { });
    else
        place(index, -1, static_cast<int>(project.roots.size()), homeSection(item.kind));
    return commit({ });
}

bool ProjectSession::resolveMove(int index, int target, const QString &position, int *parent,
                                 int *at, QString *section) const
{
    if (index < 0 || target < 0 || index == target || project.readOnly)
        return false;
    const QString trash = QStringLiteral("trash");
    if (project.items.at(index).section == trash || project.items.at(target).section == trash)
        return false;
    // An item cannot go into its own branch.
    for (int up = target; up >= 0; up = project.items.at(up).parent) {
        if (up == index)
            return false;
    }
    const QString &kind = project.items.at(index).kind;
    const ProjectItem &other = project.items.at(target);
    if (position == QLatin1String("inside")) {
        if (!canHold(other.kind, kind))
            return false;
        *parent = target;
        *at = static_cast<int>(other.children.size());
        *section = other.section;
        return true;
    }
    if (position != QLatin1String("before") && position != QLatin1String("after"))
        return false;
    *parent = other.parent;
    *section = other.section;
    if (*parent >= 0 ? !canHold(project.items.at(*parent).kind, kind)
                     : !fitsSection(kind, *section))
        return false;
    const QList<int> &siblings = *parent >= 0 ? project.items.at(*parent).children : project.roots;
    *at = static_cast<int>(siblings.indexOf(target)) + (position == QLatin1String("after") ? 1 : 0);
    // The item leaves its old place first, so a later place moves up by one.
    const qsizetype old = siblings.indexOf(index);
    if (old >= 0 && old < *at)
        --*at;
    return true;
}

bool ProjectSession::canMove(const QString &id, const QString &target,
                             const QString &position) const
{
    int parent = -1;
    int at = 0;
    QString section;
    return resolveMove(indexOf(id), indexOf(target), position, &parent, &at, &section);
}

bool ProjectSession::move(const QString &id, const QString &target, const QString &position)
{
    const int index = indexOf(id);
    int parent = -1;
    int at = 0;
    QString section;
    if (!resolveMove(index, indexOf(target), position, &parent, &at, &section))
        return false;
    detach(index);
    place(index, parent, at, section);
    return commit({ });
}

bool ProjectSession::moveBy(const QString &id, int delta)
{
    const int index = indexOf(id);
    if (index < 0 || (delta != -1 && delta != 1))
        return false;
    const ProjectItem &item = project.items.at(index);
    const QList<int> &siblings
        = item.parent >= 0 ? project.items.at(item.parent).children : project.roots;
    // Among the top level items, only the items of the same section count.
    qsizetype at = siblings.indexOf(index) + delta;
    while (at >= 0 && at < siblings.size()
           && project.items.at(siblings.at(at)).section != item.section)
        at += delta;
    if (at < 0 || at >= siblings.size())
        return false;
    return move(id, project.items.at(siblings.at(at)).id,
                delta < 0 ? QStringLiteral("before") : QStringLiteral("after"));
}

int ProjectSession::chapterWords() const
{
    // A selected chapter counts itself.
    const int index = indexOf(current);
    if (index >= 0 && project.items.at(index).kind == QLatin1String("chapter"))
        return binderModel.wordsOf(current);
    const QString chapter = binderModel.chapterOf(current);
    return chapter.isEmpty() ? -1 : binderModel.wordsOf(chapter);
}

void ProjectSession::setSpellCheck(bool on)
{
    if (checkSpelling == on)
        return;
    checkSpelling = on;
    applySpelling();
    emit spellChanged();
}

QStringList ProjectSession::dictionaries()
{
    return SpellChecker::installed();
}

bool ProjectSession::setLanguage(const QString &name)
{
    if (project.readOnly || !SpellChecker::installed().contains(name))
        return false;
    QString tag = name;
    tag.replace(u'_', u'-');
    project.data.insert_or_assign("language", tag.toStdString());
    if (!commit({ }))
        return false;
    spell = std::make_unique<SpellChecker>(name);
    thesaurus = std::make_unique<Thesaurus>(name);
    teachSpelling();
    applySpelling();
    emit spellChanged();
    return true;
}

QStringList ProjectSession::synonyms(const QString &word) const
{
    return thesaurus ? thesaurus->synonyms(word).mid(0, 12) : QStringList();
}

QString ProjectSession::dictionary() const
{
    return spell ? spell->dictionary() : QString();
}

QStringList ProjectSession::suggestions(const QString &word) const
{
    return spell ? spell->suggestions(word) : QStringList();
}

bool ProjectSession::isWordCorrect(const QString &word) const
{
    return !spell || spell->isCorrect(word);
}

void ProjectSession::teachSpelling()
{
    if (!spell)
        return;
    QFile file(QDir(project.folder).filePath(QStringLiteral("dictionary.txt")));
    if (file.open(QIODevice::ReadOnly)) {
        for (const QString &line : QString::fromUtf8(file.readAll()).split(u'\n'))
            spell->addWord(line.trimmed());
    }
    // The names of the codex are words of the story.
    static const QRegularExpression notLetters(QStringLiteral("[^\\p{L}'’]+"),
                                               QRegularExpression::UseUnicodePropertiesOption);
    for (const ProjectItem &item : std::as_const(project.items)) {
        if (!isEntity(item.kind))
            continue;
        QStringList names = strings(item.data.get("aliases"));
        names.prepend(item.title);
        for (const QString &name : std::as_const(names)) {
            for (const QString &part : name.split(notLetters, Qt::SkipEmptyParts))
                spell->addWord(part);
        }
    }
}

bool ProjectSession::addToDictionary(const QString &word)
{
    const QString clean = word.trimmed();
    if (!spell || clean.isEmpty() || clean.contains(u'\n') || project.readOnly)
        return false;
    const QString path = QDir(project.folder).filePath(QStringLiteral("dictionary.txt"));
    QStringList words;
    QFile in(path);
    if (in.open(QIODevice::ReadOnly))
        words = QString::fromUtf8(in.readAll()).split(u'\n', Qt::SkipEmptyParts);
    in.close();
    if (!words.contains(clean)) {
        words.append(clean);
        words.sort(Qt::CaseInsensitive);
        QSaveFile out(path);
        const QByteArray bytes = (words.join(u'\n') + u'\n').toUtf8();
        if (!out.open(QIODevice::WriteOnly) || out.write(bytes) != bytes.size() || !out.commit())
            return false;
    }
    spell->addWord(clean);
    text.recheckSpelling();
    for (const QPointer<Document> &document : std::as_const(tracked)) {
        if (document)
            document->recheckSpelling();
    }
    return true;
}

void ProjectSession::setStyleChecks(int checks)
{
    if (checksOfStyle == checks)
        return;
    checksOfStyle = checks;
    applySpelling();
    emit spellChanged();
}

void ProjectSession::setHideMarks(bool hide)
{
    if (marksHidden == hide)
        return;
    marksHidden = hide;
    applySpelling();
    emit spellChanged();
}

void ProjectSession::applySpelling()
{
    const SpellChecker *checker = checkSpelling ? spell.get() : nullptr;
    text.setSpellChecker(checker);
    text.setStyleChecks(checksOfStyle);
    text.setHideMarks(marksHidden);
    text.setEntityNames(knownNames);
    for (const QPointer<Document> &document : std::as_const(tracked)) {
        if (document) {
            document->setSpellChecker(checker);
            document->setStyleChecks(checksOfStyle);
            document->setHideMarks(marksHidden);
            document->setEntityNames(knownNames);
        }
    }
}

bool ProjectSession::versionsOn() const
{
    return !project.folder.isEmpty() && loadError.isEmpty() && Versions(project.folder).isOn();
}

QString ProjectSession::turnOnVersions()
{
    if (project.folder.isEmpty() || !loadError.isEmpty() || project.readOnly)
        return QStringLiteral("This project cannot change.");
    // The version holds the latest typing.
    saveAll();
    const QString error
        = Versions(project.folder)
              .turnOn(QString::fromStdString(project.data["author"].value_or(std::string())));
    emit versionsChanged();
    return error;
}

void ProjectSession::setShowPasses(bool on)
{
    if (passTints == on)
        return;
    passTints = on;
    applyPasses();
    emit passesChanged();
}

QVariantList ProjectSession::passes() const
{
    QVariantList list;
    if (const toml::array *passes = project.data.get_as<toml::array>("pass")) {
        int number = 0;
        for (const toml::node &node : *passes) {
            if (const toml::table *pass = node.as_table())
                list.append(QVariantMap {
                    { QStringLiteral("number"), ++number },
                    { QStringLiteral("name"),
                      QString::fromStdString((*pass)["name"].value_or(std::string())) } });
        }
    }
    return list;
}

int ProjectSession::startPass(const QString &name)
{
    const QString clean = name.trimmed();
    if (project.readOnly || !loadError.isEmpty() || clean.isEmpty())
        return 0;
    saveAll();
    const int number = static_cast<int>(passes().size()) + 1;
    // A snapshot of each text marks where the pass starts.
    for (const ProjectItem &item : std::as_const(project.items)) {
        if (!item.file.isEmpty() && !holdsNoText(item.kind) && !item.missingFile
            && item.section != QLatin1String("trash"))
            Snapshots::take(project.folder, item.id, item.file,
                            QStringLiteral("Pass %1 start").arg(number));
    }
    if (project.data.get_as<toml::array>("pass") == nullptr)
        project.data.insert_or_assign("pass", toml::array { });
    project.data.get_as<toml::array>("pass")->push_back(
        toml::table { { "name", clean.toStdString() } });
    if (!commit({ }))
        return 0;
    emit snapshotsChanged();
    applyPasses();
    emit passesChanged();
    return number;
}

bool ProjectSession::setPassChecks(int number, const QStringList &checks)
{
    toml::array *passes = project.data.get_as<toml::array>("pass");
    if (project.readOnly || passes == nullptr || number < 1
        || number > static_cast<int>(passes->size()))
        return false;
    toml::table *pass = (*passes)[static_cast<std::size_t>(number - 1)].as_table();
    if (pass == nullptr)
        return false;
    toml::array list;
    QStringList seen;
    for (const QString &check : checks) {
        if (!check.trimmed().isEmpty() && !seen.contains(check.trimmed())) {
            seen.append(check.trimmed());
            list.push_back(check.trimmed().toStdString());
        }
    }
    if (list.empty())
        pass->erase("checks");
    else
        pass->insert_or_assign("checks", std::move(list));
    return commit({ });
}

QVariantMap ProjectSession::checklist(int number) const
{
    QStringList checks;
    if (const toml::array *passes = project.data.get_as<toml::array>("pass");
        passes != nullptr && number >= 1 && number <= static_cast<int>(passes->size())) {
        if (const toml::table *pass = (*passes)[static_cast<std::size_t>(number - 1)].as_table())
            checks = strings(pass->get("checks"));
    }

    QSet<QString> ticked;
    if (const toml::array *ticks = project.binderData.get_as<toml::array>("tick")) {
        for (const toml::node &node : *ticks) {
            const toml::table *tick = node.as_table();
            if (tick != nullptr && (*tick)["pass"].value_or(std::int64_t { 0 }) == number)
                ticked.insert(QString::fromStdString((*tick)["item"].value_or(std::string()))
                              + u'\n'
                              + QString::fromStdString((*tick)["check"].value_or(std::string())));
        }
    }
    QVariantList chapters;
    for (const ChapterGroup &group : chapterGroups()) {
        QVariantList marks;
        for (const QString &check : std::as_const(checks))
            marks.append(ticked.contains(group.id + u'\n' + check));
        chapters.append(QVariantMap { { QStringLiteral("id"), group.id },
                                      { QStringLiteral("title"), group.title },
                                      { QStringLiteral("ticked"), marks } });
    }
    return { { QStringLiteral("checks"), checks }, { QStringLiteral("chapters"), chapters } };
}

bool ProjectSession::setTick(int number, const QString &check, const QString &itemId, bool ticked)
{
    if (project.readOnly || number < 1 || check.isEmpty())
        return false;
    if (project.binderData.get_as<toml::array>("tick") == nullptr)
        project.binderData.insert_or_assign("tick", toml::array { });
    toml::array &ticks = *project.binderData.get_as<toml::array>("tick");
    const auto same = [&](const toml::node &node) {
        const toml::table *tick = node.as_table();
        return tick != nullptr && (*tick)["pass"].value_or(std::int64_t { 0 }) == number
            && QString::fromStdString((*tick)["check"].value_or(std::string())) == check
            && QString::fromStdString((*tick)["item"].value_or(std::string())) == itemId;
    };
    const auto found = std::ranges::find_if(ticks, same);
    if (ticked && found == ticks.end())
        ticks.push_back(toml::table { { "pass", number },
                                      { "check", check.toStdString() },
                                      { "item", itemId.toStdString() } });
    else if (!ticked && found != ticks.end())
        ticks.erase(found);
    if (ticks.empty())
        project.binderData.erase("tick");
    return commit({ });
}

QVariantList ProjectSession::passRanges(const QString &id) const
{
    QVariantList list;
    const int index = indexOf(id);
    const qsizetype count = passes().size();
    if (index < 0 || count == 0 || project.items.at(index).file.isEmpty())
        return list;
    // The text at the start of each pass, then the text now. A text that is
    // newer than a pass has no snapshot of it, and was empty then.
    QHash<QString, QString> starts;
    for (const Snapshots::Snapshot &snapshot : Snapshots::list(project.folder, id)) {
        if (!starts.contains(snapshot.title)) {
            bool ok = false;
            const QString body = readTextFile(QDir(project.folder).filePath(snapshot.path), &ok);
            if (ok)
                starts.insert(snapshot.title, body);
        }
    }
    QStringList versions;
    for (qsizetype n = 1; n <= count; ++n)
        versions.append(starts.value(QStringLiteral("Pass %1 start").arg(n)));
    bool ok = false;
    versions.append(
        current == id
            ? text.text()
            : readTextFile(QDir(project.folder).filePath(project.items.at(index).file), &ok));
    // Version k, from 1, holds what pass k wrote. Version 0 is older text.
    const QList<int> origin = Diff::origins(versions);
    for (qsizetype i = 0; i < origin.size();) {
        qsizetype end = i;
        while (end < origin.size() && origin.at(end) == origin.at(i))
            ++end;
        if (origin.at(i) > 0)
            list.append(QVariantMap { { QStringLiteral("start"), static_cast<int>(i) },
                                      { QStringLiteral("length"), static_cast<int>(end - i) },
                                      { QStringLiteral("pass"), origin.at(i) } });
        i = end;
    }
    return list;
}

QString ProjectSession::textOf(int index) const
{
    const ProjectItem &item = project.items.at(index);
    if (item.id == current && !text.file().isEmpty())
        return text.text();
    bool ok = false;
    return readTextFile(QDir(project.folder).filePath(item.file), &ok);
}

QVariantList ProjectSession::comments(const QString &id) const
{
    QVariantList list;
    const toml::array *comments = project.binderData.get_as<toml::array>("comment");
    if (comments == nullptr)
        return list;
    QHash<QString, QString> texts;
    for (std::size_t i = 0; i < comments->size(); ++i) {
        const toml::table *comment = (*comments)[i].as_table();
        if (comment == nullptr)
            continue;
        const QString item = QString::fromStdString((*comment)["item"].value_or(std::string()));
        const int index = indexOf(item);
        if (index < 0 || binderModel.isInTrash(item) || (!id.isEmpty() && item != id))
            continue;
        if (!texts.contains(item))
            texts.insert(item, textOf(index));
        // The quote nearest to its old place: the text around it can change.
        const QString quote = QString::fromStdString((*comment)["quote"].value_or(std::string()));
        const auto hint = static_cast<qsizetype>((*comment)["start"].value_or(std::int64_t { 0 }));
        qsizetype best = -1;
        const QString &body = texts[item];
        for (qsizetype at = body.indexOf(quote); at >= 0 && !quote.isEmpty();
             at = body.indexOf(quote, at + 1)) {
            if (best < 0 || std::abs(at - hint) < std::abs(best - hint))
                best = at;
        }
        list.append(
            QVariantMap { { QStringLiteral("index"), static_cast<int>(i) },
                          { QStringLiteral("item"), item },
                          { QStringLiteral("title"), project.items.at(index).title },
                          { QStringLiteral("quote"), quote },
                          { QStringLiteral("text"),
                            QString::fromStdString((*comment)["text"].value_or(std::string())) },
                          { QStringLiteral("resolved"), (*comment)["resolved"].value_or(false) },
                          { QStringLiteral("start"), static_cast<int>(best) } });
    }
    return list;
}

bool ProjectSession::addComment(const QString &id, int start, const QString &quote,
                                const QString &text)
{
    const int index = indexOf(id);
    if (project.readOnly || index < 0 || quote.trimmed().isEmpty() || text.trimmed().isEmpty()
        || start < 0)
        return false;
    if (project.binderData.get_as<toml::array>("comment") == nullptr)
        project.binderData.insert_or_assign("comment", toml::array { });
    project.binderData.get_as<toml::array>("comment")->push_back(
        toml::table { { "item", id.toStdString() },
                      { "quote", quote.toStdString() },
                      { "start", start },
                      { "text", text.trimmed().toStdString() } });
    const bool ok = commit({ });
    applyComments();
    return ok;
}

bool ProjectSession::resolveComment(int index, bool resolved)
{
    toml::array *comments = project.binderData.get_as<toml::array>("comment");
    if (project.readOnly || comments == nullptr || index < 0
        || index >= static_cast<int>(comments->size()))
        return false;
    toml::table *comment = (*comments)[static_cast<std::size_t>(index)].as_table();
    if (comment == nullptr)
        return false;
    // The key is written only when true.
    if (resolved)
        comment->insert_or_assign("resolved", true);
    else
        comment->erase("resolved");
    const bool ok = commit({ });
    applyComments();
    return ok;
}

bool ProjectSession::removeComment(int index)
{
    toml::array *comments = project.binderData.get_as<toml::array>("comment");
    if (project.readOnly || comments == nullptr || index < 0
        || index >= static_cast<int>(comments->size()))
        return false;
    comments->erase(comments->begin() + index);
    if (comments->empty())
        project.binderData.erase("comment");
    const bool ok = commit({ });
    applyComments();
    return ok;
}

void ProjectSession::applyComments()
{
    QList<std::pair<int, int>> ranges;
    if (!current.isEmpty()) {
        for (const QVariant &each : comments(current)) {
            const QVariantMap comment = each.toMap();
            const int start = comment.value(QStringLiteral("start")).toInt();
            if (start >= 0 && !comment.value(QStringLiteral("resolved")).toBool())
                ranges.append(
                    { start,
                      static_cast<int>(comment.value(QStringLiteral("quote")).toString().size()) });
        }
    }
    text.setComments(ranges);
}

void ProjectSession::applyPasses()
{
    QList<MarkdownHighlighter::PassRange> ranges;
    if (passTints) {
        for (const QVariant &each : passRanges(current)) {
            const QVariantMap range = each.toMap();
            ranges.append({ range.value(QStringLiteral("start")).toInt(),
                            range.value(QStringLiteral("length")).toInt(),
                            range.value(QStringLiteral("pass")).toInt() });
        }
    }
    text.setPasses(ranges);
}

QString ProjectSession::saveVersion(const QString &message)
{
    if (!versionsOn() || project.readOnly)
        return QStringLiteral("Turn on versions first.");
    saveAll();
    const QString text = message.trimmed().isEmpty()
        ? QStringLiteral("Version of %1")
              .arg(
                  QLocale(QLocale::English, QLocale::UnitedKingdom)
                      .toString(QDateTime::currentDateTime(), QStringLiteral("d MMMM yyyy, HH:mm")))
        : message.trimmed();
    const QString error
        = Versions(project.folder)
              .save(text, QString::fromStdString(project.data["author"].value_or(std::string())));
    emit versionsChanged();
    return error;
}

QVariantList ProjectSession::versions(const QString &id) const
{
    QVariantList list;
    QString path;
    if (!id.isEmpty()) {
        const int index = indexOf(id);
        if (index < 0 || project.items.at(index).file.isEmpty())
            return list;
        path = project.items.at(index).file;
    }
    const QLocale english(QLocale::English, QLocale::UnitedKingdom);
    for (const Versions::Entry &entry : Versions(project.folder).log(path)) {
        const QDateTime time = QDateTime::fromString(entry.time, Qt::ISODate).toLocalTime();
        list.append(QVariantMap { { QStringLiteral("hash"), entry.hash },
                                  { QStringLiteral("time"), entry.time },
                                  { QStringLiteral("label"),
                                    english.toString(time, QStringLiteral("d MMMM yyyy, HH:mm")) },
                                  { QStringLiteral("message"), entry.message } });
    }
    return list;
}

QString ProjectSession::restoreVersion(const QString &id, const QString &hash)
{
    const int index = indexOf(id);
    if (!versionsOn() || project.readOnly || index < 0 || project.items.at(index).file.isEmpty())
        return QStringLiteral("This text has no versions.");
    const QString file = project.items.at(index).file;
    bool ok = false;
    const QString old = Versions(project.folder).fileAt(hash, file, &ok);
    if (!ok)
        return QStringLiteral("The version does not hold this text.");
    if (const QString error = saveVersion(
            QStringLiteral("Before the restore of %1").arg(project.items.at(index).title));
        !error.isEmpty())
        return error;
    QSaveFile out(QDir(project.folder).filePath(file));
    const QByteArray bytes = old.toUtf8() + (old.endsWith(u'\n') ? "" : "\n");
    if (!out.open(QIODevice::WriteOnly) || out.write(bytes) != bytes.size() || !out.commit())
        return QStringLiteral("Cannot write %1.").arg(file);
    noteSaved(file);
    if (text.file() == file)
        text.open(project.folder, file);
    for (const QPointer<Document> &document : std::as_const(tracked)) {
        if (document && document->file() == file)
            document->reloadIfClean();
    }
    emit versionsChanged();
    return { };
}

QString ProjectSession::restoreProject(const QString &hash)
{
    if (!versionsOn() || project.readOnly)
        return QStringLiteral("This project has no versions.");
    if (const QString error = saveVersion(QStringLiteral("Before the restore of the project"));
        !error.isEmpty())
        return error;
    Versions versions(project.folder);
    if (const QString error = versions.restoreAll(hash); !error.isEmpty())
        return error;
    // The binder and the texts changed on disk: load them again.
    reload();
    text.open(project.folder, text.file());
    for (const QPointer<Document> &document : std::as_const(tracked)) {
        if (document)
            document->reloadIfClean();
    }
    const QString error
        = versions.save(QStringLiteral("Restored a version"),
                        QString::fromStdString(project.data["author"].value_or(std::string())));
    emit versionsChanged();
    return error;
}

int ProjectSession::streak() const
{
    return history ? history->streak(QDate::currentDate()) : 0;
}

QVariantList ProjectSession::historyDays(int count) const
{
    QVariantList list;
    const QDate today = QDate::currentDate();
    for (int i = count - 1; i >= 0; --i) {
        const QDate day = today.addDays(-i);
        list.append(QVariantMap { { QStringLiteral("date"), day.toString(Qt::ISODate) },
                                  { QStringLiteral("words"), history ? history->words(day) : 0 } });
    }
    return list;
}

QString ProjectSession::deadline() const
{
    const auto date = project.data["goals"]["deadline"].value<toml::date>();
    if (!date)
        return { };
    return QDate(date->year, date->month, date->day).toString(Qt::ISODate);
}

int ProjectSession::dailyNeeded() const
{
    const QDate end = QDate::fromString(deadline(), Qt::ISODate);
    const qint64 days = QDate::currentDate().daysTo(end) + 1;
    if (goalWords() <= 0 || !end.isValid() || days < 1)
        return -1;
    // ponytail: counts from the words of now, so the figure falls while the
    // author writes today; use the words at the start of the day from the
    // history if that confuses.
    const int left = std::max(0, goalWords() - manuscriptWords());
    return static_cast<int>((left + days - 1) / days);
}

bool ProjectSession::setGoal(int words, const QString &deadline)
{
    const QDate date = QDate::fromString(deadline.trimmed(), Qt::ISODate);
    if (project.readOnly || words < 0 || (!deadline.trimmed().isEmpty() && !date.isValid()))
        return false;
    if (project.data.get_as<toml::table>("goals") == nullptr)
        project.data.insert_or_assign("goals", toml::table { });
    toml::table &goals = *project.data.get_as<toml::table>("goals");
    if (words > 0)
        goals.insert_or_assign("words", words);
    else
        goals.erase("words");
    if (date.isValid())
        goals.insert_or_assign("deadline", toml::date(date.year(), date.month(), date.day()));
    else
        goals.erase("deadline");
    if (goals.empty())
        project.data.erase("goals");
    return commit({ });
}

int ProjectSession::goalWords() const
{
    return static_cast<int>(project.data["goals"]["words"].value_or(std::int64_t { 0 }));
}

namespace {

QString readTextFile(const QString &path, bool *ok)
{
    QFile file(path);
    *ok = file.open(QIODevice::ReadOnly);
    QString text = *ok ? QString::fromUtf8(file.readAll()) : QString();
    if (text.endsWith(u'\n'))
        text.chop(1);
    return text;
}

bool writeTextFile(const QString &path, QString text)
{
    if (!text.isEmpty())
        text += u'\n';
    QSaveFile file(path);
    const QByteArray bytes = text.toUtf8();
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
}

} // namespace

QString ProjectSession::split(const QString &id, int position)
{
    editError.clear();
    const int index = indexOf(id);
    if (index < 0 || project.readOnly || project.items.at(index).kind != QLatin1String("scene")
        || project.items.at(index).section == QLatin1String("trash")) {
        editError = QStringLiteral("Only a scene can split.");
        emit edited();
        return { };
    }
    // The file must hold the latest text before the split reads it.
    if (current == id && !text.save()) {
        editError = QStringLiteral("Save the scene first.");
        emit edited();
        return { };
    }
    const QDir dir(project.folder);
    const ProjectItem source = project.items.at(index);
    bool ok = false;
    const QString all = readTextFile(dir.filePath(source.file), &ok);
    // Only the whitespace at the cut moves; no other character changes.
    const QString first = all.left(position).trimmed().isEmpty() ? QString() : [&] {
        QString left = all.left(position);
        while (!left.isEmpty() && left.back().isSpace())
            left.chop(1);
        return left;
    }();
    QString second = all.mid(position);
    while (!second.isEmpty() && second.front().isSpace())
        second.remove(0, 1);
    if (!ok || position < 0 || position > all.size() || first.isEmpty() || second.isEmpty()) {
        editError = QStringLiteral("There is no text on one side of the cursor.");
        emit edited();
        return { };
    }

    QSet<QString> ids;
    for (const ProjectItem &item : std::as_const(project.items))
        ids.insert(item.id);
    ProjectItem item;
    item.id = Naming::newItemId(ids);
    item.kind = source.kind;
    item.title = source.title + QStringLiteral(" (2)");
    for (const char *key : { "pov", "place", "time" }) {
        if (const toml::node *value = source.data.get(key))
            item.data.insert(key, *value);
    }
    const QStringList names
        = QDir(dir.filePath(QStringLiteral("manuscript"))).entryList(QDir::Files);
    item.file = QStringLiteral("manuscript/")
        + Naming::fileNameFor(item.title, QSet<QString>(names.begin(), names.end()));
    // The new file first: a crash after this keeps the text twice, never
    // loses it.
    if (!writeTextFile(dir.filePath(item.file), second)
        || !writeTextFile(dir.filePath(source.file), first)) {
        editError = QStringLiteral("Cannot write the split scenes.");
        emit edited();
        return { };
    }
    project.items.append(item);
    const int added = static_cast<int>(project.items.size() - 1);
    const int parent = project.items.at(index).parent;
    const QList<int> &siblings = parent >= 0 ? project.items.at(parent).children : project.roots;
    place(added, parent, static_cast<int>(siblings.indexOf(index)) + 1, source.section);
    commit({ item.file, source.file });
    if (current == id)
        text.open(project.folder, source.file);
    setCurrentId(item.id);
    return item.id;
}

QString ProjectSession::nextScene(const QString &id) const
{
    const int index = indexOf(id);
    if (index < 0 || project.items.at(index).kind != QLatin1String("scene")
        || project.items.at(index).section == QLatin1String("trash"))
        return { };
    const int parent = project.items.at(index).parent;
    const QList<int> &siblings = parent >= 0 ? project.items.at(parent).children : project.roots;
    const qsizetype at = siblings.indexOf(index) + 1;
    if (at >= siblings.size() || project.items.at(siblings.at(at)).kind != QLatin1String("scene"))
        return { };
    return project.items.at(siblings.at(at)).id;
}

bool ProjectSession::mergeWithNext(const QString &id)
{
    editError.clear();
    const QString next = nextScene(id);
    if (next.isEmpty() || project.readOnly) {
        editError = QStringLiteral("There is no next scene to merge.");
        emit edited();
        return false;
    }
    text.save();
    const QDir dir(project.folder);
    const int first = indexOf(id);
    const int second = indexOf(next);
    bool okFirst = false;
    bool okSecond = false;
    const QString a = readTextFile(dir.filePath(project.items.at(first).file), &okFirst);
    const QString b = readTextFile(dir.filePath(project.items.at(second).file), &okSecond);
    if (!okFirst || !okSecond) {
        editError = QStringLiteral("Cannot read the scenes to merge.");
        emit edited();
        return false;
    }
    const QString joined = a.isEmpty() ? b : b.isEmpty() ? a : a + QStringLiteral("\n\n") + b;
    if (!writeTextFile(dir.filePath(project.items.at(first).file), joined)) {
        editError = QStringLiteral("Cannot write the merged scene.");
        emit edited();
        return false;
    }
    // The notes of the second scene join the notes of the first.
    const QString notesA
        = QString::fromStdString(project.items.at(first).data["notes"].value_or(std::string()));
    const QString notesB
        = QString::fromStdString(project.items.at(second).data["notes"].value_or(std::string()));
    if (!notesB.isEmpty()) {
        const QString notes = notesA.isEmpty() ? notesB : notesA + QStringLiteral("\n\n") + notesB;
        project.items[first].data.insert_or_assign("notes", notes.toStdString());
    }
    noteSaved(project.items.at(first).file);
    // The second scene keeps its file in the trash, so its old text stays.
    moveToTrash(next);
    if (current == id || current.isEmpty())
        text.open(project.folder, project.items.at(first).file);
    setCurrentId(id);
    return true;
}

QVariantMap ProjectSession::fields(const QString &id) const
{
    QVariantMap map;
    const int index = indexOf(id);
    if (index < 0)
        return map;
    const ProjectItem &item = project.items.at(index);
    const auto string = [&item](const char *key) {
        return QString::fromStdString(item.data[key].value_or(std::string()));
    };
    map.insert(QStringLiteral("title"), item.title);
    map.insert(QStringLiteral("kind"), item.kind);
    map.insert(QStringLiteral("file"), holdsNoText(item.kind) ? QString() : item.file);
    map.insert(QStringLiteral("parent"),
               item.parent >= 0 ? project.items.at(item.parent).id : QString());
    const QString status = string("status");
    map.insert(QStringLiteral("status"),
               status.isEmpty() && item.kind == QLatin1String("scene") ? QStringLiteral("draft")
                                                                       : status);
    for (const char *key : { "pov", "place", "time", "synopsis", "notes", "beat", "date", "label" })
        map.insert(QString::fromLatin1(key), string(key));
    map.insert(QStringLiteral("dateLabel"), calendar().label(string("date")));
    QStringList tags;
    if (const toml::array *list = item.data.get_as<toml::array>("tags")) {
        for (const toml::node &tag : *list)
            tags.append(QString::fromStdString(tag.value_or(std::string())));
    }
    map.insert(QStringLiteral("tags"), tags);
    QStringList aliases;
    if (const toml::array *list = item.data.get_as<toml::array>("aliases")) {
        for (const toml::node &alias : *list)
            aliases.append(QString::fromStdString(alias.value_or(std::string())));
    }
    map.insert(QStringLiteral("aliases"), aliases);
    map.insert(QStringLiteral("plotlines"), strings(item.data.get("plotlines")));
    // The values of the fields of the entity type, by field name.
    QVariantMap values;
    if (const toml::table *fields = item.data.get_as<toml::table>("fields")) {
        for (const auto &[key, value] : *fields)
            values.insert(
                QString::fromUtf8(key.str().data(), static_cast<qsizetype>(key.str().size())),
                QString::fromStdString(value.value_or(std::string())));
    }
    map.insert(QStringLiteral("values"), values);
    map.insert(QStringLiteral("target"),
               static_cast<int>(item.data["target"].value_or(std::int64_t { 0 })));
    map.insert(QStringLiteral("words"), binderModel.wordsOf(id));
    map.insert(QStringLiteral("include"), item.data["include"].value_or(true));
    map.insert(QStringLiteral("query"), string("query"));
    map.insert(QStringLiteral("regex"), item.data["regex"].value_or(false));
    return map;
}

bool ProjectSession::setField(const QString &id, const QString &key, const QVariant &value)
{
    const int index = indexOf(id);
    if (index < 0 || project.readOnly)
        return false;
    toml::table &data = project.items[index].data;
    const std::string name = key.toStdString();
    if (key == QLatin1String("tags") || key == QLatin1String("aliases")
        || key == QLatin1String("plotlines")) {
        // A list of names: trimmed, no empty names, no copies, in the order given.
        QStringList tags;
        for (const QString &tag : value.toStringList()) {
            const QString clean = tag.trimmed();
            if (!clean.isEmpty() && !tags.contains(clean))
                tags.append(clean);
        }
        if (tags.isEmpty()) {
            data.erase(name);
        } else {
            toml::array list;
            for (const QString &tag : std::as_const(tags))
                list.push_back(tag.toStdString());
            data.insert_or_assign(name, std::move(list));
        }
    } else if (key == QLatin1String("target")) {
        bool ok = false;
        const int target = value.toInt(&ok);
        if (!ok || target < 0)
            return false;
        if (target == 0)
            data.erase(name);
        else
            data.insert_or_assign(name, target);
    } else if (key == QLatin1String("include")) {
        // The default is true, so only false is written.
        if (value.typeId() != QMetaType::Bool)
            return false;
        if (value.toBool())
            data.erase(name);
        else
            data.insert_or_assign(name, false);
    } else if (key == QLatin1String("status")) {
        const QString status = value.toString();
        if (status != QLatin1String("draft") && status != QLatin1String("revised")
            && status != QLatin1String("final"))
            return false;
        data.insert_or_assign(name, status.toStdString());
    } else if (key == QLatin1String("date")) {
        // A date of story time, valid in the calendar of the project.
        const QString date = value.toString().trimmed();
        if (date.isEmpty())
            data.erase(name);
        else if (calendar().dayOf(date))
            data.insert_or_assign(name, date.toStdString());
        else
            return false;
    } else if (key == QLatin1String("label")) {
        // A colour label: the name of a colour of the theme, or "" for none.
        const QString label = value.toString();
        if (label.isEmpty())
            data.erase(name);
        else if (QStringList { "blue", "orange", "green", "yellow", "pink" }.contains(label))
            data.insert_or_assign(name, label.toStdString());
        else
            return false;
    } else if (key == QLatin1String("beat") || key == QLatin1String("pov")
               || key == QLatin1String("place") || key == QLatin1String("time")
               || key == QLatin1String("synopsis") || key == QLatin1String("notes")) {
        const QString text = value.toString().trimmed();
        if (text.isEmpty())
            data.erase(name);
        else
            data.insert_or_assign(name, text.toStdString());
    } else {
        return false;
    }
    return commit({ });
}

QVariantList ProjectSession::childrenOf(const QString &id) const
{
    QVariantList list;
    const int index = indexOf(id);
    if (index < 0)
        return list;
    for (const int child : project.items.at(index).children) {
        const ProjectItem &item = project.items.at(child);
        const QString status = QString::fromStdString(item.data["status"].value_or(std::string()));
        list.append(QVariantMap {
            { QStringLiteral("id"), item.id },
            { QStringLiteral("title"), item.title },
            { QStringLiteral("kind"), item.kind },
            { QStringLiteral("synopsis"),
              QString::fromStdString(item.data["synopsis"].value_or(std::string())) },
            { QStringLiteral("status"),
              status.isEmpty() && item.kind == QLatin1String("scene") ? QStringLiteral("draft")
                                                                      : status },
            { QStringLiteral("words"), binderModel.wordsOf(item.id) },
        });
    }
    return list;
}

QVariantList ProjectSession::outlineOf(const QString &id) const
{
    QVariantList list;
    const int index = indexOf(id);
    if (index < 0)
        return list;
    const auto text = [this](const ProjectItem &item, const char *key) {
        return QString::fromStdString(item.data[key].value_or(std::string()));
    };
    const std::function<void(int)> walk = [&](int at) {
        const ProjectItem &item = project.items.at(at);
        if (!item.file.isEmpty() && !holdsNoText(item.kind)) {
            QVariantMap values;
            if (const toml::table *fields = item.data.get_as<toml::table>("fields")) {
                for (const auto &[key, value] : *fields)
                    values.insert(QString::fromUtf8(key.str().data(),
                                                    static_cast<qsizetype>(key.str().size())),
                                  QString::fromStdString(value.value_or(std::string())));
            }
            const QString status = text(item, "status");
            list.append(QVariantMap {
                { QStringLiteral("id"), item.id },
                { QStringLiteral("title"), item.title },
                { QStringLiteral("status"),
                  status.isEmpty() && item.kind == QLatin1String("scene") ? QStringLiteral("draft")
                                                                          : status },
                { QStringLiteral("pov"), text(item, "pov") },
                { QStringLiteral("place"), text(item, "place") },
                { QStringLiteral("time"), text(item, "time") },
                { QStringLiteral("words"), binderModel.wordsOf(item.id) },
                { QStringLiteral("fields"), values },
            });
        }
        for (const int child : item.children)
            walk(child);
    };
    walk(index);
    return list;
}

QVariantList ProjectSession::textsUnder(const QString &id) const
{
    QVariantList list;
    const int index = indexOf(id);
    if (index < 0)
        return list;
    const std::function<void(int)> walk = [&](int at) {
        for (const int child : project.items.at(at).children) {
            const ProjectItem &item = project.items.at(child);
            if (!item.file.isEmpty() && !holdsNoText(item.kind) && !item.missingFile)
                list.append(QVariantMap { { QStringLiteral("id"), item.id },
                                          { QStringLiteral("title"), item.title },
                                          { QStringLiteral("file"), item.file } });
            walk(child);
        }
    };
    walk(index);
    return list;
}

QVariantMap ProjectSession::viewState() const
{
    QVariantMap map;
    if (project.folder.isEmpty())
        return map;
    QFile file(QDir(project.folder).filePath(QStringLiteral(".codicier/state.toml")));
    if (!file.open(QIODevice::ReadOnly))
        return map;
    const QByteArray bytes = file.readAll();
    // The app can lose this file with no harm, so a bad file is no state.
    toml::table table;
    try {
        table = toml::parse(std::string_view(bytes.constData(), bytes.size()));
    } catch (const toml::parse_error &) {
        return map;
    }
    for (const auto &[key, node] : table) {
        const QString name
            = QString::fromUtf8(key.str().data(), static_cast<qsizetype>(key.str().size()));
        if (const auto *text = node.as_string())
            map.insert(name, QString::fromStdString(text->get()));
        else if (const auto *flag = node.as_boolean())
            map.insert(name, flag->get());
        else if (const auto *number = node.as_integer())
            map.insert(name, static_cast<int>(number->get()));
    }
    return map;
}

bool ProjectSession::saveViewState(const QVariantMap &state)
{
    if (project.folder.isEmpty() || !loadError.isEmpty())
        return false;
    if (!ProjectStore::makeAppFolder(project.folder).isEmpty())
        return false;
    const QDir dir(project.folder);
    toml::table table;
    for (auto it = state.cbegin(); it != state.cend(); ++it) {
        const std::string key = it.key().toStdString();
        switch (it.value().typeId()) {
        case QMetaType::Bool:
            table.insert(key, it.value().toBool());
            break;
        case QMetaType::Int:
        case QMetaType::LongLong:
        case QMetaType::Double:
            table.insert(key, static_cast<std::int64_t>(it.value().toDouble()));
            break;
        default:
            table.insert(key, it.value().toString().toStdString());
        }
    }
    std::ostringstream out;
    out << table << '\n';
    const QByteArray bytes = QByteArray::fromStdString(out.str());
    QSaveFile file(dir.filePath(QStringLiteral(".codicier/state.toml")));
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
}

QVariantList ProjectSession::items() const
{
    QVariantList list;
    const std::function<void(int, const QString &)> walk = [&](int at, const QString &path) {
        const ProjectItem &item = project.items.at(at);
        list.append(QVariantMap { { QStringLiteral("id"), item.id },
                                  { QStringLiteral("title"), item.title },
                                  { QStringLiteral("kind"), item.kind },
                                  { QStringLiteral("path"), path } });
        const QString below
            = path.isEmpty() ? item.title : path + QStringLiteral(" / ") + item.title;
        for (const int child : item.children)
            walk(child, below);
    };
    for (const int root : project.roots) {
        if (project.items.at(root).section != QLatin1String("trash"))
            walk(root, QString());
    }
    return list;
}

void ProjectSession::trackDocument(Document *document, const QString &id)
{
    if (document == nullptr)
        return;
    tracked.removeAll(nullptr);
    tracked.append(document);
    document->setSpellChecker(checkSpelling ? spell.get() : nullptr);
    document->setStyleChecks(checksOfStyle);
    document->setHideMarks(marksHidden);
    document->setEntityNames(knownNames);
    connect(document, &Document::saved, this, [this](const QString &file) {
        changedSinceBackup = true;
        noteSaved(file);
    });
    connect(document, &Document::dirtyChanged, this, [this, document] {
        if (watcher && !document->file().isEmpty())
            watcher->setUnsaved(document->file(), document->dirty());
    });
    connect(document, &Document::wordsChanged, this, [this, document, id] {
        binderModel.setItemWords(id, document->words());
        emit wordsChanged();
    });
}

QString ProjectSession::takeSnapshot(const QString &id, const QString &title)
{
    const int index = indexOf(id);
    if (index < 0 || project.items.at(index).file.isEmpty()
        || holdsNoText(project.items.at(index).kind))
        return { };
    // The snapshot holds the latest typing.
    if (current == id)
        text.save();
    for (const QPointer<Document> &document : std::as_const(tracked)) {
        if (document && document->file() == project.items.at(index).file)
            document->save();
    }
    const QString path = Snapshots::take(project.folder, id, project.items.at(index).file, title);
    if (!path.isEmpty())
        emit snapshotsChanged();
    return path;
}

QVariantList ProjectSession::snapshots(const QString &id) const
{
    QVariantList list;
    const QLocale english(QLocale::English, QLocale::UnitedKingdom);
    for (const Snapshots::Snapshot &snapshot : Snapshots::list(project.folder, id)) {
        list.append(QVariantMap {
            { QStringLiteral("path"), snapshot.path },
            { QStringLiteral("time"), snapshot.time },
            { QStringLiteral("title"), snapshot.title },
            { QStringLiteral("label"),
              english.toString(snapshot.time, QStringLiteral("d MMMM yyyy, HH:mm")) },
        });
    }
    return list;
}

bool ProjectSession::restoreSnapshot(const QString &id, const QString &path)
{
    const int index = indexOf(id);
    if (index < 0 || project.readOnly)
        return false;
    const QString file = project.items.at(index).file;
    // The text to replace must be on disk, so "Before restore" holds it.
    if (current == id && !text.save())
        return false;
    for (const QPointer<Document> &document : std::as_const(tracked)) {
        if (document && document->file() == file)
            document->save();
    }
    if (!Snapshots::restore(project.folder, id, file, path))
        return false;
    noteSaved(file);
    if (current == id)
        text.open(project.folder, file);
    for (const QPointer<Document> &document : std::as_const(tracked)) {
        if (document && document->file() == file)
            document->reloadIfClean();
    }
    emit snapshotsChanged();
    return true;
}

QVariantMap ProjectSession::compare(const QString &id, const QString &snapshotPath) const
{
    const int index = indexOf(id);
    const bool version = snapshotPath.startsWith(QStringLiteral("version:"));
    if (index < 0
        || (!version
            && (!snapshotPath.startsWith(QStringLiteral("snapshots/") + id + u'/')
                || snapshotPath.contains(QStringLiteral("..")))))
        return { };
    const QDir dir(project.folder);
    bool ok = false;
    // A saved version: the text of the file in that version.
    const QString before = version
        ? Versions(project.folder).fileAt(snapshotPath.mid(8), project.items.at(index).file, &ok)
        : readTextFile(dir.filePath(snapshotPath), &ok);
    if (!ok)
        return { };
    // The open text is newer than its file while edits wait for the save.
    const QString after = current == id
        ? text.text()
        : readTextFile(dir.filePath(project.items.at(index).file), &ok);
    QVariantList segments;
    int added = 0;
    int removed = 0;
    for (const Diff::Segment &segment : Diff::words(before, after)) {
        const char *kind = segment.kind == Diff::Segment::Added ? "added"
            : segment.kind == Diff::Segment::Removed            ? "removed"
                                                                : "same";
        if (segment.kind == Diff::Segment::Added)
            added += Index::countWords(segment.text);
        else if (segment.kind == Diff::Segment::Removed)
            removed += Index::countWords(segment.text);
        segments.append(QVariantMap { { QStringLiteral("kind"), QString::fromLatin1(kind) },
                                      { QStringLiteral("text"), segment.text } });
    }
    return { { QStringLiteral("segments"), segments },
             { QStringLiteral("addedWords"), added },
             { QStringLiteral("removedWords"), removed } };
}

int ProjectSession::trashCount() const
{
    int count = 0;
    for (const ProjectItem &item : project.items) {
        if (item.section == QLatin1String("trash"))
            ++count;
    }
    return count;
}

int ProjectSession::emptyTrash()
{
    editError.clear();
    if (project.readOnly || !loadError.isEmpty())
        return -1;
    const int count = trashCount();
    if (count == 0)
        return 0;
    // A backup first, so a mistake can be undone from the zip.
    if (!backupFolder.isEmpty()) {
        changedSinceBackup = true;
        if (backup().isEmpty()) {
            editError = QStringLiteral("The backup failed, so the trash stays.");
            emit edited();
            return -1;
        }
    }
    if (indexOf(current) >= 0
        && project.items.at(indexOf(current)).section == QLatin1String("trash"))
        setCurrentId(QString());

    const QDir dir(project.folder);
    QList<ProjectItem> kept;
    QList<int> newIndex(project.items.size(), -1);
    for (qsizetype i = 0; i < project.items.size(); ++i) {
        const ProjectItem &item = project.items.at(i);
        if (item.section != QLatin1String("trash")) {
            newIndex[i] = static_cast<int>(kept.size());
            kept.append(item);
            continue;
        }
        if (!item.file.isEmpty())
            QFile::remove(dir.filePath(item.file));
        QDir(dir.filePath(QStringLiteral("snapshots/") + item.id)).removeRecursively();
    }
    // The indexes of the parents, the children, and the roots change.
    for (ProjectItem &item : kept) {
        item.parent = item.parent >= 0 ? newIndex.at(item.parent) : -1;
        QList<int> children;
        for (const int child : std::as_const(item.children)) {
            if (newIndex.at(child) >= 0)
                children.append(newIndex.at(child));
        }
        item.children = children;
    }
    QList<int> roots;
    for (const int root : std::as_const(project.roots)) {
        if (newIndex.at(root) >= 0)
            roots.append(newIndex.at(root));
    }
    project.items = std::move(kept);
    project.roots = std::move(roots);
    commit({ });
    return count;
}

namespace {

// Marks that no text holds, so the escape of a snippet cannot touch them.
const QChar openMark(0x2);
const QChar closeMark(0x3);

// A snippet with marks as rich text: the words with the match in bold, with
// no Markdown marks and no footnotes.
QString snippetHtml(QString snippet)
{
    snippet.remove(QRegularExpression(QStringLiteral("\\[\\^[^\\]]*\\]|\\*")));
    snippet = snippet.simplified().toHtmlEscaped();
    return snippet.replace(openMark, QStringLiteral("<b>"))
        .replace(closeMark, QStringLiteral("</b>"));
}

QRegularExpression searchPattern(const QString &pattern)
{
    return QRegularExpression(pattern,
                              QRegularExpression::CaseInsensitiveOption
                                  | QRegularExpression::UseUnicodePropertiesOption);
}

// The first match that is not empty, as a start and a length. The start is
// -1 if there is no such match.
std::pair<qsizetype, qsizetype> regexMatch(const QRegularExpression &pattern, const QString &text)
{
    QRegularExpressionMatchIterator it = pattern.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        if (match.capturedLength() > 0)
            return { match.capturedStart(), match.capturedLength() };
    }
    return { -1, 0 };
}

QString readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}

} // namespace

QString ProjectSession::patternError(const QString &pattern) const
{
    const QRegularExpression expression = searchPattern(pattern);
    if (expression.isValid())
        return { };
    // PCRE writes "missing closing parenthesis": make it a sentence.
    QString error = expression.errorString();
    error[0] = error[0].toUpper();
    return error + u'.';
}

QVariantList ProjectSession::search(const QString &query, bool regex)
{
    QVariantList list;
    if (project.folder.isEmpty() || !loadError.isEmpty())
        return list;
    // The search reads the files, so the latest typing goes to disk first.
    saveAll();
    const auto append = [&list](const QString &id, const QString &title, const QString &snippet) {
        list.append(QVariantMap {
            { QStringLiteral("id"), id },
            { QStringLiteral("title"), title },
            { QStringLiteral("snippet"), snippetHtml(snippet) },
        });
    };
    if (regex) {
        // A pattern search reads every file in binder order. The index
        // cannot match a pattern.
        const QRegularExpression pattern = searchPattern(query);
        if (query.isEmpty() || !pattern.isValid())
            return list;
        for (const ProjectItem &item : std::as_const(project.items)) {
            if (binderModel.isInTrash(item.id) || item.kind == QLatin1String("search"))
                continue;
            const bool isText = !item.file.isEmpty() && !holdsNoText(item.kind);
            const QString body
                = isText ? readFile(QDir(project.folder).filePath(item.file)) : QString();
            const auto [start, length] = regexMatch(pattern, body);
            if (start >= 0) {
                // About ten words on each side of the match.
                const qsizetype from = std::max<qsizetype>(0, start - 60);
                const qsizetype to = std::min(body.size(), start + length + 80);
                const QString snippet = (from > 0 ? QStringLiteral("...") : QString())
                    + body.mid(from, start - from) + openMark + body.mid(start, length) + closeMark
                    + body.mid(start + length, to - start - length)
                    + (to < body.size() ? QStringLiteral("...") : QString());
                append(item.id, item.title, snippet);
            } else if (regexMatch(pattern, item.title).first >= 0) {
                append(item.id, item.title, QString());
            }
        }
        return list;
    }
    if (!index) {
        index = std::make_unique<Index>(project.folder);
        if (const QString error = index->open(); !error.isEmpty()) {
            qWarning("Codicier: cannot open the search index: %s", qPrintable(error));
            index.reset();
            return list;
        }
        indexStale = true;
    }
    QString error;
    if (indexStale) {
        error = index->rebuild(project);
    } else {
        for (const ProjectItem &item : std::as_const(project.items)) {
            if (error.isEmpty() && !item.file.isEmpty() && staleFiles.contains(item.file))
                error = index->update(project, item.id);
        }
    }
    if (!error.isEmpty()) {
        qWarning("Codicier: cannot update the search index: %s", qPrintable(error));
        return list;
    }
    indexStale = false;
    staleFiles.clear();
    for (const SearchHit &hit : index->search(query, openMark, closeMark)) {
        // A saved search is not a result: its title is only a query.
        const int at = indexOf(hit.id);
        if (at >= 0 && !binderModel.isInTrash(hit.id)
            && project.items.at(at).kind != QLatin1String("search"))
            append(hit.id, hit.title, hit.snippet);
    }
    return list;
}

namespace {

// The text of one manuscript item and the matches in it.
struct ReplaceTarget
{
    QString id;
    QString title;
    QString file;
    QString text;
    QList<QRegularExpressionMatch> matches;
};

// A plain query matches as it is written, as a whole word. A pattern
// ignores case, as in the search.
QRegularExpression replacePattern(const QString &query, bool regex)
{
    if (regex)
        return searchPattern(query);
    return QRegularExpression(QStringLiteral("(?<!\\w)") + QRegularExpression::escape(query)
                                  + QStringLiteral("(?!\\w)"),
                              QRegularExpression::UseUnicodePropertiesOption);
}

// The replacement for one match. In a pattern, \1 to \9 are the groups and
// \0 is the whole match.
QString expand(const QString &replacement, const QRegularExpressionMatch &match, bool regex)
{
    if (!regex)
        return replacement;
    QString out;
    for (qsizetype i = 0; i < replacement.size(); ++i) {
        const QChar c = replacement.at(i);
        if (c == u'\\' && i + 1 < replacement.size() && replacement.at(i + 1).isDigit()) {
            out += match.captured(replacement.at(i + 1).digitValue());
            ++i;
        } else {
            out += c;
        }
    }
    return out;
}

// The text items of the manuscript outside the trash, with their matches.
QList<ReplaceTarget> replaceTargets(const Project &project, const BinderModel &binder,
                                    const QRegularExpression &pattern)
{
    QList<ReplaceTarget> targets;
    if (!pattern.isValid() || pattern.pattern().isEmpty())
        return targets;
    for (const ProjectItem &item : project.items) {
        if (item.section != QLatin1String("manuscript") || item.file.isEmpty()
            || holdsNoText(item.kind) || binder.isInTrash(item.id))
            continue;
        ReplaceTarget target {
            item.id, item.title, item.file, readFile(QDir(project.folder).filePath(item.file)), { }
        };
        QRegularExpressionMatchIterator it = pattern.globalMatch(target.text);
        while (it.hasNext()) {
            const QRegularExpressionMatch match = it.next();
            if (match.capturedLength() > 0)
                target.matches.append(match);
        }
        if (!target.matches.isEmpty())
            targets.append(target);
    }
    return targets;
}

} // namespace

QVariantList ProjectSession::replacePreview(const QString &query, const QString &replacement,
                                            bool regex)
{
    QVariantList list;
    saveAll();
    // Marks that no text holds, for the old and the new words.
    const QChar newOpen(0x4);
    const QChar newClose(0x5);
    for (const ReplaceTarget &target :
         replaceTargets(project, binderModel, replacePattern(query, regex))) {
        QStringList lines;
        for (const QRegularExpressionMatch &match : target.matches) {
            const qsizetype start = match.capturedStart();
            const qsizetype end = match.capturedEnd();
            const qsizetype from = std::max<qsizetype>(0, start - 40);
            const qsizetype to = std::min(target.text.size(), end + 40);
            QString line = (from > 0 ? QStringLiteral("...") : QString())
                + target.text.mid(from, start - from) + openMark + match.captured() + closeMark
                + u' ' + newOpen + expand(replacement, match, regex) + newClose
                + target.text.mid(end, to - end)
                + (to < target.text.size() ? QStringLiteral("...") : QString());
            line = line.simplified().toHtmlEscaped();
            line.replace(openMark, QStringLiteral("<s>"))
                .replace(closeMark, QStringLiteral("</s>"))
                .replace(newOpen, QStringLiteral("<b>"))
                .replace(newClose, QStringLiteral("</b>"));
            lines.append(line);
        }
        list.append(QVariantMap {
            { QStringLiteral("id"), target.id },
            { QStringLiteral("title"), target.title },
            { QStringLiteral("count"), static_cast<int>(target.matches.size()) },
            { QStringLiteral("lines"), lines },
        });
    }
    return list;
}

int ProjectSession::replaceAll(const QString &query, const QString &replacement, bool regex)
{
    if (project.readOnly || !loadError.isEmpty())
        return -1;
    saveAll();
    int count = 0;
    for (const ReplaceTarget &target :
         replaceTargets(project, binderModel, replacePattern(query, regex))) {
        // The snapshot holds the text before the change, so the author can
        // go back. With no snapshot, the scene does not change.
        if (takeSnapshot(target.id, QStringLiteral("Before replace")).isEmpty())
            return -1;
        QString out;
        qsizetype at = 0;
        for (const QRegularExpressionMatch &match : target.matches) {
            out += target.text.mid(at, match.capturedStart() - at);
            out += expand(replacement, match, regex);
            at = match.capturedEnd();
        }
        out += target.text.mid(at);
        QSaveFile file(QDir(project.folder).filePath(target.file));
        const QByteArray bytes = out.toUtf8();
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
            return -1;
        changedSinceBackup = true;
        noteSaved(target.file);
        binderModel.setItemWords(target.id, Index::countWords(out));
        if (text.file() == target.file)
            text.reloadIfClean();
        for (const QPointer<Document> &document : std::as_const(tracked)) {
            if (document && document->file() == target.file)
                document->reloadIfClean();
        }
        count += static_cast<int>(target.matches.size());
    }
    emit wordsChanged();
    return count;
}

void ProjectSession::saveAll()
{
    text.save();
    for (const QPointer<Document> &document : std::as_const(tracked)) {
        if (document)
            document->save();
    }
}

QVariantMap ProjectSession::matchIn(const QString &id, const QString &query, bool regex) const
{
    qsizetype start = -1;
    qsizetype length = 0;
    const int at = indexOf(id);
    if (at >= 0 && !project.items.at(at).file.isEmpty()) {
        const QString body = readFile(QDir(project.folder).filePath(project.items.at(at).file));
        if (regex) {
            std::tie(start, length) = regexMatch(searchPattern(query), body);
        } else {
            start = Index::firstMatch(body, query);
            length = query.split(u' ', Qt::SkipEmptyParts).value(0).size();
        }
    }
    return { { QStringLiteral("start"), static_cast<int>(start) },
             { QStringLiteral("length"), static_cast<int>(start < 0 ? 0 : length) } };
}

int ProjectSession::firstMatch(const QString &id, const QString &query) const
{
    return matchIn(id, query, false).value(QStringLiteral("start")).toInt();
}

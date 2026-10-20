#include "ProjectStore.h"

#include "TomlWriter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSaveFile>

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

QString LoadError::text() const
{
    if (line > 0)
        return QStringLiteral("%1:%2: %3").arg(file).arg(line).arg(message);
    return QStringLiteral("%1: %2").arg(file, message);
}

namespace {

constexpr std::array<std::string_view, 7> projectKinds {
    "short-story", "novella", "novel", "series", "essay", "essay-collection", "non-fiction"
};
constexpr std::array<std::string_view, 8> sections {
    "manuscript", "front", "back", "codex", "notes", "research", "searches", "trash"
};
constexpr std::array<std::string_view, 3> statuses { "draft", "revised", "final" };
constexpr std::string_view idAlphabet = "0123456789abcdefghjkmnpqrstvwxyz";
constexpr std::size_t idLength = 10;

// What each kind of item can hold and where it can be. An empty folder means
// the kind has no file. An empty first section means any section.
struct KindRule
{
    std::string_view kind;
    std::string_view folder;
    std::string_view section;
    std::string_view otherSection;
};

constexpr std::array<KindRule, 14> kindRules { {
    { "part", "", "manuscript", "" },
    { "chapter", "", "manuscript", "" },
    { "folder", "", "", "" },
    { "scene", "manuscript/", "manuscript", "" },
    { "page", "matter/", "front", "back" },
    { "note", "notes/", "notes", "" },
    { "file", "research/", "research", "" },
    { "search", "", "searches", "" },
    // The entities of the codex: the people, places, and things of the story.
    { "character", "codex/", "codex", "" },
    { "place", "codex/", "codex", "" },
    { "item", "codex/", "codex", "" },
    { "faction", "codex/", "codex", "" },
    { "lore", "codex/", "codex", "" },
    { "term", "codex/", "codex", "" },
} };

template <std::size_t N>
bool contains(const std::array<std::string_view, N> &list, std::string_view value)
{
    return std::ranges::find(list, value) != list.end();
}

const KindRule *ruleFor(std::string_view kind)
{
    const auto *rule = std::ranges::find(kindRules, kind, &KindRule::kind);
    return rule == kindRules.end() ? nullptr : rule;
}

bool validId(std::string_view id)
{
    return id.size() == idLength && std::ranges::all_of(id, [](char c) {
               return idAlphabet.find(c) != std::string_view::npos;
           });
}

QString qs(std::string_view text)
{
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

int lineOf(const toml::node &node)
{
    return static_cast<int>(node.source().begin.line);
}

QString typeName(toml::node_type type)
{
    switch (type) {
    case toml::node_type::string:
        return QStringLiteral("a string");
    case toml::node_type::integer:
        return QStringLiteral("an integer");
    case toml::node_type::date:
        return QStringLiteral("a date");
    case toml::node_type::date_time:
        return QStringLiteral("a date and time");
    case toml::node_type::array:
        return QStringLiteral("an array");
    case toml::node_type::boolean:
        return QStringLiteral("a boolean");
    case toml::node_type::table:
        return QStringLiteral("a table");
    default:
        return QStringLiteral("a value");
    }
}

class Loader
{
public:
    explicit Loader(const QString &folder) : dir(folder) { result.project.folder = folder; }

    LoadResult run()
    {
        if (!dir.exists()) {
            error(dir.path(), 0, QStringLiteral("the folder does not exist"));
            return result;
        }
        loadProject();
        loadBinder();
        return result;
    }

private:
    QDir dir;
    LoadResult result;

    void error(const QString &file, int line, const QString &message)
    {
        result.errors.append({ file, line, message });
    }

    std::optional<toml::table> parse(const QString &name)
    {
        QFile file(dir.filePath(name));
        if (!file.open(QIODevice::ReadOnly)) {
            error(name, 0, QStringLiteral("cannot read the file"));
            return std::nullopt;
        }
        const QByteArray bytes = file.readAll();
        try {
            return toml::parse(std::string_view(bytes.constData(), bytes.size()),
                               name.toStdString());
        } catch (const toml::parse_error &failure) {
            error(name, static_cast<int>(failure.source().begin.line), qs(failure.description()));
            return std::nullopt;
        }
    }

    // Checks the type of an optional key. Returns false if the key is there
    // with a wrong type.
    bool checkType(const toml::table &table, std::string_view key, toml::node_type type,
                   const QString &file)
    {
        const toml::node *node = table.get(key);
        if (node == nullptr || node->type() == type)
            return true;
        error(file, lineOf(*node), QStringLiteral("`%1` must be %2").arg(qs(key), typeName(type)));
        return false;
    }

    // Reads a string key. A missing required key gives an error at line.
    std::optional<std::string> string(const toml::table &table, std::string_view key,
                                      const QString &file, int line, bool required)
    {
        const toml::node *node = table.get(key);
        if (node == nullptr) {
            if (required)
                error(file, line, QStringLiteral("`%1` is missing").arg(qs(key)));
            return std::nullopt;
        }
        if (!checkType(table, key, toml::node_type::string, file))
            return std::nullopt;
        return node->as_string()->get();
    }

    void checkFormat(const toml::table &table, const QString &file)
    {
        const toml::node *node = table.get("format");
        if (node == nullptr) {
            error(file, 0, QStringLiteral("`format` is missing"));
            return;
        }
        if (!checkType(table, "format", toml::node_type::integer, file))
            return;
        const std::int64_t version = node->as_integer()->get();
        if (version < 1)
            error(file, lineOf(*node), QStringLiteral("`format` must be 1 or more"));
        else if (version > ProjectStore::formatVersion)
            result.project.readOnly = true;
    }

    void loadProject()
    {
        const QString file = QStringLiteral("project.toml");
        const std::optional<toml::table> table = parse(file);
        if (!table)
            return;
        checkFormat(*table, file);
        if (const auto title = string(*table, "title", file, 0, true))
            result.project.title = qs(*title);
        if (const auto kind = string(*table, "kind", file, 0, true)) {
            if (contains(projectKinds, *kind))
                result.project.kind = qs(*kind);
            else
                error(file, lineOf(*table->get("kind")),
                      QStringLiteral("`kind` \"%1\" is not a kind of project").arg(qs(*kind)));
        }
        if (table->get("created") == nullptr)
            error(file, 0, QStringLiteral("`created` is missing"));
        checkType(*table, "created", toml::node_type::date_time, file);
        for (const std::string_view key : { "subtitle", "author", "language" })
            checkType(*table, key, toml::node_type::string, file);
        if (checkType(*table, "goals", toml::node_type::table, file)) {
            if (const toml::table *goals = table->get_as<toml::table>("goals")) {
                checkType(*goals, "words", toml::node_type::integer, file);
                checkType(*goals, "daily", toml::node_type::integer, file);
                checkType(*goals, "deadline", toml::node_type::date, file);
            }
        }
        result.project.data = *table;
    }

    void loadBinder()
    {
        const QString file = QStringLiteral("binder.toml");
        const std::optional<toml::table> table = parse(file);
        if (!table)
            return;
        checkFormat(*table, file);
        result.project.binderData = *table;
        result.project.binderData.erase("item");
        const toml::node *node = table->get("item");
        if (node == nullptr)
            return;
        const toml::array *list = node->as_array();
        if (list == nullptr || !list->is_array_of_tables()) {
            error(file, lineOf(*node), QStringLiteral("`item` must be a list of [[item]] tables"));
            return;
        }

        QHash<QString, int> indexOf;
        QList<QString> parentIds;
        for (const toml::node &entry : *list)
            readItem(*entry.as_table(), file, indexOf, parentIds);

        if (!linkParents(file, indexOf, parentIds))
            return;
        for (int i = 0; i < result.project.items.size(); ++i)
            checkPlace(i, file);
    }

    void readItem(const toml::table &entry, const QString &file, QHash<QString, int> &indexOf,
                  QList<QString> &parentIds)
    {
        const int line = lineOf(entry);
        ProjectItem item;
        item.line = line;
        item.data = entry;
        if (const auto id = string(entry, "id", file, line, true)) {
            if (!validId(*id))
                error(file, lineOf(*entry.get("id")),
                      QStringLiteral("`id` \"%1\" must have 10 characters from %2")
                          .arg(qs(*id), qs(idAlphabet)));
            item.id = qs(*id);
        }
        if (const auto kind = string(entry, "kind", file, line, true)) {
            if (ruleFor(*kind) == nullptr)
                error(file, lineOf(*entry.get("kind")),
                      QStringLiteral("`kind` \"%1\" is not a kind of item").arg(qs(*kind)));
            item.kind = qs(*kind);
        }
        if (const auto title = string(entry, "title", file, line, true))
            item.title = qs(*title);
        if (const auto section = string(entry, "section", file, line, false))
            item.section = qs(*section);
        if (const auto path = string(entry, "file", file, line, false))
            item.file = qs(*path);
        const auto parent = string(entry, "parent", file, line, false);

        for (const std::string_view key :
             { "pov", "place", "time", "synopsis", "notes", "restore", "query" })
            checkType(entry, key, toml::node_type::string, file);
        checkType(entry, "target", toml::node_type::integer, file);
        checkType(entry, "include", toml::node_type::boolean, file);
        checkType(entry, "regex", toml::node_type::boolean, file);
        if (const auto status = string(entry, "status", file, line, false);
            status && !contains(statuses, *status))
            error(file, lineOf(*entry.get("status")),
                  QStringLiteral("`status` must be draft, revised, or final"));
        for (const std::string_view key : { "tags", "aliases" }) {
            if (!checkType(entry, key, toml::node_type::array, file))
                continue;
            const toml::array *list = entry.get_as<toml::array>(key);
            if (list != nullptr && !list->is_homogeneous(toml::node_type::string))
                error(file, lineOf(*list),
                      QStringLiteral("`%1` must hold only strings").arg(qs(key)));
        }

        if (!item.id.isEmpty() && indexOf.contains(item.id))
            error(file, line, QStringLiteral("the id \"%1\" is used twice").arg(item.id));
        indexOf.insert(item.id, static_cast<int>(result.project.items.size()));
        parentIds.append(parent ? qs(*parent) : QString());
        result.project.items.append(item);
    }

    // Sets the parent and the children of each item. Returns false if the
    // tree is broken.
    bool linkParents(const QString &file, const QHash<QString, int> &indexOf,
                     const QList<QString> &parentIds)
    {
        QList<ProjectItem> &items = result.project.items;
        bool linked = true;
        for (int i = 0; i < items.size(); ++i) {
            ProjectItem &item = items[i];
            const int line = item.line;
            const QString &parentId = parentIds.at(i);
            if (parentId.isEmpty()) {
                if (item.section.isEmpty()) {
                    error(file, line, QStringLiteral("an item at the top level needs a `section`"));
                    linked = false;
                } else if (!contains(sections, item.section.toStdString())) {
                    error(file, line,
                          QStringLiteral("`section` \"%1\" is not a section").arg(item.section));
                    linked = false;
                }
                result.project.roots.append(i);
                continue;
            }
            if (!item.section.isEmpty())
                error(file, line, QStringLiteral("an item with a `parent` has no `section`"));
            const auto parent = indexOf.constFind(parentId);
            if (parent == indexOf.constEnd()) {
                error(file, line, QStringLiteral("the parent \"%1\" does not exist").arg(parentId));
                linked = false;
                continue;
            }
            const KindRule *parentRule = ruleFor(items.at(*parent).kind.toStdString());
            if (parentRule != nullptr && !parentRule->folder.empty()) {
                error(file, line,
                      QStringLiteral("a %1 cannot hold other items").arg(items.at(*parent).kind));
                linked = false;
            }
            item.parent = *parent;
            items[*parent].children.append(i);
        }
        if (!linked)
            return false;

        // Each item must reach the top level. A loop never does.
        for (int i = 0; i < items.size(); ++i) {
            int top = i;
            qsizetype steps = 0;
            while (items.at(top).parent >= 0 && steps <= items.size()) {
                top = items.at(top).parent;
                ++steps;
            }
            if (steps > items.size()) {
                error(file, items.at(i).line,
                      QStringLiteral("the parents of \"%1\" form a loop").arg(items.at(i).id));
                return false;
            }
            items[i].section = items.at(top).section;
        }
        return true;
    }

    // Checks that the kind fits the section and that the file is correct.
    void checkPlace(int index, const QString &file)
    {
        ProjectItem &item = result.project.items[index];
        const KindRule *rule = ruleFor(item.kind.toStdString());
        if (rule == nullptr)
            return;
        const int line = item.line;
        const bool inTrash = item.section == QLatin1String("trash");
        if (!inTrash && !rule->section.empty() && item.section != qs(rule->section)
            && item.section != qs(rule->otherSection))
            error(file, line,
                  QStringLiteral("a %1 cannot be in the %2 section").arg(item.kind, item.section));

        if (rule->folder.empty()) {
            if (!item.file.isEmpty())
                error(file, line, QStringLiteral("a %1 has no `file`").arg(item.kind));
            return;
        }
        if (item.file.isEmpty()) {
            error(file, line, QStringLiteral("a %1 needs a `file`").arg(item.kind));
            return;
        }
        const bool outside = QDir::isAbsolutePath(item.file)
            || item.file.split(QLatin1Char('/')).contains(QStringLiteral(".."));
        if (outside || !item.file.startsWith(qs(rule->folder))) {
            error(
                file, line,
                QStringLiteral("the file of a %1 must be in %2").arg(item.kind, qs(rule->folder)));
            return;
        }
        item.missingFile = !QFileInfo::exists(dir.filePath(item.file));
    }
};

// The key order of docs/format.md. Other keys follow in alphabetical order.
constexpr std::array<std::string_view, 7> projectOrder { "format", "title",    "subtitle", "author",
                                                         "kind",   "language", "created" };
constexpr std::array<std::string_view, 3> goalsOrder { "words", "daily", "deadline" };
constexpr std::array<std::string_view, 1> binderOrder { "format" };
constexpr std::array<std::string_view, 19> itemOrder { "id",      "kind",    "section", "parent",
                                                       "title",   "file",    "status",  "include",
                                                       "pov",     "place",   "time",    "synopsis",
                                                       "aliases", "tags",    "target",  "query",
                                                       "regex",   "restore", "notes" };

TomlWriter::Order orderFor(std::string_view path)
{
    if (path.empty())
        return projectOrder;
    if (path == "goals")
        return goalsOrder;
    if (path == "item")
        return itemOrder;
    return { };
}

TomlWriter::Order binderOrderFor(std::string_view path)
{
    return path.empty() ? TomlWriter::Order(binderOrder) : orderFor(path);
}

std::string utf8(const QString &text)
{
    return text.toStdString();
}

// The items in tree order: each item, then its children, in binder order.
void appendBranch(const Project &project, int index, toml::array &out)
{
    const ProjectItem &item = project.items.at(index);
    toml::table data = item.data;
    data.insert_or_assign("id", utf8(item.id));
    data.insert_or_assign("kind", utf8(item.kind));
    data.insert_or_assign("title", utf8(item.title));
    if (item.parent < 0) {
        data.insert_or_assign("section", utf8(item.section));
        data.erase("parent");
    } else {
        data.insert_or_assign("parent", utf8(project.items.at(item.parent).id));
        data.erase("section");
    }
    if (item.file.isEmpty())
        data.erase("file");
    else
        data.insert_or_assign("file", utf8(item.file));
    out.push_back(std::move(data));
    for (const int child : item.children)
        appendBranch(project, child, out);
}

QByteArray bytes(const std::string &text)
{
    return QByteArray(text.data(), static_cast<qsizetype>(text.size()));
}

// Writes the file only if its bytes change. Returns an error text or an
// empty string.
QString writeIfChanged(const QDir &dir, const QString &name, const QByteArray &content)
{
    const QString path = dir.filePath(name);
    QFile current(path);
    if (current.open(QIODevice::ReadOnly) && current.readAll() == content)
        return { };
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(content) != content.size() || !file.commit())
        return file.errorString();
    return { };
}

} // namespace

bool ProjectStore::isProjectKind(const QString &kind)
{
    return contains(projectKinds, kind.toStdString());
}

QString ProjectStore::makeAppFolder(const QString &projectFolder)
{
    const QDir dir(projectFolder);
    if (!dir.mkpath(QStringLiteral(".codicier")))
        return QStringLiteral("cannot make the .codicier folder");
    // git must not track the folder: everything in it can be built again.
    const QString ignore = dir.filePath(QStringLiteral(".codicier/.gitignore"));
    if (!QFile::exists(ignore)) {
        QFile file(ignore);
        if (!file.open(QIODevice::WriteOnly) || file.write("*\n") != 2)
            return QStringLiteral("cannot write .codicier/.gitignore");
    }
    return { };
}

LoadResult ProjectStore::load(const QString &folder)
{
    return Loader(folder).run();
}

QByteArray ProjectStore::projectFile(const Project &project)
{
    toml::table data = project.data;
    data.insert_or_assign("format", formatVersion);
    data.insert_or_assign("title", utf8(project.title));
    data.insert_or_assign("kind", utf8(project.kind));
    return bytes(TomlWriter::document(data, orderFor));
}

QByteArray ProjectStore::binderFile(const Project &project)
{
    toml::table data = project.binderData;
    data.insert_or_assign("format", formatVersion);
    toml::array items;
    for (const int root : project.roots)
        appendBranch(project, root, items);
    if (!items.empty())
        data.insert_or_assign("item", std::move(items));
    return bytes(TomlWriter::document(data, binderOrderFor));
}

QList<LoadError> ProjectStore::save(const Project &project)
{
    if (project.readOnly)
        return { { project.folder, 0,
                   QStringLiteral(
                       "the project format is newer than this app, so the app cannot save it") } };
    const QDir dir(project.folder);
    QList<LoadError> errors;
    const std::array<std::pair<QString, QByteArray>, 2> files { {
        { QStringLiteral("project.toml"), projectFile(project) },
        { QStringLiteral("binder.toml"), binderFile(project) },
    } };
    for (const auto &[name, content] : files) {
        if (const QString error = writeIfChanged(dir, name, content); !error.isEmpty())
            errors.append({ name, 0, QStringLiteral("cannot save the file: %1").arg(error) });
    }
    return errors;
}

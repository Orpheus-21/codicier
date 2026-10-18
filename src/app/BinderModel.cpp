#include "BinderModel.h"

#include "Index.h"

#include <QDir>
#include <QFile>

#include <array>

namespace {

constexpr std::array<const char *, 7> sectionOrder { "front",    "manuscript", "back", "notes",
                                                     "research", "searches",   "trash" };

int wordsInFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? Index::countWords(QString::fromUtf8(file.readAll()))
                                          : 0;
}

// A section key is kept apart from item ids with a prefix that no id has.
QString sectionKey(const QString &section)
{
    return QStringLiteral("section:") + section;
}

} // namespace

BinderModel::BinderModel(QObject *parent) : QAbstractListModel(parent) { }

void BinderModel::setProject(const Project &project)
{
    nodes.clear();
    const QDir dir(project.folder);
    for (const ProjectItem &item : project.items) {
        Node node;
        node.id = item.id;
        node.title = item.title;
        node.kind = item.kind;
        node.section = item.section;
        node.status = QString::fromStdString(item.data["status"].value_or(std::string()));
        node.children = item.children;
        node.missing = item.missingFile;
        node.included = item.data["include"].value_or(true);
        if (!item.file.isEmpty() && item.kind != QLatin1String("file") && !item.missingFile)
            node.words = wordsInFile(dir.filePath(item.file));
        nodes.append(node);
    }
    roots = project.roots;
    parents.clear();
    for (const ProjectItem &item : project.items)
        parents.append(item.parent);
    for (const int root : std::as_const(roots))
        sumWords(root);
    // In a new project the trash starts closed, and the rest starts open.
    if (project.folder != folder) {
        folder = project.folder;
        collapsed = { sectionKey(QStringLiteral("trash")) };
    }
    rebuild();
}

int BinderModel::sumWords(int node)
{
    for (const int child : std::as_const(nodes[node].children)) {
        const int words = sumWords(child);
        if (nodes.at(child).included)
            nodes[node].words += words;
    }
    return nodes.at(node).words;
}

void BinderModel::appendBranch(int node, int depth)
{
    rows.append({ node, QString(), depth });
    if (collapsed.contains(nodes.at(node).id))
        return;
    for (const int child : nodes.at(node).children)
        appendBranch(child, depth + 1);
}

// ponytail: a full model reset on each open or close; a row insert and
// remove can replace it if a binder of thousands of rows gets slow.
void BinderModel::rebuild()
{
    beginResetModel();
    rows.clear();
    for (const char *key : sectionOrder) {
        const QString section = QString::fromLatin1(key);
        QList<int> top;
        for (const int root : std::as_const(roots)) {
            if (nodes.at(root).section == section)
                top.append(root);
        }
        if (top.isEmpty())
            continue;
        rows.append({ -1, section, 0 });
        if (collapsed.contains(sectionKey(section)))
            continue;
        for (const int root : std::as_const(top))
            appendBranch(root, 0);
    }
    endResetModel();
}

QString BinderModel::keyOf(const Row &row) const
{
    return row.node < 0 ? sectionKey(row.section) : nodes.at(row.node).id;
}

int BinderModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(rows.size());
}

QVariant BinderModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= rows.size())
        return { };
    const Row &row = rows.at(index.row());
    const bool section = row.node < 0;
    if (section) {
        switch (role) {
        case IdRole:
            return keyOf(row);
        case TitleRole:
            return sectionLabel(row.section);
        case KindRole:
            return QStringLiteral("section");
        case IsSectionRole:
        case HasChildrenRole:
            return true;
        case ExpandedRole:
            return !collapsed.contains(keyOf(row));
        case DepthRole:
        case WordsRole:
            return 0;
        case StatusRole:
            return QString();
        case MissingRole:
            return false;
        case IncludedRole:
            return true;
        default:
            return { };
        }
    }
    const Node &node = nodes.at(row.node);
    switch (role) {
    case IdRole:
        return node.id;
    case TitleRole:
        return node.title;
    case KindRole:
        return node.kind;
    case DepthRole:
        return row.depth;
    case HasChildrenRole:
        return !node.children.isEmpty();
    case ExpandedRole:
        return !collapsed.contains(node.id);
    case IsSectionRole:
        return false;
    case StatusRole:
        return node.kind == QLatin1String("scene") && node.status.isEmpty()
            ? QStringLiteral("draft")
            : node.status;
    case WordsRole:
        return node.words;
    case MissingRole:
        return node.missing;
    case IncludedRole:
        return node.included;
    default:
        return { };
    }
}

QHash<int, QByteArray> BinderModel::roleNames() const
{
    return {
        { IdRole, "itemId" },
        { TitleRole, "title" },
        { KindRole, "kind" },
        { DepthRole, "depth" },
        { HasChildrenRole, "hasChildren" },
        { ExpandedRole, "expanded" },
        { IsSectionRole, "isSection" },
        { StatusRole, "status" },
        { WordsRole, "words" },
        { MissingRole, "missing" },
        { IncludedRole, "included" },
    };
}

void BinderModel::toggle(int row)
{
    if (row < 0 || row >= rows.size())
        return;
    setExpanded(row, collapsed.contains(keyOf(rows.at(row))));
}

void BinderModel::setExpanded(int row, bool expanded)
{
    if (row < 0 || row >= rows.size())
        return;
    const QString key = keyOf(rows.at(row));
    if (expanded == !collapsed.contains(key))
        return;
    if (expanded)
        collapsed.remove(key);
    else
        collapsed.insert(key);
    rebuild();
}

int BinderModel::rowOf(const QString &id) const
{
    for (qsizetype i = 0; i < rows.size(); ++i) {
        if (keyOf(rows.at(i)) == id)
            return static_cast<int>(i);
    }
    return -1;
}

bool BinderModel::isInTrash(const QString &id) const
{
    for (const Node &node : nodes) {
        if (node.id == id)
            return node.section == QLatin1String("trash");
    }
    return false;
}

int BinderModel::nodeOf(const QString &id) const
{
    for (qsizetype i = 0; i < nodes.size(); ++i) {
        if (nodes.at(i).id == id)
            return static_cast<int>(i);
    }
    return -1;
}

void BinderModel::setItemWords(const QString &id, int words)
{
    const int node = nodeOf(id);
    if (node < 0 || nodes.at(node).words == words || !nodes.at(node).children.isEmpty())
        return;
    const int delta = words - nodes.at(node).words;
    QSet<int> changed;
    for (int up = node; up >= 0; up = parents.at(up)) {
        nodes[up].words += delta;
        changed.insert(up);
        // A left out item stops the count at itself.
        if (!nodes.at(up).included)
            break;
    }
    for (qsizetype row = 0; row < rows.size(); ++row) {
        if (changed.contains(rows.at(row).node))
            emit dataChanged(index(static_cast<int>(row)), index(static_cast<int>(row)),
                             { WordsRole });
    }
}

int BinderModel::wordsOf(const QString &id) const
{
    const int node = nodeOf(id);
    return node < 0 ? 0 : nodes.at(node).words;
}

int BinderModel::manuscriptWords() const
{
    int total = 0;
    for (const int root : roots) {
        if (nodes.at(root).section == QLatin1String("manuscript") && nodes.at(root).included)
            total += nodes.at(root).words;
    }
    return total;
}

QString BinderModel::chapterOf(const QString &id) const
{
    const int node = nodeOf(id);
    for (int up = node < 0 ? -1 : parents.at(node); up >= 0; up = parents.at(up)) {
        if (nodes.at(up).kind == QLatin1String("chapter"))
            return nodes.at(up).id;
    }
    return { };
}

QString BinderModel::sectionLabel(const QString &section)
{
    if (section == QLatin1String("front"))
        return QStringLiteral("Front matter");
    if (section == QLatin1String("back"))
        return QStringLiteral("Back matter");
    if (section.isEmpty())
        return { };
    return section.front().toUpper() + section.mid(1);
}

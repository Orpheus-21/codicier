#pragma once

#include "Project.h"

#include <QAbstractListModel>
#include <QList>
#include <QSet>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include <cstdint>

// The visible rows of the binder, in tree order. A row is a section heading
// or an item. Each item row has a depth, so a flat list draws the tree.
class BinderModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("A BinderModel comes from a ProjectSession.")

public:
    enum Role : std::uint16_t {
        IdRole = Qt::UserRole + 1,
        TitleRole,
        KindRole,
        DepthRole,
        HasChildrenRole,
        ExpandedRole,
        IsSectionRole,
        StatusRole,
        WordsRole,
        MissingRole,
    };
    Q_ENUM(Role)

    explicit BinderModel(QObject *parent = nullptr);

    // Takes the items of project and the words of their files.
    void setProject(const Project &project);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Opens or closes the item or the section at row.
    Q_INVOKABLE void toggle(int row);
    Q_INVOKABLE void setExpanded(int row, bool expanded);
    // The row of the item with id, or -1 if the item is hidden or unknown.
    Q_INVOKABLE int rowOf(const QString &id) const;
    // True if the item with id is in the trash section.
    Q_INVOKABLE bool isInTrash(const QString &id) const;
    // Sets the words of a text item and adds the difference to each container
    // above it.
    void setItemWords(const QString &id, int words);
    // The words of the item with id, or 0.
    Q_INVOKABLE int wordsOf(const QString &id) const;
    // The words of all items in the manuscript section.
    int manuscriptWords() const;
    // The id of the nearest chapter above the item, or "".
    QString chapterOf(const QString &id) const;
    // The section label for a section key, such as "Front matter".
    static QString sectionLabel(const QString &section);

private:
    struct Node
    {
        QString id;
        QString title;
        QString kind;
        QString section;
        QString status;
        QList<int> children;
        int words = 0;
        bool missing = false;
    };
    struct Row
    {
        int node = -1;
        // For a section heading: its key, such as "manuscript".
        QString section;
        int depth = 0;
    };

    QList<Node> nodes;
    QList<int> roots;
    QList<Row> rows;
    QSet<QString> collapsed;
    // The open and closed state survives a change in the same project.
    QString folder;

    QString keyOf(const Row &row) const;
    int nodeOf(const QString &id) const;
    QList<int> parents;
    int sumWords(int node);
    void appendBranch(int node, int depth);
    void rebuild();
};

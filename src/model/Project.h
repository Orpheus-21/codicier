#pragma once

#include <QList>
#include <QString>

#include <toml++/toml.hpp>

// One entry of binder.toml. docs/format.md defines the keys.
struct ProjectItem
{
    QString id;
    QString kind;
    QString title;
    // The section of the branch, also for items below the top level.
    QString section;
    // The path relative to the project folder, for text and file items.
    QString file;
    // The index of the parent in Project::items, or -1 at the top level.
    int parent = -1;
    QList<int> children;
    // The item names a file that does not exist. The item stays in the tree.
    bool missingFile = false;
    // The line of the [[item]] header in binder.toml. A copy of a TOML table
    // loses its lines, so the loader keeps this one.
    int line = 0;
    // Every key as read, so a save keeps the keys that the app does not know.
    toml::table data;
};

struct Project
{
    QString folder;
    QString title;
    QString kind;
    // The format is newer than the app knows, so the app must not save.
    bool readOnly = false;
    // The keys of project.toml.
    toml::table data;
    // The keys at the top of binder.toml, without the items.
    toml::table binderData;
    // In the order of binder.toml.
    QList<ProjectItem> items;
    // The indexes of the top level items, in binder order.
    QList<int> roots;
};

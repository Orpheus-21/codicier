#pragma once

#include <QList>
#include <QString>

// The plot frameworks of the beat board, from data/frameworks.toml in the
// resources of the app.
namespace Frameworks {

struct Beat
{
    QString id;
    QString name;
    QString description;
    // Where the beat usually falls, as a part of the whole length, 0 to 1.
    double at = 0;
};

struct Framework
{
    QString id;
    QString name;
    QList<Beat> beats;
};

// Every framework, in the order of the file.
const QList<Framework> &all();

} // namespace Frameworks

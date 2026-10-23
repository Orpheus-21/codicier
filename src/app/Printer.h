#pragma once

#include <QObject>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QPagedPaintDevice;

// Prints chapters to a printer or a PDF file with the font and the size of
// the page. A page break comes before each chapter, the scene break mark
// between scenes, and a page number at the foot of each page. The app does
// no other typesetting.
class Printer : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    // The printers of the system, and the default one.
    Q_INVOKABLE QStringList printers() const;
    Q_INVOKABLE QString defaultPrinter() const;
    // Prints chapters (see ProjectSession::printChapters) on the printer
    // with that name, or with a pdf file, into it. Returns an error text, or "".
    Q_INVOKABLE QString print(const QVariantList &chapters, const QString &family, int pixelSize,
                              const QString &sceneBreak, const QString &printer,
                              const QUrl &pdf) const;
    // The number of pages that print gives, for a check before printing.
    Q_INVOKABLE int pageCount(const QVariantList &chapters, const QString &family, int pixelSize,
                              const QString &sceneBreak) const;
};

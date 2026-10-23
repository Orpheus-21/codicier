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
    // The printers of the system and the default one, as the last call to
    // findPrinters found them. The system can take a second to answer, so
    // the question runs on a worker thread and the answer comes later.
    Q_PROPERTY(QStringList printers MEMBER foundPrinters NOTIFY printersFound)
    Q_PROPERTY(QString defaultPrinter MEMBER foundDefault NOTIFY printersFound)
    Q_PROPERTY(bool searching MEMBER busy NOTIFY printersFound)

    Q_INVOKABLE void findPrinters();
    // Prints chapters (see ProjectSession::printChapters) on the printer
    // with that name, or with a pdf file, into it. Returns an error text, or "".
    Q_INVOKABLE QString print(const QVariantList &chapters, const QString &family, int pixelSize,
                              const QString &sceneBreak, const QString &printer,
                              const QUrl &pdf) const;
    // The number of pages that print gives, for a check before printing.
    Q_INVOKABLE int pageCount(const QVariantList &chapters, const QString &family, int pixelSize,
                              const QString &sceneBreak) const;

signals:
    void printersFound();

private:
    QStringList foundPrinters;
    QString foundDefault;
    bool busy = false;
};

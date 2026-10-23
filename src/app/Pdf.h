#pragma once

#include <QImage>
#include <QObject>
#include <QQuickPaintedItem>
#include <QSizeF>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

// The PDF files of the research section, read with poppler-qt6. A build
// without poppler-qt6 says so with available, and a PDF then opens in the
// app of the system.
class Pdf : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool available READ available CONSTANT)

public:
    bool available() const;
    // The number of pages, or 0 for a file that does not open.
    Q_INVOKABLE int pageCount(const QUrl &file) const;
    // The size of a page in points, or an empty size.
    Q_INVOKABLE QSizeF pageSize(const QUrl &file, int page) const;
};

// One page of a PDF, drawn at the size of the item.
class PdfPage : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QUrl file MEMBER file NOTIFY changed)
    Q_PROPERTY(int page MEMBER page NOTIFY changed)
    // True when the page is drawn.
    Q_PROPERTY(bool ready READ ready NOTIFY drawn)

public:
    explicit PdfPage(QQuickItem *parent = nullptr);
    void paint(QPainter *painter) override;
    bool ready() const { return !image.isNull(); }

signals:
    void changed();
    void drawn();

private:
    QUrl file;
    int page = 0;
    QImage image;
};

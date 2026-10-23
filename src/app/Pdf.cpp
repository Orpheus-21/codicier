#include "Pdf.h"

#include <QPainter>
#include <QQuickWindow>

#ifdef CODICIER_PDF
#include <QHash>
#include <poppler-qt6.h>

#include <memory>

namespace {

// The open documents, by path. The QML items use them on the GUI thread
// only, so no lock is needed.
QHash<QString, std::shared_ptr<Poppler::Document>> documents;

std::shared_ptr<Poppler::Document> open(const QString &path)
{
    if (const auto found = documents.constFind(path); found != documents.cend())
        return *found;
    std::shared_ptr<Poppler::Document> document = Poppler::Document::load(path);
    if (document && document->isLocked())
        document.reset();
    if (document) {
        document->setRenderHint(Poppler::Document::Antialiasing);
        document->setRenderHint(Poppler::Document::TextAntialiasing);
    }
    documents.insert(path, document);
    return document;
}

std::unique_ptr<Poppler::Page> pageOf(const QUrl &file, int page)
{
    const std::shared_ptr<Poppler::Document> document = open(file.toLocalFile());
    if (!document || page < 0 || page >= document->numPages())
        return { };
    return document->page(page);
}

} // namespace

bool Pdf::available() const
{
    return true;
}

int Pdf::pageCount(const QUrl &file) const
{
    const std::shared_ptr<Poppler::Document> document = open(file.toLocalFile());
    return document ? document->numPages() : 0;
}

QSizeF Pdf::pageSize(const QUrl &file, int page) const
{
    const std::unique_ptr<Poppler::Page> one = pageOf(file, page);
    return one ? one->pageSizeF() : QSizeF();
}

void PdfPage::paint(QPainter *painter)
{
    const QSize pixels = (size() * window()->devicePixelRatio()).toSize();
    if (image.size() != pixels) {
        image = { };
        // ponytail: the page draws on the GUI thread, a QQuickAsyncImageProvider
        // if long PDFs ever stutter on scroll.
        if (const std::unique_ptr<Poppler::Page> one = pageOf(file, page);
            one && one->pageSizeF().width() > 0 && pixels.width() > 0) {
            const double dpi = pixels.width() * 72.0 / one->pageSizeF().width();
            image = one->renderToImage(dpi, dpi);
            image.setDevicePixelRatio(window()->devicePixelRatio());
        }
        emit drawn();
    }
    if (!image.isNull())
        painter->drawImage(QRectF(QPointF(0, 0), size()), image);
}

#else

bool Pdf::available() const
{
    return false;
}

int Pdf::pageCount(const QUrl &) const
{
    return 0;
}

QSizeF Pdf::pageSize(const QUrl &, int) const
{
    return { };
}

void PdfPage::paint(QPainter *) { }

#endif

PdfPage::PdfPage(QQuickItem *parent) : QQuickPaintedItem(parent)
{
    // A new file or page draws again.
    connect(this, &PdfPage::changed, this, [this] {
        image = { };
        update();
    });
}

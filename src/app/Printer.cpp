#include "Printer.h"

#include <QFile>
#include <QLocale>
#include <QPageLayout>
#include <QPdfWriter>
#include <QPrinter>
#include <QPrinterInfo>
#include <QTextCursor>
#include <QTextDocument>

#include <memory>

namespace {

// The paper of the place: Letter in the United States, else A4. The
// margins are 25 mm on each side.
QPageLayout pageLayout()
{
    const bool letter = QLocale::system().measurementSystem() == QLocale::ImperialUSSystem;
    return { QPageSize(letter ? QPageSize::Letter : QPageSize::A4), QPageLayout::Portrait,
             QMarginsF(25, 25, 25, 25), QPageLayout::Millimeter };
}

std::unique_ptr<QTextDocument> layOut(const QVariantList &chapters, const QString &family,
                                      int pixelSize, const QString &sceneBreak)
{
    auto document = std::make_unique<QTextDocument>();
    // The page shows pixels; paper takes points, 3 points to 4 pixels.
    QFont font(family);
    font.setPointSizeF(pixelSize * 0.75);
    document->setDefaultFont(font);
    QTextCursor cursor(document.get());
    bool first = true;
    for (const QVariant &each : chapters) {
        const QVariantMap chapter = each.toMap();
        QTextBlockFormat heading;
        heading.setAlignment(Qt::AlignHCenter);
        heading.setBottomMargin(font.pointSizeF() * 2);
        if (!first) {
            heading.setPageBreakPolicy(QTextFormat::PageBreak_AlwaysBefore);
            cursor.insertBlock();
        }
        cursor.setBlockFormat(heading);
        QTextCharFormat title;
        title.setFontPointSize(font.pointSizeF() * 1.4);
        title.setFontWeight(QFont::DemiBold);
        cursor.insertText(chapter.value(QStringLiteral("title")).toString(), title);
        const QStringList texts = chapter.value(QStringLiteral("texts")).toStringList();
        for (qsizetype i = 0; i < texts.size(); ++i) {
            if (i > 0) {
                // The scene break mark, or a blank line when the mark is "".
                QTextBlockFormat mark;
                mark.setAlignment(Qt::AlignHCenter);
                mark.setTopMargin(font.pointSizeF());
                mark.setBottomMargin(font.pointSizeF());
                cursor.insertBlock(mark, QTextCharFormat());
                cursor.insertText(sceneBreak.isEmpty() ? QStringLiteral(" ") : sceneBreak);
            }
            cursor.insertBlock(QTextBlockFormat(), QTextCharFormat());
            cursor.insertMarkdown(texts.at(i));
        }
        first = false;
    }
    return document;
}

} // namespace

QStringList Printer::printers() const
{
    return QPrinterInfo::availablePrinterNames();
}

QString Printer::defaultPrinter() const
{
    return QPrinterInfo::defaultPrinterName();
}

QString Printer::print(const QVariantList &chapters, const QString &family, int pixelSize,
                       const QString &sceneBreak, const QString &printer, const QUrl &pdf) const
{
    if (chapters.isEmpty())
        return QStringLiteral("Nothing to print: the chapter has no text in the manuscript.");
    const std::unique_ptr<QTextDocument> document = layOut(chapters, family, pixelSize, sceneBreak);
    if (pdf.isValid() && !pdf.isEmpty()) {
        QPdfWriter writer(pdf.toLocalFile());
        writer.setPageLayout(pageLayout());
        writer.setResolution(300);
        writer.setTitle(chapters.first().toMap().value(QStringLiteral("title")).toString());
        document->print(&writer);
        return QFile::exists(pdf.toLocalFile()) ? QString()
                                                : QStringLiteral("Cannot write the PDF file.");
    }
    QPrinter paper(QPrinter::HighResolution);
    paper.setPrinterName(printer);
    if (!paper.isValid())
        return QStringLiteral("The printer %1 is not ready.").arg(printer);
    paper.setPageLayout(pageLayout());
    document->print(&paper);
    return { };
}

int Printer::pageCount(const QVariantList &chapters, const QString &family, int pixelSize,
                       const QString &sceneBreak) const
{
    const std::unique_ptr<QTextDocument> document = layOut(chapters, family, pixelSize, sceneBreak);
    // The same page size as print, in the points of the document.
    const QRectF page = pageLayout().paintRect(QPageLayout::Point);
    document->setPageSize(page.size());
    return document->pageCount();
}

#include "Rtf.h"

#include <QList>
#include <QRegularExpression>
#include <QStringDecoder>

namespace {

struct Run
{
    QString text;
    bool bold = false;
    bool italic = false;
};

struct State
{
    bool bold = false;
    bool italic = false;
    // The group is a destination that holds no text, such as a font table.
    bool skip = false;
    // The number of fallback characters after \uN.
    int unicodeSkip = 1;
};

// RTF control words are ASCII letters and numbers, whatever the locale.
bool isLetter(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool isDigit(char c)
{
    return c >= '0' && c <= '9';
}

// The groups that hold no text of the document.
bool isDestination(const QByteArray &word)
{
    static const QList<QByteArray> words { "fonttbl",
                                           "colortbl",
                                           "expandedcolortbl",
                                           "stylesheet",
                                           "info",
                                           "pict",
                                           "header",
                                           "footer",
                                           "fldinst",
                                           "listtable",
                                           "listoverridetable",
                                           "NeXTGraphic" };
    return words.contains(word);
}

// Markdown marks in the text stay text.
QString escapedText(const QString &text)
{
    QString out;
    for (const QChar c : text) {
        if (c == u'*' || c == u'_' || c == u'\\' || c == u'`')
            out += u'\\';
        out += c;
    }
    return out;
}

// One paragraph: runs of the same style join, and the marks wrap the words
// but not the spaces around them.
QString render(const QList<Run> &runs)
{
    QList<Run> merged;
    for (const Run &run : runs) {
        if (!merged.isEmpty() && merged.last().bold == run.bold
            && merged.last().italic == run.italic)
            merged.last().text += run.text;
        else
            merged.append(run);
    }
    // The style markers of Scrivener stay out of the text.
    static const QRegularExpression markers(QStringLiteral("<!?\\$Scr_[^>]*>"));
    for (Run &run : merged)
        run.text.remove(markers);
    QString out;
    for (const Run &run : std::as_const(merged)) {
        const QString core = run.text.trimmed();
        if (core.isEmpty() || (!run.bold && !run.italic)) {
            out += escapedText(run.text);
            continue;
        }
        const qsizetype lead = run.text.indexOf(core);
        const QString mark = run.bold && run.italic ? QStringLiteral("***")
            : run.bold                              ? QStringLiteral("**")
                                                    : QStringLiteral("*");
        out += run.text.left(lead) + mark + escapedText(core) + mark
            + run.text.mid(lead + core.size());
    }
    return out.trimmed();
}

} // namespace

namespace Rtf {

QString toMarkdown(const QByteArray &rtf)
{
    QStringDecoder cp1252("windows-1252");
    QList<State> stack { State() };
    QList<QString> paragraphs;
    QList<Run> runs;
    const auto add = [&](const QString &text) {
        const State &state = stack.last();
        if (state.skip || text.isEmpty())
            return;
        runs.append({ text, state.bold, state.italic });
    };
    const auto endParagraph = [&] {
        paragraphs.append(render(runs));
        runs.clear();
    };
    int fallback = 0;
    const qsizetype size = rtf.size();
    for (qsizetype at = 0; at < size; ++at) {
        const char c = rtf.at(at);
        if (c == '{') {
            stack.append(stack.last());
            // {\* starts a group that an old reader may skip: no text in it.
            if (rtf.mid(at + 1, 2) == "\\*")
                stack.last().skip = true;
            continue;
        }
        if (c == '}') {
            if (stack.size() > 1)
                stack.removeLast();
            continue;
        }
        if (c == '\r' || c == '\n')
            continue;
        if (c != '\\') {
            if (fallback > 0) {
                --fallback;
                continue;
            }
            // The text bytes are code page 1252 in Scrivener files.
            add(cp1252.decode(QByteArray(1, c)));
            continue;
        }
        // A control symbol: \ and one character that is not a letter.
        if (at + 1 >= size)
            break;
        const char next = rtf.at(at + 1);
        if (!isLetter(next)) {
            ++at;
            if (next == '\'' && at + 2 < size) {
                const QByteArray hex = rtf.mid(at + 1, 2);
                at += 2;
                if (fallback > 0) {
                    --fallback;
                    continue;
                }
                add(cp1252.decode(QByteArray(1, static_cast<char>(hex.toInt(nullptr, 16)))));
            } else if (next == '\n' || next == '\r') {
                // Cocoa writes a paragraph end as a backslash at the end of a line.
                endParagraph();
            } else if (next == '~') {
                add(QString(QChar(0x00a0)));
            } else if (next == '_') {
                add(QStringLiteral("-"));
            } else if (next == '\\' || next == '{' || next == '}') {
                add(QString(QChar::fromLatin1(next)));
            }
            continue;
        }
        // A control word: \ letters, an optional number, and an optional space.
        qsizetype end = at + 1;
        while (end < size && isLetter(rtf.at(end)))
            ++end;
        const QByteArray word = rtf.mid(at + 1, end - at - 1);
        qsizetype digits = end;
        if (digits < size && rtf.at(digits) == '-')
            ++digits;
        while (digits < size && isDigit(rtf.at(digits)))
            ++digits;
        const bool hasNumber = digits > end && !(digits == end + 1 && rtf.at(end) == '-');
        const int number = hasNumber ? rtf.mid(end, digits - end).toInt() : 1;
        // The loop steps past the space that ends the word, or onto the next byte.
        at = (digits < size && rtf.at(digits) == ' ') ? digits : digits - 1;
        State &state = stack.last();
        if (isDestination(word)) {
            state.skip = true;
        } else if (word == "par") {
            endParagraph();
        } else if (word == "line") {
            add(QString(QChar(0x2028)));
        } else if (word == "tab") {
            add(QStringLiteral(" "));
        } else if (word == "b") {
            state.bold = number != 0;
        } else if (word == "i") {
            state.italic = number != 0;
        } else if (word == "plain") {
            state.bold = false;
            state.italic = false;
        } else if (word == "uc") {
            state.unicodeSkip = number;
        } else if (word == "u") {
            add(QString(QChar(static_cast<char16_t>(number < 0 ? number + 65536 : number))));
            fallback = state.unicodeSkip;
        } else if (word == "emdash") {
            add(QString(QChar(0x2014)));
        } else if (word == "endash") {
            add(QString(QChar(0x2013)));
        } else if (word == "lquote") {
            add(QString(QChar(0x2018)));
        } else if (word == "rquote") {
            add(QString(QChar(0x2019)));
        } else if (word == "ldblquote") {
            add(QString(QChar(0x201c)));
        } else if (word == "rdblquote") {
            add(QString(QChar(0x201d)));
        } else if (word == "bullet") {
            add(QString(QChar(0x2022)));
        }
    }
    endParagraph();
    QStringList kept;
    for (QString paragraph : std::as_const(paragraphs)) {
        // A line break inside a paragraph is a Markdown hard break.
        paragraph.replace(QChar(0x2028), QStringLiteral("\\\n"));
        if (!paragraph.trimmed().isEmpty())
            kept.append(paragraph.trimmed());
    }
    return kept.join(QStringLiteral("\n\n"));
}

} // namespace Rtf

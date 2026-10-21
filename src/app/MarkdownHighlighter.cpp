#include "MarkdownHighlighter.h"

#include "SpellChecker.h"

#include <QColor>
#include <QFont>
#include <QTextBlock>

#include <algorithm>

MarkdownHighlighter::MarkdownHighlighter(QTextDocument *document)
    : QSyntaxHighlighter(document), heading(QStringLiteral("^(#{1,6})\\s")),
      strong(QStringLiteral("\\*\\*(?=\\S)(.+?)(?<=\\S)\\*\\*")),
      emphasis(QStringLiteral("(?<![*\\w])[*_](?=[^\\s*_])(.+?)(?<=[^\\s*_])[*_](?![*\\w])")),
      footnote(QStringLiteral("\\[\\^[^\\]\\s]+\\]")),
      // A link to an entity of the codex: [[Mara Vell]].
      link(QStringLiteral("\\[\\[[^\\[\\]\\n]+\\]\\]")),
      word(QStringLiteral("\\p{L}+(?:['’]\\p{L}+)*"),
           QRegularExpression::UseUnicodePropertiesOption)
{
    // The light theme colours until the app sets the theme.
    markFormat.setForeground(QColor(0x70, 0x6b, 0x65));
    strongFormat.setFontWeight(QFont::Bold);
    emphasisFormat.setFontItalic(true);
    footnoteFormat.setForeground(QColor(0x94, 0x55, 0x2a));
    footnoteFormat.setVerticalAlignment(QTextCharFormat::AlignSuperScript);
    linkFormat.setForeground(QColor(0x94, 0x55, 0x2a));
    styleFormat.setBackground(QColor(0x94, 0x55, 0x2a, 0x30));
}

void MarkdownHighlighter::setColors(const QColor &mark, const QColor &accent)
{
    if (markFormat.foreground().color() == mark && footnoteFormat.foreground().color() == accent)
        return;
    markFormat.setForeground(mark);
    footnoteFormat.setForeground(accent);
    linkFormat.setForeground(accent);
    QColor tint = accent;
    tint.setAlpha(0x30);
    styleFormat.setBackground(tint);
    rehighlight();
}

void MarkdownHighlighter::highlightBlock(const QString &text)
{
    hideHere = hideMarks && currentBlock().blockNumber() != shownBlock;
    styleBlock(text);
    dimOutsideFocus(text);
}

void MarkdownHighlighter::dimOutsideFocus(const QString &text)
{
    if (focusScope == 0 || shownBlock < 0)
        return;
    // The bright part: nothing, the whole block, or the sentence of the cursor.
    qsizetype from = 0;
    qsizetype to = 0;
    if (currentBlock().blockNumber() == shownBlock) {
        to = text.size();
        if (focusScope == 2) {
            const auto ends = [&text](qsizetype at) {
                return QStringLiteral(".!?\u2026").contains(text.at(at));
            };
            const qsizetype cursor = std::clamp<qsizetype>(shownOffset, 0, text.size());
            for (qsizetype at = cursor - 1; at > 0; --at) {
                if (ends(at - 1) && text.at(at).isSpace()) {
                    from = at + 1;
                    break;
                }
            }
            for (qsizetype at = cursor; at < text.size(); ++at) {
                if (ends(at)) {
                    to = at + 1;
                    break;
                }
            }
        }
    }
    for (qsizetype at = 0; at < text.size(); ++at) {
        if (at >= from && at < to)
            continue;
        QTextCharFormat dim = format(static_cast<int>(at));
        if (dim.foreground().color().alpha() == 0)
            continue;
        dim.setForeground(markFormat.foreground());
        setFormat(static_cast<int>(at), 1, dim);
    }
}

void MarkdownHighlighter::styleBlock(const QString &text)
{
    const QRegularExpressionMatch match = heading.match(text);
    if (match.hasMatch()) {
        // Level 1 is the largest. Each level down is a little smaller.
        const auto level = static_cast<int>(match.capturedLength(1));
        QTextCharFormat format;
        format.setFontWeight(QFont::DemiBold);
        // QML sets the size in pixels, and then the point size is -1.
        const QFont base = document()->defaultFont();
        const double scale = 1.0 + 0.12 * (7 - level);
        if (base.pixelSize() > 0)
            format.setProperty(QTextFormat::FontPixelSize, qRound(base.pixelSize() * scale));
        else
            format.setFontPointSize(base.pointSizeF() * scale);
        setFormat(0, static_cast<int>(text.size()), format);
        // A hidden mark takes the space after it too.
        setFormat(0, hideHere ? static_cast<int>(match.capturedLength()) : level, markOver(format));
        return;
    }
    styleSpans(text, strong, 2, strongFormat);
    styleSpans(text, emphasis, 1, emphasisFormat);
    styleSpans(text, footnote, 0, footnoteFormat);
    styleSpans(text, link, 2, linkFormat);
    tintPasses(text);
    // The comments: a dotted line in the accent color under the quote.
    const int first = currentBlock().position();
    for (const auto &[start, length] : std::as_const(commentRanges)) {
        const int from = std::max(start, first);
        const int to = std::min(start + length, static_cast<int>(first + text.size()));
        if (from >= to)
            continue;
        QTextCharFormat merged = format(from - first);
        merged.setUnderlineStyle(QTextCharFormat::DotLine);
        merged.setUnderlineColor(linkFormat.foreground().color());
        setFormat(from - first, to - from, merged);
    }
    checkSpelling(text);
    checkStyle(text);
}

void MarkdownHighlighter::setPasses(const QList<PassRange> &ranges, const QList<QColor> &colors)
{
    if (ranges.isEmpty() && passRanges.isEmpty())
        return;
    passRanges = ranges;
    passColors = colors;
    rehighlight();
}

void MarkdownHighlighter::setComments(const QList<std::pair<int, int>> &ranges)
{
    if (ranges == commentRanges)
        return;
    commentRanges = ranges;
    rehighlight();
}

void MarkdownHighlighter::tintPasses(const QString &text)
{
    if (passRanges.isEmpty() || passColors.isEmpty())
        return;
    const int first = currentBlock().position();
    const auto last = static_cast<int>(first + text.size());
    for (const PassRange &range : std::as_const(passRanges)) {
        const int start = std::max(range.start, first);
        const int end = std::min(range.start + range.length, last);
        if (start >= end)
            continue;
        QColor tint = passColors.at((range.pass - 1) % passColors.size());
        tint.setAlpha(0x38);
        QTextCharFormat merged = format(start - first);
        merged.setBackground(tint);
        setFormat(start - first, end - start, merged);
    }
}

void MarkdownHighlighter::setStyleChecks(int checks)
{
    const auto flags = static_cast<unsigned>(std::max(0, checks));
    if (styleChecks == flags)
        return;
    styleChecks = flags;
    rehighlight();
}

void MarkdownHighlighter::checkStyle(const QString &text)
{
    if (styleChecks == 0U)
        return;
    // Words that tell the reader what to think, or say nothing.
    static const QStringList filters { "just",     "really",   "very",      "quite",    "rather",
                                       "somewhat", "actually", "basically", "simply",   "literally",
                                       "suddenly", "felt",     "seemed",    "realized", "realised",
                                       "noticed",  "wondered", "began",     "started",  "decided" };
    // Words that end in -ly but are not adverbs.
    static const QStringList notAdverbs { "only",   "family", "reply",   "early",    "likely",
                                          "holy",   "ugly",   "belly",   "ally",     "rally",
                                          "supply", "apply",  "daily",   "friendly", "lonely",
                                          "lovely", "silly",  "elderly", "costly",   "deadly",
                                          "lily",   "jelly" };
    static const QRegularExpression passive(
        QStringLiteral("\\b(am|is|are|was|were|be|been|being)\\s+(\\w+ly\\s+)?"
                       "(\\w+ed|taken|given|seen|done|made|written|broken|chosen|driven|"
                       "eaten|forgotten|hidden|known|shown|spoken|stolen|thrown|born|built|"
                       "caught|found|held|kept|left|lost|paid|sent|sold|told|brought|bought)\\b"),
        QRegularExpression::CaseInsensitiveOption);
    const auto tint = [this](qsizetype start, qsizetype length) {
        QTextCharFormat merged = format(static_cast<int>(start));
        merged.setBackground(styleFormat.background());
        setFormat(static_cast<int>(start), static_cast<int>(length), merged);
    };
    QList<QRegularExpressionMatch> words;
    auto matches = word.globalMatch(text);
    while (matches.hasNext())
        words.append(matches.next());
    for (qsizetype i = 0; i < words.size(); ++i) {
        const QString lower = words.at(i).captured().toLower();
        const qsizetype start = words.at(i).capturedStart();
        const qsizetype length = words.at(i).capturedLength();
        if (has(Filters) && filters.contains(lower))
            tint(start, length);
        if (has(Adverbs) && lower.size() > 4 && lower.endsWith(QStringLiteral("ly"))
            && !notAdverbs.contains(lower))
            tint(start, length);
        // The same word again in the next ten words: "the the", or an echo
        // of a long word.
        if (has(Repeats)) {
            for (qsizetype j = i + 1; j < std::min(words.size(), i + 11); ++j) {
                const bool close = j == i + 1 || lower.size() >= 5;
                if (close && words.at(j).captured().toLower() == lower) {
                    tint(start, length);
                    tint(words.at(j).capturedStart(), words.at(j).capturedLength());
                }
            }
        }
    }
    if (has(Passive)) {
        auto found = passive.globalMatch(text);
        while (found.hasNext()) {
            const QRegularExpressionMatch match = found.next();
            tint(match.capturedStart(), match.capturedLength());
        }
    }
}

void MarkdownHighlighter::setSpellChecker(const SpellChecker *checker)
{
    if (spell == checker)
        return;
    spell = checker;
    rehighlight();
}

void MarkdownHighlighter::checkSpelling(const QString &text)
{
    if (spell == nullptr)
        return;
    // A link names an entity, and a footnote mark is not a word.
    QList<std::pair<qsizetype, qsizetype>> skip;
    for (const QRegularExpression *pattern : { &link, &footnote }) {
        auto matches = pattern->globalMatch(text);
        while (matches.hasNext()) {
            const QRegularExpressionMatch match = matches.next();
            skip.append({ match.capturedStart(), match.capturedEnd() });
        }
    }
    auto matches = word.globalMatch(text);
    while (matches.hasNext()) {
        const QRegularExpressionMatch match = matches.next();
        const qsizetype start = match.capturedStart();
        if (std::ranges::any_of(skip, [start](const auto &range) {
                return start >= range.first && start < range.second;
            }))
            continue;
        if (spell->isCorrect(match.captured()))
            continue;
        QTextCharFormat merged = format(static_cast<int>(start));
        merged.setUnderlineStyle(QTextCharFormat::SpellCheckUnderline);
        merged.setUnderlineColor(linkFormat.foreground().color());
        setFormat(static_cast<int>(start), static_cast<int>(match.capturedLength()), merged);
    }
}

void MarkdownHighlighter::setHideMarks(bool hide)
{
    if (hide == hideMarks)
        return;
    hideMarks = hide;
    rehighlight();
}

void MarkdownHighlighter::setFocusScope(int scope)
{
    if (scope == focusScope)
        return;
    focusScope = scope;
    rehighlight();
}

void MarkdownHighlighter::setShownBlock(int block, int offset)
{
    if (block == shownBlock && (offset == shownOffset || focusScope != 2))
        return;
    const int before = shownBlock;
    shownBlock = block;
    shownOffset = offset;
    if (!hideMarks && focusScope == 0)
        return;
    // In focus mode with no paragraph, every block changes.
    if (focusScope != 0 && (before < 0 || block < 0)) {
        rehighlight();
        return;
    }
    for (const int number : { before, block }) {
        const QTextBlock changed = document()->findBlockByNumber(number);
        if (changed.isValid())
            rehighlightBlock(changed);
    }
}

QTextCharFormat MarkdownHighlighter::markOver(QTextCharFormat base) const
{
    if (!hideHere) {
        base.setForeground(markFormat.foreground());
        return base;
    }
    base.setForeground(Qt::transparent);
    base.setProperty(QTextFormat::FontPixelSize, 1);
    return base;
}

void MarkdownHighlighter::styleSpans(const QString &text, const QRegularExpression &pattern,
                                     int markLength, const QTextCharFormat &body)
{
    auto matches = pattern.globalMatch(text);
    while (matches.hasNext()) {
        const QRegularExpressionMatch match = matches.next();
        const auto start = static_cast<int>(match.capturedStart());
        const auto length = static_cast<int>(match.capturedLength());
        // Merge, so a span inside a strong span stays strong.
        QTextCharFormat merged = format(start);
        merged.merge(body);
        setFormat(start, length, merged);
        if (markLength > 0) {
            const QTextCharFormat mark = markOver(merged);
            setFormat(start, markLength, mark);
            setFormat(start + length - markLength, markLength, mark);
        }
    }
}

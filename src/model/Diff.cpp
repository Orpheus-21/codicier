#include "Diff.h"

#include <QStringView>

#include <algorithm>
#include <vector>

namespace {

// ponytail: an LCS table of the changed middle; above this many cells the
// middle counts as one removed and one added block. Myers' diff can lift the
// limit if long rewrites need finer output.
constexpr qsizetype maxCells = 4'000'000;

// Words (letters, digits, and inner apostrophes), whitespace runs, and
// single other marks.
QList<QStringView> tokens(QStringView text)
{
    QList<QStringView> out;
    qsizetype i = 0;
    while (i < text.size()) {
        const qsizetype start = i;
        const QChar c = text.at(i);
        if (c.isLetterOrNumber()) {
            while (i < text.size()
                   && (text.at(i).isLetterOrNumber()
                       || ((text.at(i) == u'\'' || text.at(i) == u'’') && i + 1 < text.size()
                           && text.at(i + 1).isLetterOrNumber())))
                ++i;
        } else if (c.isSpace()) {
            while (i < text.size() && text.at(i).isSpace())
                ++i;
        } else {
            ++i;
        }
        out.append(text.sliced(start, i - start));
    }
    return out;
}

void push(QList<Diff::Segment> &out, Diff::Segment::Kind kind, QStringView text)
{
    if (text.isEmpty())
        return;
    if (!out.isEmpty() && out.last().kind == kind)
        out.last().text += text;
    else
        out.append({ kind, text.toString() });
}

} // namespace

namespace Diff {

QList<Segment> words(const QString &before, const QString &after)
{
    const QList<QStringView> a = tokens(before);
    const QList<QStringView> b = tokens(after);
    qsizetype head = 0;
    while (head < a.size() && head < b.size() && a.at(head) == b.at(head))
        ++head;
    qsizetype tail = 0;
    while (tail < a.size() - head && tail < b.size() - head
           && a.at(a.size() - 1 - tail) == b.at(b.size() - 1 - tail))
        ++tail;

    QList<Segment> out;
    for (qsizetype i = 0; i < head; ++i)
        push(out, Segment::Same, a.at(i));

    const qsizetype n = a.size() - head - tail;
    const qsizetype m = b.size() - head - tail;
    if (n * m > maxCells) {
        for (qsizetype i = 0; i < n; ++i)
            push(out, Segment::Removed, a.at(head + i));
        for (qsizetype j = 0; j < m; ++j)
            push(out, Segment::Added, b.at(head + j));
    } else {
        // length[i][j]: the longest common run of a[i..] and b[j..].
        std::vector<int> length(static_cast<std::size_t>((n + 1) * (m + 1)), 0);
        const auto at
            = [m](qsizetype i, qsizetype j) { return static_cast<std::size_t>(i * (m + 1) + j); };
        for (qsizetype i = n - 1; i >= 0; --i) {
            for (qsizetype j = m - 1; j >= 0; --j) {
                length[at(i, j)] = a.at(head + i) == b.at(head + j)
                    ? length[at(i + 1, j + 1)] + 1
                    : std::max(length[at(i + 1, j)], length[at(i, j + 1)]);
            }
        }
        qsizetype i = 0;
        qsizetype j = 0;
        while (i < n || j < m) {
            if (i < n && j < m && a.at(head + i) == b.at(head + j)) {
                push(out, Segment::Same, a.at(head + i));
                ++i;
                ++j;
            } else if (i < n && (j == m || length[at(i + 1, j)] >= length[at(i, j + 1)])) {
                // On a tie the removed words come first: old, then new.
                push(out, Segment::Removed, a.at(head + i));
                ++i;
            } else {
                push(out, Segment::Added, b.at(head + j));
                ++j;
            }
        }
    }

    for (qsizetype i = a.size() - tail; i < a.size(); ++i)
        push(out, Segment::Same, a.at(i));
    return out;
}

QList<int> origins(const QStringList &versions)
{
    if (versions.isEmpty())
        return { };
    QList<int> origin(versions.first().size(), 0);
    for (qsizetype k = 1; k < versions.size(); ++k) {
        QList<int> next;
        next.reserve(versions.at(k).size());
        qsizetype before = 0;
        // The same text keeps its origin. Added text has this version.
        for (const Segment &segment : words(versions.at(k - 1), versions.at(k))) {
            if (segment.kind == Segment::Removed) {
                before += segment.text.size();
            } else if (segment.kind == Segment::Added) {
                next.append(QList<int>(segment.text.size(), static_cast<int>(k)));
            } else {
                for (qsizetype i = 0; i < segment.text.size(); ++i)
                    next.append(before + i < origin.size() ? origin.at(before + i) : 0);
                before += segment.text.size();
            }
        }
        origin = next;
    }
    return origin;
}

} // namespace Diff

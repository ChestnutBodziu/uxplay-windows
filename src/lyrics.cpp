#include "lyrics.h"

#include <QRegularExpression>
#include <QStringList>
#include <algorithm>

int Lyrics::indexAt(qint64 positionMs) const {
    if (!synced || lines.isEmpty()) {
        return -1;
    }
    // Last line whose time is <= positionMs (lines are sorted by time).
    const auto it = std::upper_bound(
        lines.begin(), lines.end(), positionMs,
        [](qint64 value, const LyricLine &line) { return value < line.timeMs; });
    return int(it - lines.begin()) - 1;
}

Lyrics Lyrics::fromLrc(const QString &lrc) {
    static const QRegularExpression timestamp(
        QStringLiteral(R"(\[(\d{1,3}):(\d{2})(?:[.:](\d{1,3}))?\])"));

    Lyrics result;
    result.synced = true;
    const QStringList rows = lrc.split(QLatin1Char('\n'));
    for (const QString &rawRow : rows) {
        const QString row = rawRow.trimmed();
        if (row.isEmpty()) {
            continue;
        }
        QVector<qint64> times;
        qsizetype textStart = 0;
        auto it = timestamp.globalMatch(row);
        while (it.hasNext()) {
            const QRegularExpressionMatch match = it.next();
            if (match.capturedStart() != textStart) {
                break; // timestamps must be at the start of the line
            }
            const qint64 minutes = match.captured(1).toLongLong();
            const qint64 seconds = match.captured(2).toLongLong();
            qint64 millis = 0;
            if (!match.captured(3).isEmpty()) {
                QString fraction = match.captured(3);
                while (fraction.size() < 3) {
                    fraction.append(QLatin1Char('0'));
                }
                millis = fraction.left(3).toLongLong();
            }
            times.append((minutes * 60 + seconds) * 1000 + millis);
            textStart = match.capturedEnd();
        }
        if (times.isEmpty()) {
            continue; // metadata tags like [ar:...] or untimed text
        }
        const QString text = row.mid(textStart).trimmed();
        for (qint64 time : times) {
            result.lines.append({time, text});
        }
    }
    std::stable_sort(result.lines.begin(), result.lines.end(),
                     [](const LyricLine &a, const LyricLine &b) {
                         return a.timeMs < b.timeMs;
                     });
    if (result.lines.isEmpty()) {
        result.synced = false;
    }
    return result;
}

Lyrics Lyrics::fromPlain(const QString &text) {
    Lyrics result;
    const QStringList rows = text.split(QLatin1Char('\n'));
    for (const QString &row : rows) {
        result.lines.append({-1, row.trimmed()});
    }
    // Drop leading/trailing blank lines.
    while (!result.lines.isEmpty() && result.lines.first().text.isEmpty()) {
        result.lines.removeFirst();
    }
    while (!result.lines.isEmpty() && result.lines.last().text.isEmpty()) {
        result.lines.removeLast();
    }
    return result;
}

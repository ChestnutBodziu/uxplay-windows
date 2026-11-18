#pragma once

#include <QString>
#include <QVector>

struct LyricLine {
    qint64 timeMs = -1; // -1 for lyrics without timing
    QString text;
};

// Lyrics of one track, either time-synced (LRC) or plain text.
struct Lyrics {
    bool synced = false;
    bool instrumental = false;
    QString source; // e.g. "LRCLIB", "YouTube Music (LyricFind)"
    QVector<LyricLine> lines;

    bool isEmpty() const { return lines.isEmpty(); }

    // Index of the line being sung at positionMs, -1 before the first line or
    // when the lyrics are not synced.
    int indexAt(qint64 positionMs) const;

    // "[mm:ss.xx] text" lines; a line may carry several timestamps.
    static Lyrics fromLrc(const QString &lrc);
    static Lyrics fromPlain(const QString &text);
};

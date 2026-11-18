#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QTimer>

#include "lyrics.h"
#include "lyricsprovider.h"

class MediaState;
class OverlaySettings;

// Lyrics of the current track and the line being sung. Fetches only when the
// user enabled lyrics, for the track MediaState reports; keeps a small cache so
// replays and skips do not hit the network again.
class LyricsState : public QObject {
    Q_OBJECT

public:
    enum class Status { Off, Idle, Loading, Ready, NotFound, Error };
    Q_ENUM(Status)

    LyricsState(const MediaState *media, OverlaySettings *settings,
                QObject *parent = nullptr);

    Status status() const { return m_status; }
    const Lyrics &lyrics() const { return m_lyrics; }
    // Index of the current line, -1 before the first line or when not synced.
    int currentIndex() const { return m_index; }

signals:
    void changed();            // status or lyrics changed
    void currentLineChanged(); // another line is being sung

private:
    void refresh();
    void doFetch();
    void onFetched(quint64 id, LyricsProvider::Result result, const Lyrics &lyrics);
    void updateIndex();
    void scheduleNextLine();
    void setStatus(Status status);
    QString currentKey() const;

    struct CacheEntry {
        bool found = false;
        Lyrics lyrics;
    };

    const MediaState *m_media;
    OverlaySettings *m_settings;
    LyricsProvider m_provider;
    QTimer m_debounce;
    QTimer m_lineTimer; // fires exactly when the next line starts
    Status m_status = Status::Off;
    Lyrics m_lyrics;
    int m_index = -1;
    quint64 m_requestId = 0;
    QString m_key;
    bool m_lastEnabled = false;
    QHash<QString, CacheEntry> m_cache;
};

#include "lyricsstate.h"

#include "mediastate.h"
#include "overlaysettings.h"

#include <algorithm>

namespace {
constexpr int kDebounceMs = 700;
constexpr int kMaxCacheEntries = 60;
} // namespace

LyricsState::LyricsState(const MediaState *media, OverlaySettings *settings,
                         QObject *parent)
    : QObject(parent), m_media(media), m_settings(settings), m_provider(this) {
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(kDebounceMs);
    connect(&m_debounce, &QTimer::timeout, this, &LyricsState::doFetch);
    connect(&m_provider, &LyricsProvider::finished, this, &LyricsState::onFetched);

    connect(m_media, &MediaState::trackChanged, this, &LyricsState::refresh);
    connect(m_media, &MediaState::stateChanged, this, &LyricsState::refresh);
    // Re-sync whenever the device reports a position (start, resume, seek) or
    // the play state changes; between those, a timer fires on each line change.
    m_lineTimer.setSingleShot(true);
    m_lineTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_lineTimer, &QTimer::timeout, this, &LyricsState::updateIndex);
    connect(m_media, &MediaState::progressReported, this, &LyricsState::updateIndex);
    connect(m_media, &MediaState::stateChanged, this, &LyricsState::updateIndex);
    connect(m_settings, &OverlaySettings::changed, this, [this]() {
        if (m_settings->lyricsEnabled() != m_lastEnabled) {
            refresh();
        } else {
            updateIndex(); // the timing offset may have changed
        }
    });
    refresh();
}

QString LyricsState::currentKey() const {
    return m_media->title() + QLatin1Char('\x1f') + m_media->artist();
}

void LyricsState::setStatus(Status status) {
    m_status = status;
    emit changed();
}

void LyricsState::refresh() {
    const bool enabled = m_settings->lyricsEnabled();
    m_lastEnabled = enabled;

    const auto reset = [this](Status status) {
        m_debounce.stop();
        m_provider.cancel();
        ++m_requestId; // drop any answer still in flight
        m_key.clear();
        m_lyrics = Lyrics();
        m_index = -1;
        setStatus(status);
    };

    if (!enabled) {
        if (m_status != Status::Off) {
            reset(Status::Off);
        }
        return;
    }
    if (m_media->state() == MediaState::State::Disconnected ||
        m_media->title().isEmpty()) {
        if (m_status != Status::Idle) {
            reset(Status::Idle);
        }
        return;
    }

    const QString key = currentKey();
    if (key == m_key && m_status != Status::Off && m_status != Status::Idle) {
        return; // same track: keep what we have
    }
    m_key = key;
    m_lyrics = Lyrics();
    m_index = -1;

    const auto cached = m_cache.constFind(key);
    if (cached != m_cache.constEnd()) {
        m_debounce.stop();
        m_provider.cancel();
        ++m_requestId;
        if (cached->found) {
            m_lyrics = cached->lyrics;
            setStatus(Status::Ready);
            updateIndex();
        } else {
            setStatus(Status::NotFound);
        }
        return;
    }
    // Metadata can arrive in two steps; wait briefly before asking the network.
    m_provider.cancel();
    ++m_requestId;
    setStatus(Status::Loading);
    m_debounce.start();
}

void LyricsState::doFetch() {
    if (!m_settings->lyricsEnabled() || m_media->title().isEmpty()) {
        return;
    }
    LyricsProvider::Query query;
    query.title = m_media->title();
    query.artist = m_media->artist();
    query.album = m_media->album();
    query.durationMs = m_media->durationMs();
    m_provider.fetch(++m_requestId, query);
}

void LyricsState::onFetched(quint64 id, LyricsProvider::Result result,
                            const Lyrics &lyrics) {
    if (id != m_requestId) {
        return; // stale answer for a previous track
    }
    if (m_cache.size() >= kMaxCacheEntries) {
        m_cache.clear();
    }
    switch (result) {
    case LyricsProvider::Result::Found:
        m_lyrics = lyrics;
        m_cache.insert(m_key, {true, lyrics});
        setStatus(Status::Ready);
        updateIndex();
        break;
    case LyricsProvider::Result::NotFound:
        m_cache.insert(m_key, {false, {}});
        setStatus(Status::NotFound);
        break;
    case LyricsProvider::Result::Error:
        setStatus(Status::Error); // not cached: retried on the next track change
        break;
    }
}

void LyricsState::updateIndex() {
    if (m_status != Status::Ready || !m_lyrics.synced) {
        m_lineTimer.stop();
        return;
    }
    const int index = m_lyrics.indexAt(m_media->positionMs() + m_settings->lyricsOffsetMs());
    if (index != m_index) {
        m_index = index;
        emit currentLineChanged();
    }
    scheduleNextLine();
}

void LyricsState::scheduleNextLine() {
    m_lineTimer.stop();
    if (m_media->state() != MediaState::State::Playing) {
        return; // paused: the next progress/state event re-syncs
    }
    const int next = m_lyrics.indexAt(m_media->positionMs() + m_settings->lyricsOffsetMs()) + 1;
    if (next >= m_lyrics.lines.size()) {
        return;
    }
    const qint64 now = m_media->positionMs() + m_settings->lyricsOffsetMs();
    const qint64 delay = m_lyrics.lines.at(next).timeMs - now;
    m_lineTimer.start(int(std::clamp<qint64>(delay, 1, 60000)));
}

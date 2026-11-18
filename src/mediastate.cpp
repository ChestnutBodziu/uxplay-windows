#include "mediastate.h"

#include "airplayevents.h"

#include <QDebug>
#include <algorithm>

namespace {
constexpr double kRtpRate = 44100.0;
constexpr int kMaxCoverBytes = 8 * 1024 * 1024;
constexpr int kMaxCoverSide = 512;
constexpr int kSilenceMs = 1500;
} // namespace

MediaState::MediaState(AirPlayEvents *events, QObject *parent)
    : QObject(parent) {
    m_tick.setInterval(1000);
    m_tick.setTimerType(Qt::CoarseTimer);
    connect(&m_tick, &QTimer::timeout, this, &MediaState::tick);
    m_silence.setSingleShot(true);
    m_silence.setInterval(kSilenceMs);
    m_silence.setTimerType(Qt::CoarseTimer);
    connect(&m_silence, &QTimer::timeout, this, &MediaState::onSilence);

    // Engine threads emit these signals, so queue them onto the GUI thread.
    const auto queued = Qt::QueuedConnection;
    connect(events, &AirPlayEvents::clientConnected, this,
            [this](const QString &name, const QString &model, const QString &) {
                onConnected(name, model);
            }, queued);
    connect(events, &AirPlayEvents::clientDisconnected, this,
            &MediaState::onDisconnected, queued);
    connect(events, &AirPlayEvents::metadata, this, &MediaState::onMetadata,
            queued);
    connect(events, &AirPlayEvents::coverArt, this, &MediaState::onCover,
            queued);
    connect(events, &AirPlayEvents::progress, this, &MediaState::onProgress,
            queued);
    connect(events, &AirPlayEvents::volume, this, &MediaState::onVolume,
            queued);
    connect(events, &AirPlayEvents::dacp, this, &MediaState::onDacp, queued);
    connect(events, &AirPlayEvents::audioActivity, this, &MediaState::onActivity,
            queued);
}

qint64 MediaState::positionMs() const {
    qint64 position = m_basePositionMs;
    if (m_state == State::Playing && m_sinceBase.isValid()) {
        position += m_sinceBase.elapsed();
    }
    if (m_durationMs > 0) {
        position = std::min(position, m_durationMs);
    }
    return std::max<qint64>(position, 0);
}

void MediaState::setState(State state) {
    if (m_state == state) {
        return;
    }
    m_state = state;
    if (state == State::Playing) {
        m_tick.start();
    } else {
        m_tick.stop();
        m_silence.stop();
    }
    qInfo() << "[media] state:" << state;
    emit stateChanged(state);
}

void MediaState::resetTrack() {
    m_title.clear();
    m_artist.clear();
    m_album.clear();
    m_durationMs = 0;
    m_basePositionMs = 0;
    m_sinceBase.invalidate();
    if (!m_cover.isNull()) {
        m_cover = QImage();
        emit coverChanged();
    }
    emit trackChanged();
    emit positionChanged(0, 0);
}

void MediaState::onConnected(const QString &name, const QString &model) {
    m_deviceName = name;
    m_deviceModel = model;
    m_volume = -1.0;
    m_dacpId.clear();
    m_activeRemote.clear();
    qInfo().noquote() << "[media] connected:" << name << "(" << model << ")";
    resetTrack();
    setState(State::Idle);
    emit deviceChanged();
}

void MediaState::onDisconnected() {
    qInfo() << "[media] disconnected";
    setState(State::Disconnected);
    m_deviceName.clear();
    m_deviceModel.clear();
    m_dacpId.clear();
    m_activeRemote.clear();
    resetTrack();
    emit deviceChanged();
    emit dacpChanged();
}

void MediaState::onMetadata(const QString &title, const QString &artist,
                            const QString &album) {
    if (m_state == State::Disconnected) {
        // A client can send metadata before the connect event is observed.
        setState(State::Idle);
    }
    if (title == m_title && artist == m_artist && album == m_album) {
        return;
    }
    m_title = title;
    m_artist = artist;
    m_album = album;
    qInfo().noquote() << "[media] track:" << title << "|" << artist << "|"
                      << album;
    emit trackChanged();
}

void MediaState::onCover(const QByteArray &jpeg) {
    if (jpeg.isEmpty() || jpeg.size() > kMaxCoverBytes) {
        return;
    }
    QImage image;
    if (!image.loadFromData(jpeg)) {
        qWarning() << "[media] cover art could not be decoded";
        return;
    }
    // Keep the model small: the overlay never shows more than this.
    if (std::max(image.width(), image.height()) > kMaxCoverSide) {
        image = image.scaled(kMaxCoverSide, kMaxCoverSide, Qt::KeepAspectRatio,
                             Qt::SmoothTransformation);
    }
    m_cover = image;
    qInfo() << "[media] cover:" << image.size();
    emit coverChanged();
}

void MediaState::onProgress(quint32 start, quint32 current, quint32 end) {
    if (m_state == State::Disconnected) {
        setState(State::Idle);
    }
    // 32-bit RTP tick differences, wrap-safe.
    const quint32 elapsed = current - start;
    const quint32 total = end - start;
    m_durationMs = static_cast<qint64>(total / kRtpRate * 1000.0);
    m_basePositionMs = static_cast<qint64>(elapsed / kRtpRate * 1000.0);
    m_sinceBase.restart();
    qInfo() << "[media] progress:" << m_basePositionMs << "/" << m_durationMs
            << "ms";
    emit positionChanged(positionMs(), m_durationMs);
}

void MediaState::onVolume(float db) {
    double value;
    if (db < -30.0f) {
        value = 0.0; // includes the -144 dB mute signal
    } else if (db > 0.0f) {
        value = 1.0;
    } else {
        value = (30.0 + db) / 30.0;
    }
    if (value == m_volume) {
        return;
    }
    m_volume = value;
    emit volumeChanged(value);
}

void MediaState::onDacp(const QString &id, const QString &remote) {
    if (id == m_dacpId && remote == m_activeRemote) {
        return;
    }
    m_dacpId = id;
    m_activeRemote = remote;
    qInfo().noquote() << "[media] dacp id:" << id;
    emit dacpChanged();
}

void MediaState::onActivity() {
    if (m_state == State::Disconnected) {
        return;
    }
    m_lastAudio.restart();
    m_silence.start();
    if (m_state != State::Playing) {
        // Resume from the frozen position (or the last reported one).
        m_sinceBase.restart();
        setState(State::Playing);
        emit positionChanged(positionMs(), m_durationMs);
    }
}

qint64 MediaState::positionAtLastAudioMs() const {
    qint64 position = m_basePositionMs;
    if (m_sinceBase.isValid() && m_lastAudio.isValid()) {
        position += std::max<qint64>(
            m_sinceBase.elapsed() - m_lastAudio.elapsed(), 0);
    }
    if (m_durationMs > 0) {
        position = std::min(position, m_durationMs);
    }
    return std::max<qint64>(position, 0);
}

void MediaState::onSilence() {
    if (m_state != State::Playing) {
        return;
    }
    // Freeze at the moment the last packet arrived, not when we noticed.
    m_basePositionMs = positionAtLastAudioMs();
    m_sinceBase.invalidate();
    setState(State::Paused);
    emit positionChanged(m_basePositionMs, m_durationMs);
}

void MediaState::tick() {
    emit positionChanged(positionMs(), m_durationMs);
}

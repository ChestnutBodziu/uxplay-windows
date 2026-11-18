#pragma once

#include <QElapsedTimer>
#include <QImage>
#include <QObject>
#include <QString>
#include <QTimer>

class AirPlayEvents;

// Model of what the connected device is playing. Lives in the GUI thread and
// knows nothing about the engine or about widgets.
//
// Playing/Paused is derived from audio packets actually arriving: packets
// present means playing, silence for kSilenceMs means paused. (iOS sends no
// flush on pause, only a progress message, so progress alone is ambiguous.)
// PROGRESS messages only set the track position and duration. The position is
// interpolated locally by a 1 Hz timer that runs only while playing.
class MediaState : public QObject {
    Q_OBJECT

public:
    enum class State { Disconnected, Idle, Playing, Paused };
    Q_ENUM(State)

    explicit MediaState(AirPlayEvents *events, QObject *parent = nullptr);

    State state() const { return m_state; }
    QString deviceName() const { return m_deviceName; }
    QString deviceModel() const { return m_deviceModel; }
    QString title() const { return m_title; }
    QString artist() const { return m_artist; }
    QString album() const { return m_album; }
    const QImage &coverArt() const { return m_cover; }
    qint64 durationMs() const { return m_durationMs; }
    qint64 positionMs() const;
    // AirPlay volume mapped to 0.0..1.0 (0 = mute), -1 if unknown.
    double volume() const { return m_volume; }
    QString dacpId() const { return m_dacpId; }
    QString activeRemote() const { return m_activeRemote; }

signals:
    void stateChanged(MediaState::State state);
    void deviceChanged();
    void trackChanged();
    void coverChanged();
    void positionChanged(qint64 positionMs, qint64 durationMs);
    void volumeChanged(double volume);
    void dacpChanged();

private:
    void onConnected(const QString &name, const QString &model);
    void onDisconnected();
    void onMetadata(const QString &title, const QString &artist,
                    const QString &album);
    void onCover(const QByteArray &jpeg);
    void onProgress(quint32 start, quint32 current, quint32 end);
    void onVolume(float db);
    void onDacp(const QString &id, const QString &remote);
    void onActivity();
    void onSilence();
    void setState(State state);
    void resetTrack();
    void tick();
    qint64 positionAtLastAudioMs() const;

    State m_state = State::Disconnected;
    QString m_deviceName, m_deviceModel;
    QString m_title, m_artist, m_album;
    QImage m_cover;
    qint64 m_durationMs = 0;
    qint64 m_basePositionMs = 0;
    QElapsedTimer m_sinceBase;
    double m_volume = -1.0;
    QString m_dacpId, m_activeRemote;
    QTimer m_tick;
    QTimer m_silence;           // fires when audio packets stop arriving
    QElapsedTimer m_lastAudio;
};

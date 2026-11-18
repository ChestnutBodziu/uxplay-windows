#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

#include "uxplay_api.h"

// Bridge between the C event callback of the UxPlay engine (called from engine
// threads) and Qt. All data is copied; signals reach receivers living in the
// GUI thread through queued connections. This is the only class, besides
// AirPlayWorker, that knows uxplay_api.h.
class AirPlayEvents : public QObject {
    Q_OBJECT

public:
    explicit AirPlayEvents(QObject *parent = nullptr);
    ~AirPlayEvents() override;

signals:
    void clientConnected(const QString &name, const QString &model,
                         const QString &deviceId);
    void clientDisconnected();
    void metadata(const QString &title, const QString &artist,
                  const QString &album);
    void coverArt(const QByteArray &jpeg);
    // Ticks of a 44100 Hz clock, as sent by the client.
    void progress(quint32 start, quint32 current, quint32 end);
    // AirPlay volume in dB: -30..0, -144 means mute.
    void volume(float db);
    void dacp(const QString &dacpId, const QString &activeRemote);
    void audioFlushed();
    // Audio packets are arriving (throttled by the engine to 4 per second).
    void audioActivity();

private:
    static void trampoline(const uxp_event *ev, void *user);
    void handle(const uxp_event &ev);
};

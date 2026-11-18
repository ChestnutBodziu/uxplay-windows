#include "airplayevents.h"

#include "uxplay_api.h"

namespace {
QString fromUtf8(const char *text) {
    return text ? QString::fromUtf8(text) : QString();
}
} // namespace

AirPlayEvents::AirPlayEvents(QObject *parent) : QObject(parent) {
    uxplay_set_event_callback(&AirPlayEvents::trampoline, this);
}

AirPlayEvents::~AirPlayEvents() {
    // Returns only after any in-flight callback has finished.
    uxplay_set_event_callback(nullptr, nullptr);
}

void AirPlayEvents::trampoline(const uxp_event *ev, void *user) {
    if (!ev || !user) {
        return;
    }
    try {
        static_cast<AirPlayEvents *>(user)->handle(*ev);
    } catch (...) {
        // Never let an exception cross into the C engine.
    }
}

void AirPlayEvents::handle(const uxp_event &ev) {
    switch (ev.type) {
    case UXP_EVT_CLIENT_CONNECTED:
        emit clientConnected(fromUtf8(ev.device_name),
                             fromUtf8(ev.device_model),
                             fromUtf8(ev.device_id));
        break;
    case UXP_EVT_CLIENT_DISCONNECTED:
        emit clientDisconnected();
        break;
    case UXP_EVT_METADATA:
        emit metadata(fromUtf8(ev.title), fromUtf8(ev.artist),
                      fromUtf8(ev.album));
        break;
    case UXP_EVT_COVERART:
        if (ev.data && ev.data_len > 0) {
            emit coverArt(QByteArray(static_cast<const char *>(ev.data),
                                     static_cast<qsizetype>(ev.data_len)));
        }
        break;
    case UXP_EVT_PROGRESS:
        emit progress(ev.rtp_start, ev.rtp_curr, ev.rtp_end);
        break;
    case UXP_EVT_VOLUME:
        emit volume(ev.volume_db);
        break;
    case UXP_EVT_DACP:
        emit dacp(fromUtf8(ev.dacp_id), fromUtf8(ev.active_remote));
        break;
    case UXP_EVT_AUDIO_FLUSH:
        emit audioFlushed();
        break;
    case UXP_EVT_AUDIO_ACTIVITY:
        emit audioActivity();
        break;
    }
}

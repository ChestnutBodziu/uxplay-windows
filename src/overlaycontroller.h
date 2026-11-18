#pragma once

#include <QObject>
#include <QTimer>

#include "mediastate.h"

class DacpClient;
class LyricsState;
class OverlaySettings;
class OverlayWindow;
class QAction;
class QMenu;

// Decides when the overlay is visible, from the AirPlay state and the settings.
class OverlayController : public QObject {
    Q_OBJECT

public:
    OverlayController(const MediaState *media, DacpClient *dacp, LyricsState *lyrics,
                      OverlaySettings *settings, QObject *parent = nullptr);
    ~OverlayController() override;

    OverlaySettings *settings() const { return m_settings; }

    // Adds overlay toggles to an existing tray menu.
    void attachTrayMenu(QMenu *menu);

private:
    void evaluate();
    void syncTrayActions();

    const MediaState *m_media;
    OverlaySettings *m_settings;
    OverlayWindow *m_window;
    QTimer m_hideTimer;
    bool m_wasEnabled = false;

    QAction *m_enabledAction = nullptr;
    QAction *m_expandedAction = nullptr;
    QAction *m_clickThroughAction = nullptr;
};

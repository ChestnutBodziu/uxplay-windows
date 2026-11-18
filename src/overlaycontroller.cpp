#include "overlaycontroller.h"

#include "overlaysettings.h"
#include "overlaywindow.h"

#include <QAction>
#include <QMenu>

OverlayController::OverlayController(const MediaState *media, DacpClient *dacp,
                                     LyricsState *lyrics, OverlaySettings *settings,
                                     QObject *parent)
    : QObject(parent),
      m_media(media),
      m_settings(settings),
      m_window(new OverlayWindow(media, dacp, lyrics, m_settings)) {
    m_hideTimer.setSingleShot(true);
    connect(&m_hideTimer, &QTimer::timeout, this, [this]() {
        m_window->hideOverlay();
    });

    connect(media, &MediaState::stateChanged, this, &OverlayController::evaluate);
    connect(media, &MediaState::trackChanged, this, [this]() {
        // A new track while hidden (auto-show on) brings the overlay back.
        if (m_media->state() != MediaState::State::Disconnected &&
            !m_media->title().isEmpty()) {
            evaluate();
        }
    });
    connect(m_settings, &OverlaySettings::changed, this, [this]() {
        evaluate();
        syncTrayActions();
    });

    m_wasEnabled = m_settings->enabled();
    evaluate();
}

OverlayController::~OverlayController() {
    delete m_window;
}

void OverlayController::evaluate() {
    const bool enabled = m_settings->enabled();
    const MediaState::State state = m_media->state();
    const bool justEnabled = enabled && !m_wasEnabled;
    m_wasEnabled = enabled;

    if (!enabled) {
        m_hideTimer.stop();
        m_window->hideOverlay();
        return;
    }

    if (state == MediaState::State::Disconnected) {
        if (justEnabled) {
            // Nothing to show yet: display the waiting state so the toggle has
            // visible effect, then let auto-hide take it away again.
            m_window->showOverlay();
        }
        if (m_window->isOverlayShown() && m_settings->autoHide() &&
            !m_hideTimer.isActive()) {
            m_hideTimer.start(m_settings->hideDelaySec() * 1000);
        }
        return;
    }

    // Connected: keep it visible, and bring it up when auto-show is on.
    m_hideTimer.stop();
    if (m_settings->autoShow() || m_window->isOverlayShown() || justEnabled) {
        m_window->showOverlay();
    }
}

void OverlayController::attachTrayMenu(QMenu *menu) {
    if (!menu) {
        return;
    }
    menu->addSeparator();
    m_enabledAction = menu->addAction(tr("Floating overlay"));
    m_enabledAction->setCheckable(true);
    connect(m_enabledAction, &QAction::toggled, m_settings,
            &OverlaySettings::setEnabled);

    m_expandedAction = menu->addAction(tr("Overlay: expanded mode"));
    m_expandedAction->setCheckable(true);
    connect(m_expandedAction, &QAction::toggled, m_settings, [this](bool on) {
        m_settings->setMode(on ? OverlaySettings::Mode::Expanded
                               : OverlaySettings::Mode::Compact);
    });

    m_clickThroughAction = menu->addAction(tr("Overlay: click-through"));
    m_clickThroughAction->setCheckable(true);
    connect(m_clickThroughAction, &QAction::toggled, m_settings,
            &OverlaySettings::setClickThrough);

    menu->addSeparator();
    syncTrayActions();
}

void OverlayController::syncTrayActions() {
    const auto sync = [](QAction *action, bool value) {
        if (action && action->isChecked() != value) {
            QSignalBlocker blocker(action);
            action->setChecked(value);
        }
    };
    sync(m_enabledAction, m_settings->enabled());
    sync(m_expandedAction,
         m_settings->mode() == OverlaySettings::Mode::Expanded);
    sync(m_clickThroughAction, m_settings->clickThrough());
}

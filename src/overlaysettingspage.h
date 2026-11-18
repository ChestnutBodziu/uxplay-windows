#pragma once

#include <QWidget>

class OverlaySettings;
class QCheckBox;
class QComboBox;
class QLabel;
class QSlider;
class QSpinBox;

// Settings page for the floating overlay. Edits go straight to OverlaySettings
// (which persists them); changes made elsewhere (tray, overlay menu) are
// reflected back without feedback loops.
class OverlaySettingsPage : public QWidget {
    Q_OBJECT

public:
    explicit OverlaySettingsPage(OverlaySettings *settings, QWidget *parent = nullptr);

private:
    void refresh();

    OverlaySettings *m_settings;
    QCheckBox *m_enabled;
    QCheckBox *m_autoShow;
    QCheckBox *m_alwaysOnTop;
    QCheckBox *m_clickThrough;
    QCheckBox *m_animations;
    QCheckBox *m_autoHide;
    QCheckBox *m_lyrics;
    QSlider *m_lyricsOffset;
    QLabel *m_lyricsOffsetValue;
    QSpinBox *m_hideDelay;
    QComboBox *m_mode;
    QSlider *m_scale;
    QLabel *m_scaleValue;
    QSlider *m_opacity;
    QLabel *m_opacityValue;
};

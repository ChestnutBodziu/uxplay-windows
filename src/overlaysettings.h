#pragma once

#include <QObject>
#include <QPoint>
#include <QSettings>

// Persistent settings of the floating overlay (QSettings group "overlay").
// Every setter validates, stores immediately and emits changed() only when the
// value actually changed.
class OverlaySettings : public QObject {
    Q_OBJECT

public:
    enum class Mode { Compact, Expanded };
    Q_ENUM(Mode)

    static constexpr int kMinScale = 75;
    static constexpr int kMaxScale = 150;
    static constexpr int kMinOpacity = 20;
    static constexpr int kMaxOpacity = 100;
    static constexpr int kMinLyricsOffsetMs = -3000;
    static constexpr int kMaxLyricsOffsetMs = 3000;
    static constexpr int kMinHideDelaySec = 0;
    static constexpr int kMaxHideDelaySec = 60;

    explicit OverlaySettings(QObject *parent = nullptr);

    bool enabled() const { return m_enabled; }
    bool autoShow() const { return m_autoShow; }
    bool alwaysOnTop() const { return m_alwaysOnTop; }
    bool hasPosition() const { return m_hasPosition; }
    QPoint position() const { return m_position; }
    int scalePercent() const { return m_scale; }
    int opacityPercent() const { return m_opacity; }
    Mode mode() const { return m_mode; }
    bool animations() const { return m_animations; }
    bool clickThrough() const { return m_clickThrough; }
    bool autoHide() const { return m_autoHide; }
    int hideDelaySec() const { return m_hideDelay; }
    // Online lyrics (LRCLIB). Off by default: title/artist/album leave the PC.
    bool lyricsEnabled() const { return m_lyricsEnabled; }
    // Overlay shows lyrics instead of the cover (only meaningful when enabled).
    bool lyricsView() const { return m_lyricsView; }
    // Shifts lyrics timing: positive shows each line earlier.
    int lyricsOffsetMs() const { return m_lyricsOffsetMs; }

    void setEnabled(bool value);
    void setAutoShow(bool value);
    void setAlwaysOnTop(bool value);
    void setPosition(const QPoint &value);
    void clearPosition();
    void setScalePercent(int value);
    void setOpacityPercent(int value);
    void setMode(Mode value);
    void setAnimations(bool value);
    void setClickThrough(bool value);
    void setAutoHide(bool value);
    void setHideDelaySec(int value);
    void setLyricsEnabled(bool value);
    void setLyricsView(bool value);
    void setLyricsOffsetMs(int value);

signals:
    void changed();

private:
    template <typename T>
    void update(T &field, const T &value, const char *key);

    bool m_enabled = true;
    bool m_autoShow = true;
    bool m_alwaysOnTop = true;
    bool m_hasPosition = false;
    QPoint m_position;
    int m_scale = 100;
    int m_opacity = 100;
    Mode m_mode = Mode::Compact;
    bool m_animations = true;
    bool m_clickThrough = false;
    bool m_autoHide = true;
    int m_hideDelay = 5;
    bool m_lyricsEnabled = false;
    bool m_lyricsView = false;
    int m_lyricsOffsetMs = 300;
};

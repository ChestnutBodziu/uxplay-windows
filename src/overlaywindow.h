#pragma once

#include <QPoint>
#include <QVariantAnimation>
#include <QWidget>

class MediaState;
class OverlaySettings;

// Frameless, always-on-top mini player. Pure view: it paints what MediaState
// holds and what OverlaySettings says, and owns no playback logic.
class OverlayWindow : public QWidget {
    Q_OBJECT

public:
    OverlayWindow(const MediaState *media, OverlaySettings *settings,
                  QWidget *parent = nullptr);

    // Fade in/out (instant when animations are off). Never takes focus.
    void showOverlay();
    void hideOverlay();
    bool isOverlayShown() const { return m_shown; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    void applySettings();
    void applyFlags();
    void placeOnScreen();
    void applyOpacity();
    void fadeTo(qreal target);
    QSize baseSize() const;

    const MediaState *m_media;
    OverlaySettings *m_settings;
    QVariantAnimation m_fade;
    qreal m_fadeValue = 0.0;
    bool m_shown = false;
    bool m_dragging = false;
    QPoint m_dragOffset;
    bool m_hadPosition = false;
    bool m_lastTopMost = true;
    bool m_lastClickThrough = false;
};

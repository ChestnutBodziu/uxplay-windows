#include "overlaywindow.h"

#include "dacpclient.h"
#include "lyrics.h"
#include "lyricsstate.h"
#include "mediastate.h"
#include "overlaysettings.h"

#include <QAction>
#include <QContextMenuEvent>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QToolTip>
#include <algorithm>

namespace {
constexpr QSize kCompactSize(320, 84);
constexpr QSize kExpandedSize(360, 150);
constexpr int kFadeMs = 180;
constexpr int kScreenMargin = 16;

const QColor kBackground(24, 24, 28, 238);
const QColor kBorder(255, 255, 255, 30);
const QColor kTextPrimary(243, 243, 245);
const QColor kTextSecondary(160, 160, 168);
const QColor kAccent(108, 182, 255);
const QColor kTrack(255, 255, 255, 40);

QString formatTime(qint64 ms) {
    const qint64 total = std::max<qint64>(ms, 0) / 1000;
    return QStringLiteral("%1:%2")
        .arg(total / 60)
        .arg(total % 60, 2, 10, QLatin1Char('0'));
}

QFont uiFont(int pixelSize, bool bold) {
    QFont font(QStringLiteral("Segoe UI Variable Text"));
    font.setFamilies({QStringLiteral("Segoe UI Variable Text"),
                      QStringLiteral("Segoe UI")});
    font.setPixelSize(pixelSize);
    font.setBold(bold);
    return font;
}

void drawElided(QPainter &p, const QRect &rect, const QFont &font,
                const QColor &color, const QString &text) {
    p.setFont(font);
    p.setPen(color);
    p.drawText(rect, Qt::AlignLeft | Qt::AlignVCenter,
               QFontMetrics(font).elidedText(text, Qt::ElideRight,
                                             rect.width()));
}
} // namespace

OverlayWindow::OverlayWindow(const MediaState *media, DacpClient *dacp,
                             LyricsState *lyrics, OverlaySettings *settings,
                             QWidget *parent)
    : QWidget(parent), m_media(media), m_dacp(dacp), m_lyrics(lyrics), m_settings(settings) {
    setMouseTracking(true);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setFocusPolicy(Qt::NoFocus);
    setWindowTitle(QStringLiteral("uxplay-windows overlay"));

    m_fade.setDuration(kFadeMs);
    m_fade.setEasingCurve(QEasingCurve::OutCubic);
    connect(&m_fade, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &value) {
                m_fadeValue = value.toReal();
                applyOpacity();
            });
    connect(&m_fade, &QVariantAnimation::finished, this, [this]() {
        if (!m_shown) {
            QWidget::hide();
        }
    });

    // Repaint only while visible; MediaState emits at most 1 Hz.
    const auto repaint = [this]() {
        if (m_shown) {
            update();
        }
    };
    connect(m_media, &MediaState::trackChanged, this, repaint);
    connect(m_media, &MediaState::coverChanged, this, repaint);
    connect(m_media, &MediaState::stateChanged, this, repaint);
    connect(m_media, &MediaState::deviceChanged, this, repaint);
    connect(m_media, &MediaState::positionChanged, this, repaint);
    connect(m_dacp, &DacpClient::statusChanged, this, repaint);
    connect(m_lyrics, &LyricsState::changed, this, repaint);
    connect(m_lyrics, &LyricsState::currentLineChanged, this, repaint);
    connect(m_settings, &OverlaySettings::changed, this,
            &OverlayWindow::applySettings);

    applyFlags();
    applySettings();
}

QSize OverlayWindow::baseSize() const {
    return m_settings->mode() == OverlaySettings::Mode::Expanded
               ? kExpandedSize
               : kCompactSize;
}

void OverlayWindow::applyFlags() {
    const bool topMost = m_settings->alwaysOnTop();
    const bool clickThrough = m_settings->clickThrough();
    m_lastTopMost = topMost;
    m_lastClickThrough = clickThrough;

    const bool wasVisible = isVisible();
    Qt::WindowFlags flags = Qt::Tool | Qt::FramelessWindowHint |
                            Qt::WindowDoesNotAcceptFocus;
    if (topMost) {
        flags |= Qt::WindowStaysOnTopHint;
    }
    if (clickThrough) {
        flags |= Qt::WindowTransparentForInput;
    }
    setWindowFlags(flags);
    if (wasVisible) {
        QWidget::show(); // changing flags hides the window
    }
}

void OverlayWindow::applySettings() {
    const bool positionReset = m_hadPosition && !m_settings->hasPosition();
    m_hadPosition = m_settings->hasPosition();
    if (m_settings->alwaysOnTop() != m_lastTopMost ||
        m_settings->clickThrough() != m_lastClickThrough) {
        applyFlags();
    }
    const qreal scale = m_settings->scalePercent() / 100.0;
    const QSize target = (QSizeF(baseSize()) * scale).toSize();
    if (size() != target) {
        // Keep the top-right corner stable when the size changes.
        const QPoint topRight = geometry().topRight();
        setFixedSize(target);
        if (isVisible()) {
            move(topRight.x() - target.width() + 1, y());
        }
    }
    if (positionReset && isVisible() && !m_dragging) {
        placeOnScreen();
    }
    applyOpacity();
    update();
}

void OverlayWindow::applyOpacity() {
    setWindowOpacity(m_fadeValue * (m_settings->opacityPercent() / 100.0));
}

void OverlayWindow::placeOnScreen() {
    QScreen *screen = nullptr;
    if (m_settings->hasPosition()) {
        screen = QGuiApplication::screenAt(m_settings->position() +
                                           QPoint(width() / 2, height() / 2));
    }
    QPoint position;
    if (screen) {
        position = m_settings->position();
    } else {
        // Default, or the saved monitor is gone: top-right of the primary screen.
        screen = QGuiApplication::primaryScreen();
        const QRect area = screen->availableGeometry();
        position = QPoint(area.right() - width() - kScreenMargin + 1,
                          area.top() + kScreenMargin);
    }
    const QRect area = screen->availableGeometry();
    position.setX(std::clamp(position.x(), area.left(),
                             std::max(area.left(), area.right() - width() + 1)));
    position.setY(std::clamp(position.y(), area.top(),
                             std::max(area.top(), area.bottom() - height() + 1)));
    move(position);
}

void OverlayWindow::fadeTo(qreal target) {
    m_fade.stop();
    if (!m_settings->animations()) {
        m_fadeValue = target;
        applyOpacity();
        if (target == 0.0) {
            QWidget::hide();
        }
        return;
    }
    m_fade.setStartValue(m_fadeValue);
    m_fade.setEndValue(target);
    m_fade.start();
}

void OverlayWindow::showOverlay() {
    if (m_shown) {
        return;
    }
    m_shown = true;
    if (!isVisible()) {
        applySettings();
        placeOnScreen();
        applyOpacity();
        QWidget::show();
    }
    fadeTo(1.0);
}

void OverlayWindow::hideOverlay() {
    if (!m_shown) {
        return;
    }
    m_shown = false;
    m_dragging = false;
    fadeTo(0.0);
}

void OverlayWindow::paintEvent(QPaintEvent *) {
    const bool expanded =
        m_settings->mode() == OverlaySettings::Mode::Expanded;
    const qreal scale = m_settings->scalePercent() / 100.0;

    QPainter p(this);
    p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing |
                     QPainter::SmoothPixmapTransform);
    p.scale(scale, scale);
    const QRectF bounds(QPointF(0, 0), QSizeF(baseSize()));

    QPainterPath card;
    card.addRoundedRect(bounds.adjusted(0.5, 0.5, -0.5, -0.5), 16, 16);
    p.fillPath(card, kBackground);
    p.setPen(QPen(kBorder, 1));
    p.drawPath(card);

    const MediaState::State state = m_media->state();
    const bool hasTrack = !m_media->title().isEmpty();

    QString title;
    QString subtitle;
    QString detail;
    if (state == MediaState::State::Disconnected) {
        title = tr("Waiting for AirPlay");
        subtitle = tr("Connect a device to start");
    } else if (!hasTrack) {
        title = tr("Connected");
        subtitle = m_media->deviceName();
        detail = tr("Waiting for playback");
    } else {
        title = m_media->title();
        subtitle = m_media->artist();
        detail = m_media->album();
    }

    const bool lyricsView = lyricsViewActive();
    const bool lyricsOnly = expanded && lyricsView;

    const int pad = expanded ? 12 : 10;
    const int coverSize = expanded ? 96 : 64;
    const QRect coverRect(pad, pad, coverSize, coverSize);

    if (!lyricsOnly) {
    QPainterPath coverPath;
    coverPath.addRoundedRect(coverRect, 10, 10);
    p.save();
    p.setClipPath(coverPath);
    if (!m_media->coverArt().isNull()) {
        const QImage &cover = m_media->coverArt();
        // Center-crop to a square.
        const int side = std::min(cover.width(), cover.height());
        const QRect source((cover.width() - side) / 2,
                           (cover.height() - side) / 2, side, side);
        p.drawImage(coverRect, cover, source);
    } else {
        p.fillRect(coverRect, QColor(255, 255, 255, 22));
        p.setPen(kTextSecondary);
        p.setFont(uiFont(coverSize / 2, false));
        p.drawText(coverRect, Qt::AlignCenter, QStringLiteral("♪"));
    }
    p.restore();

    const int textX = pad + coverSize + 12;
    const int stateGlyphSpace = expanded ? 0 : 36;
    const int textWidth = int(bounds.width()) - textX - pad - stateGlyphSpace;

    if (expanded) {
        drawElided(p, QRect(textX, 14, textWidth, 24), uiFont(15, true),
                   kTextPrimary, title);
        drawElided(p, QRect(textX, 40, textWidth, 20), uiFont(12, false),
                   kTextSecondary, subtitle);
        drawElided(p, QRect(textX, 57, textWidth, 18), uiFont(12, false),
                   kTextSecondary, detail);
    } else {
        drawElided(p, QRect(textX, 12, textWidth, 22), uiFont(14, true),
                   kTextPrimary, title);
        if (lyricsView) {
            drawElided(p, QRect(textX, 36, textWidth, 20), uiFont(12, false),
                       kAccent, lyricsLine());
        } else {
            drawElided(p, QRect(textX, 36, textWidth, 20), uiFont(12, false),
                       kTextSecondary, subtitle);
        }
    }

    } // !lyricsOnly
    if (lyricsOnly) {
        paintLyricsView(p, QRectF(pad, 10, bounds.width() - 2 * pad, 62));
    }

    // Transport buttons. Enabled only when the device really accepts remote
    // control (DACP); otherwise dimmed, with the reason in a tooltip.
    if (buttonsVisible()) {
        if (expanded) {
            paintButton(p, Button::Previous);
            paintButton(p, Button::Next);
        }
        paintButton(p, Button::PlayPause);
    }

    // Progress.
    const qint64 duration = m_media->durationMs();
    const qint64 position = m_media->positionMs();
    const qreal fraction =
        duration > 0 ? std::clamp(qreal(position) / duration, 0.0, 1.0) : 0.0;
    if (expanded) {
        const QRectF bar(pad, bounds.height() - 34, bounds.width() - 2 * pad, 4);
        p.setPen(Qt::NoPen);
        p.setBrush(kTrack);
        p.drawRoundedRect(bar, 2, 2);
        if (duration > 0) {
            p.setBrush(kAccent);
            p.drawRoundedRect(QRectF(bar.left(), bar.top(),
                                     bar.width() * fraction, bar.height()),
                              2, 2);
        }
        p.setFont(uiFont(11, false));
        p.setPen(kTextSecondary);
        const QRect timeRow(pad, int(bounds.height()) - 26,
                            int(bounds.width()) - 2 * pad, 18);
        p.drawText(timeRow, Qt::AlignLeft | Qt::AlignVCenter,
                   formatTime(position));
        if (duration > 0) {
            p.drawText(timeRow, Qt::AlignRight | Qt::AlignVCenter,
                       formatTime(duration));
        }
    } else if (duration > 0) {
        const QRectF bar(pad + 4, bounds.height() - 7, bounds.width() - 2 * pad - 8,
                         2.5);
        p.setPen(Qt::NoPen);
        p.setBrush(kTrack);
        p.drawRoundedRect(bar, 1.25, 1.25);
        p.setBrush(kAccent);
        p.drawRoundedRect(QRectF(bar.left(), bar.top(), bar.width() * fraction,
                                 bar.height()),
                          1.25, 1.25);
    }
}

QPoint OverlayWindow::logicalPoint(const QPointF &windowPos) const {
    return (windowPos / (m_settings->scalePercent() / 100.0)).toPoint();
}

bool OverlayWindow::lyricsViewActive() const {
    return m_settings->lyricsEnabled() && m_settings->lyricsView() &&
           m_media->state() != MediaState::State::Disconnected &&
           !m_media->title().isEmpty();
}

bool OverlayWindow::lyricsToggleAvailable() const {
    return m_settings->lyricsEnabled() &&
           m_media->state() != MediaState::State::Disconnected &&
           !m_media->title().isEmpty();
}

QRect OverlayWindow::lyricsToggleRect() const {
    if (m_settings->mode() == OverlaySettings::Mode::Expanded) {
        return lyricsViewActive() ? QRect(12, 10, kExpandedSize.width() - 24, 62)
                                  : QRect(12, 12, 96, 96);
    }
    return QRect(10, 10, 64, 64);
}

// The line shown in the compact overlay, or a short status message.
QString OverlayWindow::lyricsLine() const {
    switch (m_lyrics->status()) {
    case LyricsState::Status::Off:
    case LyricsState::Status::Idle:
        return QString();
    case LyricsState::Status::Loading:
        return tr("Loading lyrics...");
    case LyricsState::Status::NotFound:
        return tr("No lyrics found");
    case LyricsState::Status::Error:
        return tr("Lyrics unavailable");
    case LyricsState::Status::Ready:
        break;
    }
    const Lyrics &lyrics = m_lyrics->lyrics();
    if (lyrics.instrumental) {
        return tr("Instrumental");
    }
    if (!lyrics.synced) {
        return tr("Lyrics are not time-synced");
    }
    const int index = m_lyrics->currentIndex();
    if (index < 0 || lyrics.lines.at(index).text.isEmpty()) {
        return QStringLiteral("\u266A");
    }
    return lyrics.lines.at(index).text;
}

void OverlayWindow::paintLyricsView(QPainter &p, const QRectF &area) const {
    const Lyrics &lyrics = m_lyrics->lyrics();
    const bool ready = m_lyrics->status() == LyricsState::Status::Ready &&
                       lyrics.synced && !lyrics.instrumental;
    if (!ready) {
        // Status message, centered.
        p.setFont(uiFont(13, false));
        p.setPen(kTextSecondary);
        const QString text = lyricsLine();
        p.drawText(area, Qt::AlignCenter,
                   QFontMetrics(p.font()).elidedText(text, Qt::ElideRight,
                                                     int(area.width())));
        return;
    }

    const int index = m_lyrics->currentIndex();
    const auto lineAt = [&](int i) -> QString {
        return i >= 0 && i < lyrics.lines.size() ? lyrics.lines.at(i).text : QString();
    };
    const auto drawLine = [&](const QRectF &rect, const QFont &font, const QColor &color,
                              const QString &text) {
        p.setFont(font);
        p.setPen(color);
        p.drawText(rect, Qt::AlignCenter,
                   QFontMetrics(font).elidedText(text, Qt::ElideRight, int(rect.width())));
    };
    const QString current = lineAt(index).isEmpty() ? QStringLiteral("\u266A") : lineAt(index);
    QColor dim = kTextSecondary;
    dim.setAlpha(150);
    drawLine(QRectF(area.left(), area.top(), area.width(), 16), uiFont(11, false), dim,
             lineAt(index - 1));
    drawLine(QRectF(area.left(), area.top() + 18, area.width(), 24), uiFont(16, true),
             kTextPrimary, current);
    drawLine(QRectF(area.left(), area.top() + 44, area.width(), 16), uiFont(11, false), dim,
             lineAt(index + 1));
}

bool OverlayWindow::buttonsVisible() const {
    return m_media->state() != MediaState::State::Disconnected;
}

bool OverlayWindow::buttonEnabled() const {
    return m_dacp && m_dacp->canControl();
}

QRect OverlayWindow::buttonRect(Button button) const {
    const QSize size = baseSize();
    if (m_settings->mode() == OverlaySettings::Mode::Compact) {
        if (button == Button::PlayPause) {
            return QRect(size.width() - 10 - 34, size.height() / 2 - 17, 34, 34);
        }
        return QRect();
    }
    constexpr int x0 = 12 + 96 + 12;
    constexpr int y = 78;
    switch (button) {
    case Button::Previous:
        return QRect(x0, y, 30, 30);
    case Button::PlayPause:
        return QRect(x0 + 38, y, 30, 30);
    case Button::Next:
        return QRect(x0 + 76, y, 30, 30);
    case Button::None:
        break;
    }
    return QRect();
}

OverlayWindow::Button OverlayWindow::buttonAt(const QPointF &windowPos) const {
    if (!buttonsVisible()) {
        return Button::None;
    }
    const qreal scale = m_settings->scalePercent() / 100.0;
    const QPoint logical = (windowPos / scale).toPoint();
    for (Button button : {Button::Previous, Button::PlayPause, Button::Next}) {
        if (buttonRect(button).contains(logical)) {
            return button;
        }
    }
    return Button::None;
}

void OverlayWindow::paintButton(QPainter &p, Button button) const {
    const QRectF rect = buttonRect(button);
    const bool enabled = buttonEnabled();
    const bool hover = enabled && m_hover == button;
    const bool pressed = enabled && m_pressed == button;

    p.setPen(Qt::NoPen);
    if (hover || pressed) {
        p.setBrush(QColor(255, 255, 255, pressed ? 46 : 26));
        p.drawEllipse(rect);
    }
    QColor color = kTextPrimary;
    if (!enabled) {
        color.setAlpha(110);
    }
    p.setBrush(color);

    const QPointF c = rect.center();
    const qreal s = rect.width() * 0.30; // half size of the glyph
    const auto triangle = [&](qreal cx, qreal dir) {
        QPainterPath tri;
        tri.moveTo(cx - dir * s * 0.55, c.y() - s);
        tri.lineTo(cx - dir * s * 0.55, c.y() + s);
        tri.lineTo(cx + dir * s * 0.75, c.y());
        tri.closeSubpath();
        p.drawPath(tri);
    };

    switch (button) {
    case Button::PlayPause:
        if (m_media->state() == MediaState::State::Playing) {
            p.drawRoundedRect(QRectF(c.x() - s * 0.85, c.y() - s, s * 0.6, s * 2), 1.5, 1.5);
            p.drawRoundedRect(QRectF(c.x() + s * 0.25, c.y() - s, s * 0.6, s * 2), 1.5, 1.5);
        } else {
            triangle(c.x(), 1.0);
        }
        break;
    case Button::Previous:
        p.drawRoundedRect(QRectF(c.x() - s * 0.95, c.y() - s, s * 0.25, s * 2), 1, 1);
        triangle(c.x() + s * 0.1, -1.0);
        break;
    case Button::Next:
        p.drawRoundedRect(QRectF(c.x() + s * 0.7, c.y() - s, s * 0.25, s * 2), 1, 1);
        triangle(c.x() - s * 0.1, 1.0);
        break;
    case Button::None:
        break;
    }
}

void OverlayWindow::trigger(Button button) {
    if (!m_dacp || !m_dacp->canControl()) {
        return;
    }
    switch (button) {
    case Button::PlayPause:
        m_dacp->playPause();
        break;
    case Button::Previous:
        m_dacp->previousTrack();
        break;
    case Button::Next:
        m_dacp->nextTrack();
        break;
    case Button::None:
        break;
    }
}

void OverlayWindow::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        return;
    }
    const Button hit = buttonAt(event->position());
    if (hit != Button::None && buttonEnabled()) {
        m_pressed = hit;
        update();
        event->accept();
        return;
    }
    if (lyricsToggleAvailable() &&
        lyricsToggleRect().contains(logicalPoint(event->position()))) {
        m_pressedToggle = true; // a click toggles cover/lyrics; no drag
        event->accept();
        return;
    }
    m_dragging = true;
    m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
    event->accept();
}

void OverlayWindow::mouseMoveEvent(QMouseEvent *event) {
    if (m_dragging) {
        move(event->globalPosition().toPoint() - m_dragOffset);
        event->accept();
        return;
    }
    const Button hit = buttonAt(event->position());
    if (hit != m_hover) {
        m_hover = hit;
        update();
    }
    const bool overToggle = lyricsToggleAvailable() &&
                            lyricsToggleRect().contains(logicalPoint(event->position()));
    if (overToggle) {
        QToolTip::showText(event->globalPosition().toPoint(),
                           m_settings->lyricsView() ? tr("Show the cover") : tr("Show lyrics"),
                           this);
    } else if (hit != Button::None && !buttonEnabled() && m_dacp) {
        QToolTip::showText(event->globalPosition().toPoint(),
                           m_dacp->unavailableReason(), this);
    } else {
        QToolTip::hideText();
    }
}

void OverlayWindow::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        return;
    }
    if (m_pressed != Button::None) {
        const Button pressed = m_pressed;
        m_pressed = Button::None;
        update();
        if (buttonAt(event->position()) == pressed) {
            trigger(pressed);
        }
        event->accept();
        return;
    }
    if (m_pressedToggle) {
        m_pressedToggle = false;
        if (lyricsToggleAvailable() &&
            lyricsToggleRect().contains(logicalPoint(event->position()))) {
            m_settings->setLyricsView(!m_settings->lyricsView());
        }
        event->accept();
        return;
    }
    if (m_dragging) {
        m_dragging = false;
        m_settings->setPosition(pos());
        event->accept();
    }
}

void OverlayWindow::mouseDoubleClickEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        return;
    }
    // A fast second click on a button is a second press, not a mode switch.
    const Button hit = buttonAt(event->position());
    if (hit != Button::None ||
        (lyricsToggleAvailable() &&
         lyricsToggleRect().contains(logicalPoint(event->position())))) {
        mousePressEvent(event);
        return;
    }
    m_settings->setMode(m_settings->mode() == OverlaySettings::Mode::Compact
                            ? OverlaySettings::Mode::Expanded
                            : OverlaySettings::Mode::Compact);
    event->accept();
}

void OverlayWindow::leaveEvent(QEvent *event) {
    QWidget::leaveEvent(event);
    if (m_hover != Button::None) {
        m_hover = Button::None;
        update();
    }
    QToolTip::hideText();
}

void OverlayWindow::contextMenuEvent(QContextMenuEvent *event) {
    QMenu menu;
    QAction *expanded = menu.addAction(tr("Expanded mode"));
    expanded->setCheckable(true);
    expanded->setChecked(m_settings->mode() == OverlaySettings::Mode::Expanded);
    // Turning this on is the opt-in: title, artist and album go to lrclib.net.
    QAction *lyricsView = menu.addAction(tr("Show lyrics (looked up on lrclib.net)"));
    lyricsView->setCheckable(true);
    lyricsView->setChecked(m_settings->lyricsEnabled());
    QAction *clickThrough = menu.addAction(tr("Click-through (use tray menu to undo)"));
    clickThrough->setCheckable(true);
    clickThrough->setChecked(m_settings->clickThrough());
    menu.addSeparator();
    QAction *hide = menu.addAction(tr("Hide overlay"));

    QAction *chosen = menu.exec(event->globalPos());
    if (chosen == expanded) {
        m_settings->setMode(expanded->isChecked()
                                ? OverlaySettings::Mode::Expanded
                                : OverlaySettings::Mode::Compact);
    } else if (chosen == clickThrough) {
        m_settings->setClickThrough(clickThrough->isChecked());
    } else if (chosen == lyricsView) {
        m_settings->setLyricsEnabled(lyricsView->isChecked());
        if (lyricsView->isChecked()) {
            m_settings->setLyricsView(true); // show them right away
        }
    } else if (chosen == hide) {
        m_settings->setEnabled(false);
    }
}

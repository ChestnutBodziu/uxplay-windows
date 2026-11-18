#include "overlaysettings.h"

#include <QVariant>
#include <algorithm>

namespace {
constexpr const char *kGroup = "overlay";
}

OverlaySettings::OverlaySettings(QObject *parent) : QObject(parent) {
    QSettings settings;
    settings.beginGroup(kGroup);
    m_enabled = settings.value("enabled", true).toBool();
    m_autoShow = settings.value("auto_show", true).toBool();
    m_alwaysOnTop = settings.value("always_on_top", true).toBool();
    m_hasPosition = settings.contains("x") && settings.contains("y");
    if (m_hasPosition) {
        m_position = QPoint(settings.value("x").toInt(),
                            settings.value("y").toInt());
    }
    m_scale = std::clamp(settings.value("scale", 100).toInt(), kMinScale,
                         kMaxScale);
    m_opacity = std::clamp(settings.value("opacity", 100).toInt(), kMinOpacity,
                           kMaxOpacity);
    m_mode = settings.value("mode", "compact").toString() == "expanded"
                 ? Mode::Expanded
                 : Mode::Compact;
    m_animations = settings.value("animations", true).toBool();
    m_clickThrough = settings.value("click_through", false).toBool();
    m_autoHide = settings.value("auto_hide", true).toBool();
    m_lyricsEnabled = settings.value("lyrics_enabled", false).toBool();
    m_lyricsView = settings.value("lyrics_view", false).toBool();
    m_lyricsOffsetMs = std::clamp(settings.value("lyrics_offset_ms", 0).toInt(),
                                  kMinLyricsOffsetMs, kMaxLyricsOffsetMs);
    m_hideDelay = std::clamp(settings.value("hide_delay_sec", 5).toInt(),
                             kMinHideDelaySec, kMaxHideDelaySec);
}

template <typename T>
void OverlaySettings::update(T &field, const T &value, const char *key) {
    if (field == value) {
        return;
    }
    field = value;
    QSettings settings;
    settings.beginGroup(kGroup);
    settings.setValue(key, QVariant::fromValue(value));
    emit changed();
}

void OverlaySettings::setEnabled(bool value) { update(m_enabled, value, "enabled"); }
void OverlaySettings::setAutoShow(bool value) { update(m_autoShow, value, "auto_show"); }
void OverlaySettings::setAlwaysOnTop(bool value) { update(m_alwaysOnTop, value, "always_on_top"); }
void OverlaySettings::setAnimations(bool value) { update(m_animations, value, "animations"); }
void OverlaySettings::setClickThrough(bool value) { update(m_clickThrough, value, "click_through"); }
void OverlaySettings::setAutoHide(bool value) { update(m_autoHide, value, "auto_hide"); }

void OverlaySettings::setScalePercent(int value) {
    update(m_scale, std::clamp(value, kMinScale, kMaxScale), "scale");
}

void OverlaySettings::setOpacityPercent(int value) {
    update(m_opacity, std::clamp(value, kMinOpacity, kMaxOpacity), "opacity");
}

void OverlaySettings::setLyricsEnabled(bool value) { update(m_lyricsEnabled, value, "lyrics_enabled"); }
void OverlaySettings::setLyricsView(bool value) { update(m_lyricsView, value, "lyrics_view"); }

void OverlaySettings::setLyricsOffsetMs(int value) {
    update(m_lyricsOffsetMs,
           std::clamp(value, kMinLyricsOffsetMs, kMaxLyricsOffsetMs),
           "lyrics_offset_ms");
}

void OverlaySettings::setHideDelaySec(int value) {
    update(m_hideDelay,
           std::clamp(value, kMinHideDelaySec, kMaxHideDelaySec),
           "hide_delay_sec");
}

void OverlaySettings::setMode(Mode value) {
    if (m_mode == value) {
        return;
    }
    m_mode = value;
    QSettings settings;
    settings.beginGroup(kGroup);
    settings.setValue("mode", value == Mode::Expanded ? "expanded" : "compact");
    emit changed();
}

void OverlaySettings::setPosition(const QPoint &value) {
    if (m_hasPosition && m_position == value) {
        return;
    }
    m_hasPosition = true;
    m_position = value;
    QSettings settings;
    settings.beginGroup(kGroup);
    settings.setValue("x", value.x());
    settings.setValue("y", value.y());
    emit changed();
}

void OverlaySettings::clearPosition() {
    if (!m_hasPosition) {
        return;
    }
    m_hasPosition = false;
    QSettings settings;
    settings.beginGroup(kGroup);
    settings.remove("x");
    settings.remove("y");
    emit changed();
}

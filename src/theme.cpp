#include "theme.h"

#include <QApplication>
#include <QColor>
#include <QFont>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QStyleFactory>
#include <QWidget>

#ifdef _WIN32
#include <windows.h>
#include <dwmapi.h>
#endif

namespace {

constexpr const char *kStyleSheet = R"(
* { font-family: "Segoe UI Variable Text", "Segoe UI"; }
QMainWindow, QDialog { background: #1b1b1f; }
QWidget#content { background: #1b1b1f; }
QListWidget#sidebar {
    background: #202024; border: none; outline: 0; padding: 8px 6px;
}
QListWidget#sidebar::item {
    color: #c9c9d0; padding: 10px 12px; margin: 2px 0; border-radius: 8px;
}
QListWidget#sidebar::item:hover { background: #2a2a30; }
QListWidget#sidebar::item:selected { background: #34343c; color: #ffffff; }
QLabel { color: #e6e6ea; }
QLabel#pageTitle { font-size: 22px; font-weight: 600; color: #ffffff; }
QLabel#sectionTitle { font-size: 13px; font-weight: 600; color: #8ab4f8; padding-top: 10px; }
QLabel#muted { color: #9a9aa4; }
QLabel#trackTitle { font-size: 20px; font-weight: 600; color: #ffffff; }
QLabel#statusChip {
    background: #2a2a30; border-radius: 10px; padding: 4px 12px; color: #c9c9d0;
}
QFrame#card { background: #25252a; border: 1px solid #303038; border-radius: 12px; }
QLabel#cover { background: #2a2a30; border-radius: 12px; color: #6a6a74; font-size: 54px; }
QPushButton {
    background: #2d2d34; color: #f0f0f4; border: 1px solid #3a3a43;
    border-radius: 8px; padding: 8px 16px;
}
QPushButton:hover { background: #373740; }
QPushButton:pressed { background: #2a2a30; }
QPushButton:disabled { color: #6a6a74; background: #26262b; }
QCheckBox { color: #e6e6ea; spacing: 10px; padding: 4px 0; }
QCheckBox::indicator {
    width: 18px; height: 18px; border-radius: 5px;
    border: 1px solid #5a5a66; background: #25252a;
}
QCheckBox::indicator:hover { border-color: #8ab4f8; }
QCheckBox::indicator:checked { background: #6cb6ff; border-color: #6cb6ff; image: none; }
QCheckBox:disabled { color: #6a6a74; }
QComboBox, QSpinBox {
    background: #2d2d34; color: #f0f0f4; border: 1px solid #3a3a43;
    border-radius: 8px; padding: 6px 10px; min-height: 22px;
}
QComboBox:hover, QSpinBox:hover { border-color: #5a5a66; }
QComboBox QAbstractItemView {
    background: #2d2d34; color: #f0f0f4; border: 1px solid #3a3a43;
    selection-background-color: #3d4a63; outline: 0;
}
QComboBox::drop-down { border: none; width: 24px; }
QSlider::groove:horizontal { height: 4px; background: #3a3a43; border-radius: 2px; }
QSlider::sub-page:horizontal { background: #6cb6ff; border-radius: 2px; }
QSlider::handle:horizontal {
    background: #ffffff; width: 16px; height: 16px; margin: -6px 0; border-radius: 8px;
}
QSlider::handle:horizontal:hover { background: #cfe6ff; }
QProgressBar { background: #3a3a43; border: none; border-radius: 2px; max-height: 4px; }
QProgressBar::chunk { background: #6cb6ff; border-radius: 2px; }
QMenu { background: #2d2d34; color: #f0f0f4; border: 1px solid #3a3a43; border-radius: 8px; padding: 6px; }
QMenu::item { padding: 7px 24px; border-radius: 6px; }
QMenu::item:selected { background: #3d4a63; }
QMenu::separator { height: 1px; background: #3a3a43; margin: 6px 8px; }
QToolTip { background: #2d2d34; color: #f0f0f4; border: 1px solid #3a3a43; padding: 4px 8px; }
QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar::handle:vertical { background: #44444d; border-radius: 4px; min-height: 30px; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; border: none; }
QPlainTextEdit { background: #16161a; color: #d8d8de; border: 1px solid #303038; border-radius: 8px; }
)";

} // namespace

void Theme::apply(QApplication &app) {
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QPalette palette;
    palette.setColor(QPalette::Window, QColor(0x1b, 0x1b, 0x1f));
    palette.setColor(QPalette::WindowText, QColor(0xe6, 0xe6, 0xea));
    palette.setColor(QPalette::Base, QColor(0x16, 0x16, 0x1a));
    palette.setColor(QPalette::AlternateBase, QColor(0x25, 0x25, 0x2a));
    palette.setColor(QPalette::Text, QColor(0xe6, 0xe6, 0xea));
    palette.setColor(QPalette::Button, QColor(0x2d, 0x2d, 0x34));
    palette.setColor(QPalette::ButtonText, QColor(0xf0, 0xf0, 0xf4));
    palette.setColor(QPalette::ToolTipBase, QColor(0x2d, 0x2d, 0x34));
    palette.setColor(QPalette::ToolTipText, QColor(0xf0, 0xf0, 0xf4));
    palette.setColor(QPalette::Highlight, QColor(0x6c, 0xb6, 0xff));
    palette.setColor(QPalette::HighlightedText, QColor(0x10, 0x10, 0x14));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(0x6a, 0x6a, 0x74));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0x6a, 0x6a, 0x74));
    app.setPalette(palette);

    QFont font(QStringLiteral("Segoe UI Variable Text"));
    font.setFamilies({QStringLiteral("Segoe UI Variable Text"),
                      QStringLiteral("Segoe UI")});
    font.setPointSize(10);
    app.setFont(font);
    app.setStyleSheet(QString::fromUtf8(kStyleSheet));
}

QIcon Theme::glyphIcon(QChar glyph, int size, const QColor &color) {
    QFont font(QStringLiteral("Segoe Fluent Icons"));
    font.setFamilies({QStringLiteral("Segoe Fluent Icons"),
                      QStringLiteral("Segoe MDL2 Assets")});
    font.setPixelSize(size);

    QPixmap pixmap(size * 2, size * 2); // 2x for sharp rendering on HiDPI
    pixmap.fill(Qt::transparent);
    {
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::TextAntialiasing);
        QFont big = font;
        big.setPixelSize(size * 2 - 4);
        painter.setFont(big);
        painter.setPen(color);
        painter.drawText(pixmap.rect(), Qt::AlignCenter, QString(glyph));
    }
    pixmap.setDevicePixelRatio(2.0);
    return QIcon(pixmap);
}

void Theme::useDarkTitleBar(QWidget *window) {
#ifdef _WIN32
    if (!window) {
        return;
    }
    const HWND hwnd = reinterpret_cast<HWND>(window->winId());
    const BOOL enabled = TRUE;
    // 20 = DWMWA_USE_IMMERSIVE_DARK_MODE (19 on early Windows 10 builds).
    if (FAILED(DwmSetWindowAttribute(hwnd, 20, &enabled, sizeof(enabled)))) {
        DwmSetWindowAttribute(hwnd, 19, &enabled, sizeof(enabled));
    }
#else
    Q_UNUSED(window);
#endif
}

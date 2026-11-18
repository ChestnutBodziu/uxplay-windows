#pragma once

#include <QChar>
#include <QIcon>

class QApplication;
class QWidget;

// Dark, Windows 11 / Fluent-inspired look for the whole application.
namespace Theme {

void apply(QApplication &app);

// Icon drawn from the Segoe Fluent Icons / MDL2 Assets symbol font.
QIcon glyphIcon(QChar glyph, int size, const QColor &color);

// Dark title bar for a top-level window (Windows 10 2004+ / Windows 11).
void useDarkTitleBar(QWidget *window);

} // namespace Theme

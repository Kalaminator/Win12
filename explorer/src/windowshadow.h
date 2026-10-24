#pragma once
#include <QColor>
#include <QImage>

class QWidget;

// Server-side drop shadow for the frameless window (KWin draws it outside the geometry).
namespace WindowShadow
{
QImage image(int radius, const QColor &colour);
void install(QWidget *window, int radius = 24);
}

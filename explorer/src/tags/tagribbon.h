#pragma once
#include "tag.h"
#include <QFont>
#include <QFontMetrics>
#include <QList>
#include <QRect>

class QPainter;

// Explorer's tag ribbon (arrow-ended pill, white text) and the Dolphin-style corner flags.
namespace TagRibbon {
constexpr int Height = 18, PadLeft = 8, PadRight = 14, Point = 8, Gap = 6;
QFont font(const QFont &base);                                                        // DemiBold, ~11 px tall
int width(const Tag &, const QFontMetrics &ribbonFm);                                 // PadLeft + text + PadRight
void paint(QPainter *, const QRect &slot, const Tag &, const QFontMetrics &ribbonFm); // left-aligned in the slot
void paintFlags(QPainter *, const QRect &iconRect, const QList<Tag> &);               // 12x5 stacked, bottom-right, <= 3
}

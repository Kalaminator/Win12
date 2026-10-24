#pragma once
#include <QByteArray>
#include <QList>
#include <QString>

// One emblem icon per colour set: the Dolphin overlay plugin hands KIconLoader a single name,
// so all of a file's flags stay together in the icon's bottom-right corner.
namespace Emblem {
QString iconName(const QList<int> &colourIndexes); // "win12-tags-ff8c00-e81123" (sorted by colour index, max 3, empty list -> "")
QString iconDir();                                 // GenericDataLocation + "/icons/hicolor/scalable/emblems"
QString ensure(const QList<int> &colourIndexes);   // writes <iconDir>/<name>.svg if missing, returns name ("" on failure)
QByteArray svg(const QList<int> &colourIndexes);   // 16x16 viewBox, 1-3 arrow flags 12x5 stacked bottom-right, 1 px gap
}

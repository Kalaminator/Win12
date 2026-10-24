#pragma once
#include <QColor>
#include <QList>
#include <QString>
#include <QStringList>

namespace TagColours {
constexpr int Count = 48;
QString name(int index);           // "YellowGold", "Gold", ...; index 0..47 (empty when out of range)
QColor colour(int index);          // windowsColours order from ConfigAppearance.qml
int indexOfName(const QString &);  // case-insensitive; -1 when unknown
int indexOfColour(const QColor &); // -1 when not one of the 48
}

struct Tag {
    int colour = -1; // index into TagColours; -1 = foreign (not written by Win12)
    QString word;    // may be empty for a colour-only tag; the raw string for foreign tags
    bool isForeign() const { return colour < 0; }
    QString encode() const; // "Red/Xiaomi"; colour-only -> "Red"; foreign -> word unchanged
    static Tag decode(const QString &);
    QString display() const;    // word, or the colour name for colour-only, or the raw foreign text
    QColor paintColour() const; // foreign -> "Overcast"
    bool operator==(const Tag &) const = default;
};

constexpr int MaxTags = 3;
QList<Tag> decodeAll(const QStringList &); // order kept
QStringList encodeAll(const QList<Tag> &);
int win12TagCount(const QList<Tag> &); // non-foreign only

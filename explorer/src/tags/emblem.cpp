#include "emblem.h"
#include "tag.h"
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QStandardPaths>
#include <algorithm>

namespace {
QList<int> normalised(QList<int> c)
{
    c.removeIf([](int i) { return i < 0 || i >= TagColours::Count; });
    std::sort(c.begin(), c.end());
    return c.mid(0, MaxTags);
}
QString hex(int i) { return TagColours::colour(i).name().mid(1); }
}

namespace Emblem {

QString iconName(const QList<int> &colourIndexes)
{
    const QList<int> c = normalised(colourIndexes);
    if (c.isEmpty())
        return {};
    QString n = QStringLiteral("win12-tags");
    for (int i : c)
        n += QLatin1Char('-') + hex(i);
    return n;
}

QString iconDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/icons/hicolor/scalable/emblems");
}

QByteArray svg(const QList<int> &colourIndexes)
{
    // Mockup .mf: 12x5, clip-path polygon(0 0,70% 0,100% 50%,70% 100%,0 100%); first colour on top.
    // ponytail: 3 flags need 17 px, so the top one loses its upper pixel row to the 16 px box.
    const QList<int> c = normalised(colourIndexes);
    QByteArray s = "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"16\" height=\"16\" viewBox=\"0 0 16 16\">\n";
    for (qsizetype k = 0; k < c.size(); ++k) {
        const int y = 11 - 6 * int(c.size() - 1 - k);
        s += QStringLiteral("<polygon points=\"4,%1 12.4,%1 16,%2 12.4,%3 4,%3\" fill=\"#%4\"/>\n")
                 .arg(y).arg(y + 2.5).arg(y + 5).arg(hex(c[k])).toUtf8();
    }
    return s + "</svg>\n";
}

QString ensure(const QList<int> &colourIndexes)
{
    const QString name = iconName(colourIndexes);
    if (name.isEmpty())
        return {};
    const QString path = iconDir() + QLatin1Char('/') + name + QStringLiteral(".svg");
    if (QFile::exists(path))
        return name;
    QSaveFile f(path); // atomic: KIconLoader never sees a half-written file
    if (!QDir().mkpath(iconDir()) || !f.open(QIODevice::WriteOnly) || f.write(svg(colourIndexes)) < 0 || !f.commit())
        return {};
    return name;
}

}

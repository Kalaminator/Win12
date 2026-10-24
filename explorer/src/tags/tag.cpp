#include "tag.h"

namespace
{
struct Entry {
    const char *name;
    const char *hex;
};
// Palette order = windowsColours in ConfigAppearance.qml (rows of 8).
const Entry kTable[TagColours::Count] = {
    {"YellowGold", "#ffb900"}, {"Gold", "#ff8c00"}, {"OrangeBright", "#f7630c"}, {"OrangeDark", "#ca5010"},
    {"Rust", "#da3b01"}, {"PaleRust", "#ef6950"}, {"BrickRed", "#d13438"}, {"ModRed", "#ff4343"},
    {"PaleRed", "#e74856"}, {"Red", "#e81123"}, {"RoseBright", "#ea005e"}, {"Rose", "#c30052"},
    {"PlumLight", "#e3008c"}, {"Plum", "#bf0077"}, {"OrchidLight", "#c239b3"}, {"Orchid", "#9a0089"},
    {"Blue", "#0078d7"}, {"NavyBlue", "#0063b1"}, {"PurpleShadow", "#8e8cd8"}, {"PurpleShadowDark", "#6b69d6"},
    {"IrisPastel", "#8764b8"}, {"IrisSpring", "#744da9"}, {"VioletRedLight", "#b146c2"}, {"VioletRed", "#881798"},
    {"CoolBlueBright", "#0099bc"}, {"CoolBlue", "#2d7d9a"}, {"Seafoam", "#00b7c3"}, {"SeafoamTeal", "#038387"},
    {"MintLight", "#00b294"}, {"MintDark", "#018574"}, {"TurfGreen", "#00cc6a"}, {"SportGreen", "#10893e"},
    {"Gray", "#7a7574"}, {"GrayBrown", "#5d5a58"}, {"SteelBlue", "#68768a"}, {"MetalBlue", "#515c6b"},
    {"PaleMoss", "#567c73"}, {"Moss", "#486860"}, {"MeadowGreen", "#498205"}, {"Green", "#107c10"},
    {"Overcast", "#767676"}, {"Storm", "#4c4a48"}, {"BlueGray", "#69797e"}, {"GrayDark", "#4a5459"},
    {"LiddyGreen", "#647c64"}, {"Sage", "#525e54"}, {"CamouflageDesert", "#847545"}, {"Camouflage", "#7e735f"},
};
}

namespace TagColours
{
QString name(int i)
{
    return i >= 0 && i < Count ? QString::fromLatin1(kTable[i].name) : QString();
}
QColor colour(int i)
{
    return i >= 0 && i < Count ? QColor(QLatin1String(kTable[i].hex)) : QColor();
}
int indexOfName(const QString &n)
{
    for (int i = 0; i < Count; ++i)
        if (n.compare(QLatin1String(kTable[i].name), Qt::CaseInsensitive) == 0)
            return i;
    return -1;
}
int indexOfColour(const QColor &c)
{
    for (int i = 0; i < Count; ++i)
        if (colour(i) == c)
            return i;
    return -1;
}
}

QString Tag::encode() const
{
    if (isForeign())
        return word;
    const QString w = word.trimmed();
    return w.isEmpty() ? TagColours::name(colour) : TagColours::name(colour) + QLatin1Char('/') + w;
}

Tag Tag::decode(const QString &s)
{
    const int slash = s.indexOf(QLatin1Char('/'));
    const int c = TagColours::indexOfName(slash < 0 ? s : s.left(slash));
    if (c < 0)
        return {-1, s};
    return {c, slash < 0 ? QString() : s.mid(slash + 1)};
}

QString Tag::display() const
{
    return isForeign() || !word.isEmpty() ? word : TagColours::name(colour);
}

QColor Tag::paintColour() const
{
    return TagColours::colour(isForeign() ? TagColours::indexOfName(QStringLiteral("Overcast")) : colour);
}

QList<Tag> decodeAll(const QStringList &l)
{
    QList<Tag> r;
    for (const QString &s : l)
        r << Tag::decode(s);
    return r;
}

QStringList encodeAll(const QList<Tag> &l)
{
    QStringList r;
    for (const Tag &t : l)
        r << t.encode();
    return r;
}

int win12TagCount(const QList<Tag> &l)
{
    int n = 0;
    for (const Tag &t : l)
        n += !t.isForeign();
    return n;
}

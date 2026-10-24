#include "sortgroup.h"
#include "tagstore.h"
#include <KDirModel>
#include <KLocalizedString>
#include <QCollator>

namespace SortGroup {

static GroupInfo modifiedGroup(const KFileItem &item, const QDateTime &now)
{
    const QDate d = item.time(KFileItem::ModificationTime).date(), n = now.date();
    const auto weekStart = [](const QDate &x) { return x.addDays(1 - x.dayOfWeek()); };
    const int months = (n.year() * 12 + n.month()) - (d.year() * 12 + d.month());
    if (d == n)
        return {0, i18n("Today")};
    if (d == n.addDays(-1))
        return {1, i18n("Yesterday")};
    if (weekStart(d) == weekStart(n))
        return {2, i18n("Earlier this week")};
    if (weekStart(d) == weekStart(n).addDays(-7))
        return {3, i18n("Last week")};
    if (months == 0)
        return {4, i18n("Earlier this month")};
    if (months == 1)
        return {5, i18n("Last month")};
    if (d.year() == n.year())
        return {6, i18n("Earlier this year")};
    return {7, i18n("A long time ago")};
}

static GroupInfo sizeGroup(const KFileItem &item)
{
    if (item.isDir())
        return {-1, i18n("Folders")};
    const quint64 s = item.size();
    constexpr quint64 KB = 1024, MB = KB * KB, GB = MB * KB;
    if (s == 0)
        return {0, i18n("Empty (0 KB)")};
    if (s <= 16 * KB)
        return {1, i18n("Tiny (0 - 16 KB)")};
    if (s <= MB)
        return {2, i18n("Small (16 KB - 1 MB)")};
    if (s <= 128 * MB)
        return {3, i18n("Medium (1 - 128 MB)")};
    if (s <= GB)
        return {4, i18n("Large (128 MB - 1 GB)")};
    if (s <= 4 * GB)
        return {5, i18n("Huge (1 - 4 GB)")};
    return {6, i18n("Gigantic (> 4 GB)")};
}

static GroupInfo nameGroup(const KFileItem &item)
{
    const QString name = item.name();
    const QChar c = name.isEmpty() ? QChar() : name.at(0).toLower();
    if (c >= QLatin1Char('0') && c <= QLatin1Char('9'))
        return {0, i18n("0-9")};
    if (c >= QLatin1Char('a') && c <= QLatin1Char('h'))
        return {1, i18n("A-H")};
    if (c >= QLatin1Char('i') && c <= QLatin1Char('p'))
        return {2, i18n("I-P")};
    if (c >= QLatin1Char('q') && c <= QLatin1Char('z'))
        return {3, i18n("Q-Z")};
    return {4, i18n("Other")};
}

QList<GroupInfo> tagGroups(const QList<Tag> &tags, GroupRole role)
{
    if (tags.isEmpty())
        return {{1000, i18n("No tag")}};
    QList<GroupInfo> out;
    for (const Tag &t : tags) {
        GroupInfo g;
        if (role == GroupRole::TagName)
            g = t.isForeign() ? GroupInfo{999, t.word} : GroupInfo{0, t.display()};
        else {
            const int c = t.isForeign() ? TagColours::indexOfName(QStringLiteral("Overcast")) : t.colour;
            g = {c, TagColours::name(c)};
        }
        if (!out.contains(g))
            out.append(g);
    }
    return out;
}

GroupInfo groupInfo(const KFileItem &item, GroupRole role, const QDateTime &now)
{
    switch (role) {
    case GroupRole::Modified:
        return modifiedGroup(item, now);
    case GroupRole::Size:
        return sizeGroup(item);
    case GroupRole::Name:
        return nameGroup(item);
    case GroupRole::Type: {
        if (item.isDir())
            return {0, i18n("File folder")};
        // Not mimeComment(): that stats the filesystem per item and reads .desktop files. currentMimeType() never
        // sniffs content; with delayedMimeTypes it is the extension-based guess until the real type is resolved.
        const QString display = item.entry().stringValue(KIO::UDSEntry::UDS_DISPLAY_TYPE);
        return {1, display.isEmpty() ? item.currentMimeType().comment() : display};
    }
    case GroupRole::TagName:
    case GroupRole::TagColour:
        return tagGroups(TagStore::read(item.url()), role).first();
    case GroupRole::None:
        break;
    }
    return {0, QString()};
}

bool groupLess(const GroupInfo &a, const GroupInfo &b)
{
    if (a.order != b.order)
        return a.order < b.order;
    if (a.label == b.label)
        return false;
    static const QCollator c = [] {
        QCollator x;
        x.setCaseSensitivity(Qt::CaseInsensitive);
        return x;
    }();
    return c.compare(a.label, b.label) < 0;
}

int sortColumnFor(SortRole r)
{
    switch (r) {
    case SortRole::Modified:
        return KDirModel::ModifiedTime;
    case SortRole::Type:
        return KDirModel::Type;
    case SortRole::Size:
        return KDirModel::Size;
    case SortRole::Name:
        break;
    }
    return KDirModel::Name;
}

}

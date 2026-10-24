#pragma once
#include "viewsettings.h"
#include <KFileItem>
#include "tag.h"
#include <QDateTime>
#include <QList>
#include <QString>

namespace SortGroup {
struct GroupInfo {
    int order;
    QString label;
    bool operator==(const GroupInfo &) const = default;
};
// TagName: one group per distinct display() (order 0); TagColour: one per colour (order = colour index).
// No tags -> {1000, ""} (drawn without a header); foreign tags -> {999, raw text} (TagName) / Overcast (TagColour).
QList<GroupInfo> tagGroups(const QList<Tag> &, GroupRole);
GroupInfo groupInfo(const KFileItem &, GroupRole, const QDateTime &now);
bool groupLess(const GroupInfo &, const GroupInfo &); // by order, then label (locale-aware, case-insensitive)
int sortColumnFor(SortRole); // KDirModel::Name, ModifiedTime, Type, Size
}

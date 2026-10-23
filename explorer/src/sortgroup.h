#pragma once
#include "viewsettings.h"
#include <KFileItem>
#include <QDateTime>
#include <QString>

namespace SortGroup {
struct GroupInfo {
    int order;
    QString label;
    bool operator==(const GroupInfo &) const = default;
};
GroupInfo groupInfo(const KFileItem &, GroupRole, const QDateTime &now);
bool groupLess(const GroupInfo &, const GroupInfo &); // by order, then label (locale-aware, case-insensitive)
int sortColumnFor(SortRole); // KDirModel::Name, ModifiedTime, Type, Size
}

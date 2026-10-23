#pragma once
#include <QDateTime>
#include <QList>
#include <QMetaType>
#include <QPair>
#include <QSet>
#include <QString>
#include <QUrl>

struct RenameBlock {
    enum Kind { Number, Text, CurrentName, Date, FindReplace, Case };
    Kind kind = Text;
    int digits = 2, start = 1, step = 1; // Number
    QString text; // Text; FindReplace: find; Date: format, default "yyyy-MM-dd"
    QString replace; // FindReplace
    enum CaseMode { Upper, Lower, Title };
    CaseMode caseMode = Title;
};
Q_DECLARE_METATYPE(RenameBlock)

struct RenameItem {
    QUrl url;
    QString name; // full current name
    QDateTime modified;
};

struct PlannedRename {
    QUrl url;
    QString oldName;
    QString newName;
    enum Status { Rename, Unchanged, ClashExisting, ClashPlanned };
    Status status = Unchanged;
};

namespace RenamePlan
{
// base name only, no extension
QString buildName(const QList<RenameBlock> &, const RenameItem &, int position);
QList<PlannedRename> plan(const QList<RenameBlock> &, const QList<RenameItem> &items, const QSet<QString> &existingNames);
// (newUrl -> oldName), reversed; only Status::Rename rows
QList<QPair<QUrl, QString>> undoPlan(const QList<PlannedRename> &executed);
}

#pragma once
#include <KFileItem>
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
    KFileItem item; // Rename many ▸ fx evaluates the formula on it
};

struct PlannedRename {
    QUrl url;
    QString oldName;
    QString newName;
    enum Status { Rename, Unchanged, ClashExisting, ClashPlanned, Invalid };
    Status status = Unchanged;
    QString note; // why a row is not a plain Rename
};

namespace RenamePlan
{
// base name only, no extension
QString buildName(const QList<RenameBlock> &, const RenameItem &, int position);
// baseNames[i] is item i's new name without extension (the extension is appended like plan())
QList<PlannedRename> planNames(const QList<QString> &baseNames, const QList<RenameItem> &items, const QSet<QString> &existingNames);
QList<PlannedRename> plan(const QList<RenameBlock> &, const QList<RenameItem> &items, const QSet<QString> &existingNames);
// (newUrl -> oldName), reversed; only Status::Rename rows
QList<QPair<QUrl, QString>> undoPlan(const QList<PlannedRename> &executed);
}

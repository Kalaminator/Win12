#include "renameplan.h"
#include <QHash>

namespace
{
// A name whose only dot is the leading one has no extension (whole name is the base).
int dotPos(const QString &name)
{
    int i = name.lastIndexOf(QLatin1Char('.'));
    return i > 0 ? i : -1;
}
QString baseOf(const QString &name)
{
    int i = dotPos(name);
    return i < 0 ? name : name.left(i);
}
QString extOf(const QString &name)
{
    int i = dotPos(name);
    return i < 0 ? QString() : name.mid(i);
}
QString title(const QString &s)
{
    QString r;
    bool up = true;
    for (QChar c : s) {
        r += up ? c.toUpper() : c.toLower();
        up = !c.isLetterOrNumber() && c != QLatin1Char('\'');
    }
    return r;
}
}

QString RenamePlan::buildName(const QList<RenameBlock> &blocks, const RenameItem &item, int position)
{
    QString s;
    for (const RenameBlock &b : blocks) {
        switch (b.kind) {
        case RenameBlock::Number:
            s += QString::number(b.start + b.step * position).rightJustified(b.digits, QLatin1Char('0'));
            break;
        case RenameBlock::Text:
            s += b.text;
            break;
        case RenameBlock::CurrentName:
            s += baseOf(item.name);
            break;
        case RenameBlock::Date:
            s += item.modified.toString(b.text.isEmpty() ? QStringLiteral("yyyy-MM-dd") : b.text);
            break;
        case RenameBlock::FindReplace:
            if (!b.text.isEmpty())
                s.replace(b.text, b.replace);
            break;
        case RenameBlock::Case:
            s = b.caseMode == RenameBlock::Upper ? s.toUpper() : b.caseMode == RenameBlock::Lower ? s.toLower() : title(s);
            break;
        }
    }
    return s;
}

QList<PlannedRename> RenamePlan::plan(const QList<RenameBlock> &blocks, const QList<RenameItem> &items, const QSet<QString> &existing)
{
    QList<PlannedRename> out;
    QHash<QString, int> count;
    for (int i = 0; i < items.size(); ++i) {
        const RenameItem &it = items[i];
        PlannedRename p{it.url, it.name, it.name, PlannedRename::Unchanged};
        const QString base = buildName(blocks, it, i);
        if (!base.isEmpty()) {
            p.newName = base + extOf(it.name);
            if (p.newName == it.name)
                p.status = PlannedRename::Unchanged;
            else if (existing.contains(p.newName))
                p.status = PlannedRename::ClashExisting;
            else {
                p.status = PlannedRename::Rename;
                ++count[p.newName];
            }
        }
        out << p;
    }
    for (PlannedRename &p : out)
        if (p.status == PlannedRename::Rename && count.value(p.newName) > 1)
            p.status = PlannedRename::ClashPlanned;
    return out;
}

QList<QPair<QUrl, QString>> RenamePlan::undoPlan(const QList<PlannedRename> &executed)
{
    QList<QPair<QUrl, QString>> r;
    for (int i = executed.size() - 1; i >= 0; --i) {
        const PlannedRename &p = executed[i];
        if (p.status != PlannedRename::Rename)
            continue;
        QUrl u = p.url.adjusted(QUrl::RemoveFilename);
        u.setPath(u.path() + p.newName);
        r.append({u, p.oldName});
    }
    return r;
}

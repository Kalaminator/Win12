#include "planstep.h"
#include <KLocalizedString>

QString PlanStep::actionText() const
{
    auto dest = [this] {
        const QString p = destFolder.toLocalFile();
        const QString b = baseFolder.endsWith(u'/') ? baseFolder : baseFolder + u'/';
        return !baseFolder.isEmpty() && p.startsWith(b) ? p.mid(b.size()) : p;
    };
    switch (kind) {
    case Move:
        return i18n("Move → %1", dest());
    case Copy:
        return i18n("Copy → %1", dest());
    case Rename:
        return i18n("Rename → %1", newName);
    case Tag:
        return i18n("Tag %1", tag.encode());
    case Untag:
        return i18n("Untag %1", tag.encode());
    case Trash:
        break;
    }
    return i18n("Trash");
}

QString PlanStep::fileText() const
{
    return source.fileName();
}

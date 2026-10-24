#include "win12tagsoverlay.h"
#include "emblem.h"
#include "tagstore.h"
#include <KConfigGroup>
#include <KSharedConfig>
#include <QUrl>

QStringList Win12TagsOverlay::getOverlays(const QUrl &url)
{
    if (!url.isLocalFile())
        return {};
    // ponytail: a KConfigWatcher + overlaysChanged for cached urls if F5 after toggling turns out to annoy
    if (!KSharedConfig::openConfig(QStringLiteral("win12-explorerrc"))->group(QStringLiteral("Tags")).readEntry("showFlags", true))
        return {};
    QList<int> colours;
    for (const Tag &t : TagStore::read(url))
        if (!t.isForeign())
            colours << t.colour;
    const QString name = Emblem::ensure(colours);
    return name.isEmpty() ? QStringList{} : QStringList{name};
}

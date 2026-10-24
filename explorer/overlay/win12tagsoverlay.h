#pragma once
#include <KOverlayIconPlugin>

// Dolphin's KFileItemModelRolesUpdater loads every plugin in kf6/overlayicon; the first
// overlay goes to the icon's bottom-right corner, so one emblem carries all of a file's flags.
class Win12TagsOverlay : public KOverlayIconPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.kde.overlayicon.win12tags")
public:
    using KOverlayIconPlugin::KOverlayIconPlugin;
    QStringList getOverlays(const QUrl &) override;
};

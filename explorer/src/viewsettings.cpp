#include "viewsettings.h"
#include <KDirModel>
#include <KLocalizedString>

int iconSizeFor(ViewMode m)
{
    switch (m) {
    case ViewMode::ExtraLargeIcons: return 256;
    case ViewMode::LargeIcons: return 96;
    case ViewMode::MediumIcons:
    case ViewMode::Tiles: return 48;
    case ViewMode::Content: return 32;
    default: return 16;
    }
}

ViewMode zoomStep(ViewMode m, int steps)
{
    static const ViewMode order[] = {ViewMode::Details, ViewMode::List, ViewMode::SmallIcons,
                                     ViewMode::MediumIcons, ViewMode::LargeIcons, ViewMode::ExtraLargeIcons};
    int i = 3; // Tiles and Content count as MediumIcons
    for (int k = 0; k < 6; ++k)
        if (order[k] == m)
            i = k;
    return order[qBound(0, i + steps, 5)];
}

QStringList knownColumnIds()
{
    return {QStringLiteral("name"), QStringLiteral("size"), QStringLiteral("modified"), QStringLiteral("permissions"),
            QStringLiteral("owner"), QStringLiteral("group"), QStringLiteral("type"), QStringLiteral("tags")};
}

int modelColumnFor(const QString &id)
{
    static const int cols[] = {KDirModel::Name, KDirModel::Size, KDirModel::ModifiedTime, KDirModel::Permissions,
                               KDirModel::Owner, KDirModel::Group, KDirModel::Type, 7};
    const int i = knownColumnIds().indexOf(id);
    return i < 0 ? -1 : cols[i];
}

QString columnTitle(const QString &id)
{
    switch (knownColumnIds().indexOf(id)) {
    case 0: return i18n("Name");
    case 1: return i18n("Size");
    case 2: return i18n("Date modified");
    case 3: return i18n("Permissions");
    case 4: return i18n("Owner");
    case 5: return i18n("Group");
    case 6: return i18n("Type");
    case 7: return i18n("Tags");
    }
    return id;
}

ViewSettingsStore::ViewSettingsStore(const KSharedConfig::Ptr &config)
    : m_config(config)
{
}

bool ViewSettingsStore::perFolder() const
{
    return m_config->group(QStringLiteral("View")).readEntry("PerFolder", false);
}

void ViewSettingsStore::setPerFolder(bool on)
{
    m_config->group(QStringLiteral("View")).writeEntry("PerFolder", on);
    m_config->sync();
}

KConfigGroup ViewSettingsStore::folderGroup(const QUrl &folder) const
{
    return m_config->group(QStringLiteral("Folders")).group(folder.toString(QUrl::FullyEncoded));
}

bool ViewSettingsStore::hasFolderSettings(const QUrl &folder) const
{
    return m_config->group(QStringLiteral("Folders")).hasGroup(folder.toString(QUrl::FullyEncoded));
}

void ViewSettingsStore::clearFolder(const QUrl &folder)
{
    folderGroup(folder).deleteGroup();
    m_config->sync();
}

ViewSettings ViewSettingsStore::load(const QUrl &folder) const
{
    const KConfigGroup g = (perFolder() && hasFolderSettings(folder)) ? folderGroup(folder) : m_config->group(QStringLiteral("View"));
    const ViewSettings d;
    ViewSettings s;
    s.mode = ViewMode(g.readEntry("Mode", int(d.mode)));
    s.sortRole = SortRole(g.readEntry("SortRole", int(d.sortRole)));
    s.sortOrder = Qt::SortOrder(g.readEntry("SortOrder", int(d.sortOrder)));
    s.groupRole = GroupRole(g.readEntry("GroupRole", int(d.groupRole)));
    s.columns = g.readEntry("Columns", d.columns);
    s.hiddenFiles = g.readEntry("HiddenFiles", d.hiddenFiles);
    s.showExtensions = g.readEntry("ShowExtensions", d.showExtensions);
    s.checkBoxes = g.readEntry("CheckBoxes", d.checkBoxes);
    s.previews = g.readEntry("Previews", d.previews);
    return s;
}

void ViewSettingsStore::save(const QUrl &folder, const ViewSettings &s)
{
    KConfigGroup g = perFolder() ? folderGroup(folder) : m_config->group(QStringLiteral("View"));
    g.writeEntry("Mode", int(s.mode));
    g.writeEntry("SortRole", int(s.sortRole));
    g.writeEntry("SortOrder", int(s.sortOrder));
    g.writeEntry("GroupRole", int(s.groupRole));
    g.writeEntry("Columns", s.columns);
    g.writeEntry("HiddenFiles", s.hiddenFiles);
    g.writeEntry("ShowExtensions", s.showExtensions);
    g.writeEntry("CheckBoxes", s.checkBoxes);
    g.writeEntry("Previews", s.previews);
    m_config->sync();
}

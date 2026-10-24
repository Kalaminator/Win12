#pragma once
#include <KConfigGroup>
#include <KSharedConfig>
#include <QStringList>
#include <QUrl>

enum class ViewMode { ExtraLargeIcons, LargeIcons, MediumIcons, SmallIcons, List, Details, Tiles, Content };
enum class SortRole { Name, Modified, Type, Size };
enum class GroupRole { None, Name, Modified, Type, Size, TagName, TagColour };

struct ViewSettings {
    ViewMode mode = ViewMode::Details;
    SortRole sortRole = SortRole::Name;
    Qt::SortOrder sortOrder = Qt::AscendingOrder;
    GroupRole groupRole = GroupRole::None;
    QStringList columns = {QStringLiteral("name"), QStringLiteral("modified"), QStringLiteral("type"), QStringLiteral("size")};
    bool hiddenFiles = false, showExtensions = true, checkBoxes = false, previews = true;
    bool operator==(const ViewSettings &) const = default;
};

int iconSizeFor(ViewMode);
ViewMode zoomStep(ViewMode, int steps);
QStringList knownColumnIds();
int modelColumnFor(const QString &id);
QString columnTitle(const QString &id);

class ViewSettingsStore
{
public:
    explicit ViewSettingsStore(const KSharedConfig::Ptr &config);
    bool perFolder() const;
    void setPerFolder(bool);
    ViewSettings load(const QUrl &folder) const;
    void save(const QUrl &folder, const ViewSettings &);
    bool hasFolderSettings(const QUrl &) const;
    void clearFolder(const QUrl &);

private:
    KConfigGroup folderGroup(const QUrl &) const;
    KSharedConfig::Ptr m_config;
};

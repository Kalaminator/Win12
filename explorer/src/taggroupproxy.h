#pragma once
#include "sortgroup.h"
#include "tag.h"
#include "viewsettings.h"
#include <QAbstractProxyModel>
#include <QHash>
#include <QPersistentModelIndex>
#include <QUrl>

// Flat proxy between FolderModel and TagsColumns. Grouped by tag name or colour, a file gets one row in every group it
// matches; any other group role maps rows 1:1. Also owns the per-url tag cache (TagsColumns reads it).
class TagGroupProxy : public QAbstractProxyModel
{
    Q_OBJECT
public:
    explicit TagGroupProxy(QObject *parent = nullptr);
    void setSourceModel(QAbstractItemModel *) override;
    void setGroupRole(GroupRole); // TagName/TagColour duplicate rows; anything else = identity mapping
    GroupRole groupRole() const;
    QList<Tag> tags(const QUrl &) const; // TagStore::read, cached; a source dataChanged/removal drops its rows' entries
    void invalidateTags(const QList<QUrl> &); // drops cached tags; tag mode regroups

    // identity: rows == source rows; tag mode: one row per (source row, group), ordered by groupLess then source row
    QModelIndex mapToSource(const QModelIndex &) const override;
    QModelIndex mapFromSource(const QModelIndex &) const override; // the first duplicate
    QModelIndex index(int row, int column, const QModelIndex &parent = {}) const override;
    QModelIndex parent(const QModelIndex &) const override;
    QModelIndex sibling(int row, int column, const QModelIndex &) const override;
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    bool hasChildren(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &, int role = Qt::DisplayRole) const override; // Group*Role answered here in tag mode

private:
    bool tagMode() const;
    QUrl urlOf(int sourceRow) const;
    void rebuild();
    void regroup(); // reset + rebuild

    struct Row {
        int source;
        int group; // index into m_groups
    };
    GroupRole m_role = GroupRole::None;
    QList<SortGroup::GroupInfo> m_groups; // tag mode, in groupLess order
    QList<Row> m_rows; // tag mode: proxy row -> source row + group
    QList<QList<int>> m_proxyRows; // tag mode: source row -> its proxy rows
    QModelIndexList m_layoutProxy; // identity layoutChanged: persistent indexes and their sources
    QList<QPersistentModelIndex> m_layoutSource;
    mutable QHash<QUrl, QList<Tag>> m_cache;
};

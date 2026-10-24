#include "taggroupproxy.h"
#include "foldermodel.h"
#include "tagstore.h"
#include <KDirModel>
#include <algorithm>

TagGroupProxy::TagGroupProxy(QObject *parent)
    : QAbstractProxyModel(parent)
{
}

void TagGroupProxy::setSourceModel(QAbstractItemModel *source)
{
    beginResetModel();
    if (sourceModel())
        disconnect(sourceModel(), nullptr, this, nullptr);
    QAbstractProxyModel::setSourceModel(source);
    m_cache.clear();
    if (source) {
        // ponytail: tag mode rebuilds the whole row map inside a reset; fine-grained row maps when folders with thousands of tagged files feel slow
        connect(source, &QAbstractItemModel::modelAboutToBeReset, this, [this] {
            beginResetModel();
        });
        connect(source, &QAbstractItemModel::modelReset, this, [this] {
            m_cache.clear();
            rebuild();
            endResetModel();
        });
        connect(source, &QAbstractItemModel::rowsAboutToBeInserted, this, [this](const QModelIndex &p, int first, int last) {
            if (p.isValid())
                return; // flat
            tagMode() ? beginResetModel() : beginInsertRows({}, first, last);
        });
        connect(source, &QAbstractItemModel::rowsInserted, this, [this](const QModelIndex &p) {
            if (p.isValid())
                return;
            if (!tagMode())
                return endInsertRows();
            rebuild();
            endResetModel();
        });
        connect(source, &QAbstractItemModel::rowsAboutToBeRemoved, this, [this](const QModelIndex &p, int first, int last) {
            if (p.isValid())
                return;
            for (int r = first; r <= last; ++r)
                m_cache.remove(urlOf(r));
            tagMode() ? beginResetModel() : beginRemoveRows({}, first, last);
        });
        connect(source, &QAbstractItemModel::rowsRemoved, this, [this](const QModelIndex &p) {
            if (p.isValid())
                return;
            if (!tagMode())
                return endRemoveRows();
            rebuild();
            endResetModel();
        });
        connect(source, &QAbstractItemModel::rowsAboutToBeMoved, this, [this](const QModelIndex &sp, int first, int last, const QModelIndex &dp, int dest) {
            if (sp.isValid() || dp.isValid())
                return;
            tagMode() ? beginResetModel() : (void)beginMoveRows({}, first, last, {}, dest);
        });
        connect(source, &QAbstractItemModel::rowsMoved, this, [this](const QModelIndex &sp, int, int, const QModelIndex &dp) {
            if (sp.isValid() || dp.isValid())
                return;
            if (!tagMode())
                return endMoveRows();
            rebuild();
            endResetModel();
        });
        connect(source, &QAbstractItemModel::layoutAboutToBeChanged, this, [this] {
            if (tagMode())
                return beginResetModel();
            Q_EMIT layoutAboutToBeChanged();
            m_layoutProxy = persistentIndexList();
            for (const QModelIndex &i : std::as_const(m_layoutProxy))
                m_layoutSource << mapToSource(i);
        });
        connect(source, &QAbstractItemModel::layoutChanged, this, [this] {
            if (tagMode()) {
                rebuild();
                return endResetModel();
            }
            for (qsizetype i = 0; i < m_layoutProxy.size(); ++i)
                changePersistentIndex(m_layoutProxy.at(i), mapFromSource(m_layoutSource.at(i)));
            m_layoutProxy.clear();
            m_layoutSource.clear();
            Q_EMIT layoutChanged();
        });
        connect(source, &QAbstractItemModel::dataChanged, this, [this](const QModelIndex &tl, const QModelIndex &br, const QList<int> &roles) {
            if (tl.parent().isValid())
                return;
            // KDirModel emits dataChanged for every icon/preview/mimetype update: re-read only those rows, and only when text may have changed.
            if (roles.isEmpty() || roles.contains(Qt::DisplayRole)) {
                bool moved = false;
                for (int r = tl.row(); r <= br.row(); ++r) {
                    const QUrl url = urlOf(r);
                    const QList<Tag> before = m_cache.take(url);
                    moved |= tagMode() && tags(url) != before;
                }
                if (moved)
                    return regroup();
            }
            if (!tagMode())
                return Q_EMIT dataChanged(index(tl.row(), tl.column()), index(br.row(), br.column()), roles);
            for (int r = tl.row(); r <= br.row(); ++r)
                for (int pr : m_proxyRows.value(r))
                    Q_EMIT dataChanged(index(pr, tl.column()), index(pr, br.column()), roles);
        });
        connect(source, &QAbstractItemModel::headerDataChanged, this, &QAbstractItemModel::headerDataChanged);
        // ponytail: column inserts/removals are not forwarded; FolderModel's columns never change
    }
    rebuild();
    endResetModel();
}

void TagGroupProxy::setGroupRole(GroupRole role)
{
    if (role == m_role)
        return;
    const bool was = tagMode();
    m_role = role;
    if (was || tagMode()) {
        beginResetModel();
        rebuild();
        endResetModel();
    }
}

GroupRole TagGroupProxy::groupRole() const
{
    return m_role;
}

bool TagGroupProxy::tagMode() const
{
    return m_role == GroupRole::TagName || m_role == GroupRole::TagColour;
}

QList<Tag> TagGroupProxy::tags(const QUrl &url) const
{
    auto it = m_cache.find(url);
    if (it == m_cache.end()) // ponytail: one getxattr per row, the first time the row is read (paint, grouping, or the view's slot-width scan); move to the PreviewLoader thread if a network mount is slow
        it = m_cache.insert(url, TagStore::read(url));
    return *it;
}

void TagGroupProxy::invalidateTags(const QList<QUrl> &urls)
{
    for (const QUrl &u : urls)
        m_cache.remove(u);
    if (tagMode())
        regroup();
}

QUrl TagGroupProxy::urlOf(int sourceRow) const
{
    return sourceModel()->index(sourceRow, 0).data(KDirModel::FileItemRole).value<KFileItem>().url();
}

void TagGroupProxy::regroup()
{
    beginResetModel();
    rebuild();
    endResetModel();
}

void TagGroupProxy::rebuild()
{
    m_groups.clear();
    m_rows.clear();
    m_proxyRows.clear();
    if (!tagMode() || !sourceModel())
        return;
    const int n = sourceModel()->rowCount();
    QList<std::pair<SortGroup::GroupInfo, int>> pairs;
    for (int r = 0; r < n; ++r)
        for (const SortGroup::GroupInfo &g : SortGroup::tagGroups(tags(urlOf(r)), m_role))
            pairs.append({g, r});
    std::stable_sort(pairs.begin(), pairs.end(), [](const auto &a, const auto &b) {
        return SortGroup::groupLess(a.first, b.first);
    });
    m_proxyRows.resize(n);
    for (const auto &[g, r] : std::as_const(pairs)) {
        if (m_groups.isEmpty() || SortGroup::groupLess(m_groups.last(), g))
            m_groups.append(g);
        const int group = int(m_groups.size()) - 1;
        QList<int> &rows = m_proxyRows[r];
        if (!rows.isEmpty() && m_rows.at(rows.last()).group == group)
            continue; // two of the file's tags fall in one group
        rows.append(int(m_rows.size()));
        m_rows.append({r, group});
    }
}

QModelIndex TagGroupProxy::mapToSource(const QModelIndex &proxy) const
{
    if (!proxy.isValid() || !sourceModel())
        return {};
    return sourceModel()->index(tagMode() ? m_rows.at(proxy.row()).source : proxy.row(), proxy.column());
}

QModelIndex TagGroupProxy::mapFromSource(const QModelIndex &source) const
{
    if (!source.isValid() || source.parent().isValid())
        return {};
    if (!tagMode())
        return index(source.row(), source.column());
    const QList<int> rows = m_proxyRows.value(source.row());
    return rows.isEmpty() ? QModelIndex() : index(rows.first(), source.column());
}

QModelIndex TagGroupProxy::index(int row, int column, const QModelIndex &parent) const
{
    return hasIndex(row, column, parent) ? createIndex(row, column) : QModelIndex();
}

QModelIndex TagGroupProxy::parent(const QModelIndex &) const
{
    return {};
}

QModelIndex TagGroupProxy::sibling(int row, int column, const QModelIndex &) const
{
    return index(row, column); // the base maps through the source, which would land on the first duplicate
}

int TagGroupProxy::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid() || !sourceModel())
        return 0;
    return tagMode() ? int(m_rows.size()) : sourceModel()->rowCount();
}

int TagGroupProxy::columnCount(const QModelIndex &) const
{
    return sourceModel() ? sourceModel()->columnCount() : 0;
}

bool TagGroupProxy::hasChildren(const QModelIndex &parent) const
{
    return !parent.isValid() && rowCount() > 0;
}

QVariant TagGroupProxy::data(const QModelIndex &index, int role) const
{
    if (tagMode() && index.isValid()) {
        const Row &row = m_rows.at(index.row());
        switch (role) {
        case FolderModel::GroupOrderRole:
            return m_groups.at(row.group).order;
        case FolderModel::GroupLabelRole:
            return m_groups.at(row.group).label;
        case FolderModel::GroupFirstRole:
            return index.row() == 0 || m_rows.at(index.row() - 1).group != row.group;
        }
    }
    return QAbstractProxyModel::data(index, role);
}

#include "foldermodel.h"
#include "tagstore.h"
#include <KDirLister>
#include <KIO/PreviewJob>
#include <KLocalizedString>
#include <QAbstractItemView>
#include <QScrollBar>
#include <QSet>

FolderModel::FolderModel(QObject *parent)
    : KDirSortFilterProxyModel(parent)
    , m_dirModel(new KDirModel(this))
{
    lister()->setAutoUpdate(true);
    lister()->setDelayedMimeTypes(true);
    // Connected before setSourceModel so the group cache is fixed before the proxy re-sorts.
    connect(m_dirModel, &QAbstractItemModel::dataChanged, this, [this](const QModelIndex &tl, const QModelIndex &br) {
        for (int r = tl.row(); r <= br.row(); ++r)
            m_groups.remove(m_dirModel->index(r, 0, tl.parent()).internalPointer());
    });
    connect(m_dirModel, &QAbstractItemModel::rowsAboutToBeRemoved, this, &FolderModel::clearGroups);
    connect(m_dirModel, &QAbstractItemModel::modelAboutToBeReset, this, &FolderModel::clearGroups);
    setSourceModel(m_dirModel);
    setSorting(SortRole::Name, Qt::AscendingOrder);
}

KDirModel *FolderModel::dirModel() const
{
    return m_dirModel;
}

KDirLister *FolderModel::lister() const
{
    return m_dirModel->dirLister();
}

void FolderModel::openUrl(const QUrl &url)
{
    lister()->openUrl(url, KDirLister::NoFlags);
}

QUrl FolderModel::url() const
{
    return lister()->url();
}

void FolderModel::setGroupRole(GroupRole role)
{
    m_groupRole = role;
    clearGroups();
    invalidate();
}

GroupRole FolderModel::groupRole() const
{
    return m_groupRole;
}

void FolderModel::setSorting(SortRole role, Qt::SortOrder order)
{
    clearGroups();
    sort(SortGroup::sortColumnFor(role), order);
}

void FolderModel::setPreviewLoader(PreviewLoader *loader)
{
    if (m_loader)
        disconnect(m_loader, nullptr, this, nullptr);
    m_loader = loader;
    if (loader)
        connect(loader, &PreviewLoader::previewReady, this, [this](const QUrl &url) {
            const QModelIndex idx = indexFor(url);
            if (idx.isValid())
                Q_EMIT dataChanged(idx, idx, {Qt::DecorationRole, PreviewRole});
        });
}

KFileItem FolderModel::itemFor(const QModelIndex &proxyIndex) const
{
    return m_dirModel->itemForIndex(mapToSource(proxyIndex));
}

QModelIndex FolderModel::indexFor(const QUrl &url) const
{
    return mapFromSource(m_dirModel->indexForUrl(url));
}

QVariant FolderModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return KDirSortFilterProxyModel::data(index, role);
    switch (role) {
    case GroupOrderRole:
        return groupOf(mapToSource(index)).order;
    case GroupLabelRole:
        return groupOf(mapToSource(index)).label;
    case GroupFirstRole:
        return index.row() == 0 || groupOf(mapToSource(index)) != groupOf(mapToSource(this->index(index.row() - 1, 0, index.parent())));
    case PreviewRole: {
        const QPixmap pix = previewOf(index);
        return pix.isNull() ? QVariant() : QVariant(pix);
    }
    case Qt::DisplayRole: // Windows details text
        if (index.column() == KDirModel::Size && itemFor(index).isDir())
            return QString(); // folders show no size
        if (index.column() == KDirModel::ModifiedTime)
            return QLocale().toString(itemFor(index).time(KFileItem::ModificationTime), QLocale::ShortFormat);
        break;
    case Qt::TextAlignmentRole:
        if (index.column() == KDirModel::Size)
            return QVariant::fromValue(Qt::AlignRight | Qt::AlignVCenter);
        break;
    case Qt::DecorationRole:
        if (index.column() == 0) {
            const QPixmap pix = previewOf(index);
            if (!pix.isNull())
                return pix;
        }
        break;
    }
    return KDirSortFilterProxyModel::data(index, role);
}

bool FolderModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    if (m_groupRole != GroupRole::None) {
        // Groups stay ascending whatever the sort order (the proxy swaps arguments for descending).
        const bool asc = sortOrder() == Qt::AscendingOrder;
        const SortGroup::GroupInfo a = groupOf(left), b = groupOf(right);
        if (SortGroup::groupLess(a, b))
            return asc;
        if (SortGroup::groupLess(b, a))
            return !asc;
    }
    if (sortColumn() == KDirModel::Type) {
        // The base compares mimeComment(), which sniffs content and reads .desktop files per item;
        // use the same cheap label as the Type groups, then the base's name order (folders first).
        const KFileItem a = m_dirModel->itemForIndex(left), b = m_dirModel->itemForIndex(right);
        if (a.isDir() == b.isDir()) {
            const SortGroup::GroupInfo ta = SortGroup::groupInfo(a, GroupRole::Type, m_now);
            const SortGroup::GroupInfo tb = SortGroup::groupInfo(b, GroupRole::Type, m_now);
            if (SortGroup::groupLess(ta, tb))
                return true;
            if (SortGroup::groupLess(tb, ta))
                return false;
        }
        return KDirSortFilterProxyModel::lessThan(left.siblingAtColumn(KDirModel::Name), right.siblingAtColumn(KDirModel::Name));
    }
    return KDirSortFilterProxyModel::lessThan(left, right); // folders first inside a group
}

SortGroup::GroupInfo FolderModel::groupOf(const QModelIndex &sourceIndex) const
{
    const void *key = sourceIndex.internalPointer();
    const auto it = m_groups.constFind(key);
    if (it != m_groups.cend())
        return *it;
    if (m_groups.isEmpty())
        m_now = QDateTime::currentDateTime();
    return *m_groups.insert(key, SortGroup::groupInfo(m_dirModel->itemForIndex(sourceIndex), m_groupRole, m_now));
}

QPixmap FolderModel::previewOf(const QModelIndex &proxyIndex) const
{
    return m_loader ? m_loader->previewFor(itemFor(proxyIndex)) : QPixmap();
}

void FolderModel::clearGroups()
{
    m_groups.clear();
}

TagsColumns::TagsColumns(FolderModel *source, QObject *parent)
    : KExtraColumnsProxyModel(parent)
{
    appendColumn(i18n("Tags"));
    setSourceModel(source);
    auto clear = [this] { m_cache.clear(); };
    connect(source, &QAbstractItemModel::modelReset, this, clear);
    connect(source, &QAbstractItemModel::rowsRemoved, this, clear);
    connect(source, &QAbstractItemModel::dataChanged, this, clear);
}

FolderModel *TagsColumns::folderModel() const
{
    return static_cast<FolderModel *>(sourceModel());
}

KFileItem TagsColumns::itemFor(const QModelIndex &idx) const
{
    // Extra columns have no source index; go through column 0 of the same row.
    return folderModel()->itemFor(mapToSource(index(idx.row(), 0, idx.parent())));
}

QVariant TagsColumns::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
        for (const QString &id : knownColumnIds())
            if (modelColumnFor(id) == section)
                return columnTitle(id);
    return KExtraColumnsProxyModel::headerData(section, orientation, role);
}

QVariant TagsColumns::extraColumnData(const QModelIndex &parent, int row, int, int role) const
{
    if (role != TagsRole && role != Qt::DisplayRole && role != Qt::ToolTipRole)
        return {};
    const QUrl url = itemFor(index(row, 0, parent)).url();
    auto it = m_cache.find(url);
    if (it == m_cache.end()) // ponytail: one getxattr per visible row on first paint; move to the PreviewLoader thread if a network mount is slow
        it = m_cache.insert(url, TagStore::read(url));
    if (role == TagsRole)
        return QVariant::fromValue(*it);
    return encodeAll(*it).join(QStringLiteral(", "));
}

void TagsColumns::invalidateTags(const QList<QUrl> &urls)
{
    for (const QUrl &u : urls)
        m_cache.remove(u);
    const QSet<QUrl> set(urls.begin(), urls.end());
    for (int r = 0; r < rowCount(); ++r)
        if (set.contains(itemFor(index(r, 0)).url()))
            Q_EMIT dataChanged(index(r, 0), index(r, TagsColumn));
}

static std::pair<QUrl, qint64> cacheKey(const KFileItem &item)
{
    return {item.url(), item.time(KFileItem::ModificationTime).toMSecsSinceEpoch()};
}

PreviewLoader::PreviewLoader(FolderModel *model, QObject *parent)
    : QObject(parent)
    , m_model(model)
    , m_plugins(KIO::PreviewJob::availablePlugins())
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(50);
    connect(&m_debounce, &QTimer::timeout, this, &PreviewLoader::start);
    // A new folder (or reload) resets the model: drop its previews so the cache stays bounded.
    connect(model, &QAbstractItemModel::modelReset, this, [this] {
        stop();
        clearCache();
    });
}

void PreviewLoader::setView(QAbstractItemView *view)
{
    if (m_view) {
        disconnect(m_view->verticalScrollBar(), nullptr, this, nullptr);
        disconnect(m_view->horizontalScrollBar(), nullptr, this, nullptr);
    }
    m_view = view;
    if (view) {
        connect(view->verticalScrollBar(), &QScrollBar::valueChanged, this, &PreviewLoader::requestVisible);
        connect(view->horizontalScrollBar(), &QScrollBar::valueChanged, this, &PreviewLoader::requestVisible);
    } else {
        stop();
    }
}

void PreviewLoader::setEnabled(bool on)
{
    if (on == m_enabled)
        return;
    m_enabled = on;
    if (on) {
        requestVisible();
    } else {
        stop();
        clearCache();
    }
}

void PreviewLoader::setSize(int px)
{
    if (px == m_size)
        return;
    m_size = px;
    stop();
    clearCache();
    requestVisible();
}

QPixmap PreviewLoader::previewFor(const KFileItem &item) const
{
    return m_cache.value(cacheKey(item));
}

void PreviewLoader::requestVisible()
{
    if (m_enabled && m_view)
        m_debounce.start();
}

void PreviewLoader::start()
{
    stop();
    const int rows = m_model->rowCount();
    if (!m_enabled || !m_view || rows == 0)
        return;
    // View rows equal FolderModel rows (TagsColumns is an identity proxy).
    const QRect r = m_view->viewport()->rect();
    const QModelIndex top = m_view->indexAt(r.topLeft());
    QModelIndex bottom = m_view->indexAt(r.bottomRight());
    if (!bottom.isValid())
        bottom = m_view->indexAt(r.bottomLeft());
    // ponytail: corner probes; a gap under both bottom corners counts every row after `top` as visible.
    const int first = top.isValid() ? top.row() : 0;
    const int last = bottom.isValid() ? bottom.row() : rows - 1;
    const auto enqueue = [this](int from, int to) {
        for (int row = from; row <= to; ++row) {
            const KFileItem item = m_model->itemFor(m_model->index(row, 0));
            if (!m_cache.contains(cacheKey(item)))
                m_queue.append(item);
        }
    };
    enqueue(first, last);
    m_nextBatch = m_queue.isEmpty() ? 100 : int(m_queue.size());
    enqueue(last + 1, rows - 1);
    enqueue(0, first - 1);
    startNextBatch();
}

void PreviewLoader::startNextBatch()
{
    if (m_queue.isEmpty())
        return;
    const int n = qMin(m_nextBatch, int(m_queue.size()));
    m_nextBatch = 100;
    const KFileItemList batch = m_queue.mid(0, n);
    m_queue.remove(0, n);
    m_job = new KIO::PreviewJob(batch, QSize(m_size, m_size), &m_plugins);
    connect(m_job, &KIO::PreviewJob::gotPreview, this, [this](const KFileItem &item, const QPixmap &pix) {
        m_cache.insert(cacheKey(item), pix);
        Q_EMIT previewReady(item.url());
    });
    connect(m_job, &KIO::PreviewJob::failed, this, [this](const KFileItem &item) {
        m_cache.insert(cacheKey(item), QPixmap());
    });
    connect(m_job, &KJob::result, this, &PreviewLoader::startNextBatch);
}

void PreviewLoader::stop()
{
    m_debounce.stop();
    m_queue.clear();
    if (m_job)
        m_job->kill(); // quiet kill: no result signal, so no next batch
}

void PreviewLoader::clearCache()
{
    m_cache.clear();
    if (m_view)
        m_view->viewport()->update();
}

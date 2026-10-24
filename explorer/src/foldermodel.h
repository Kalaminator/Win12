#pragma once
#include "sortgroup.h"
#include "tag.h"
#include "viewsettings.h"
#include <KDirModel>
#include <KDirSortFilterProxyModel>
#include <KExtraColumnsProxyModel>
#include <KFileItem>
#include <QHash>
#include <QUrl>
#include <QPixmap>
#include <QPointer>
#include <QTimer>

class KDirLister;
class QAbstractItemView;
class TagGroupProxy;
namespace KIO { class PreviewJob; }
class PreviewLoader;

// KDirModel (own KDirLister: autoUpdate, delayedMimeTypes) sorted and grouped Windows-style.
class FolderModel : public KDirSortFilterProxyModel
{
    Q_OBJECT
public:
    enum Roles { GroupOrderRole = Qt::UserRole + 100, GroupLabelRole, GroupFirstRole /*bool: first row of its group*/, PreviewRole /*QPixmap or invalid*/ };
    explicit FolderModel(QObject *parent = nullptr);
    KDirModel *dirModel() const;
    KDirLister *lister() const;
    void openUrl(const QUrl &);
    QUrl url() const;
    void setGroupRole(GroupRole); // invalidates sort
    GroupRole groupRole() const;
    void setSorting(SortRole, Qt::SortOrder);
    void setPreviewLoader(PreviewLoader *); // Qt::DecorationRole on column 0 returns the preview when one is cached
    KFileItem itemFor(const QModelIndex &proxyIndex) const;
    QModelIndex indexFor(const QUrl &) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

protected:
    bool lessThan(const QModelIndex &, const QModelIndex &) const override;

private:
    SortGroup::GroupInfo groupOf(const QModelIndex &sourceIndex) const; // by value: a later insert may rehash
    QPixmap previewOf(const QModelIndex &proxyIndex) const;
    void clearGroups();

    KDirModel *m_dirModel;
    GroupRole m_groupRole = GroupRole::None;
    QPointer<PreviewLoader> m_loader;
    // Keyed by KDirModel node (index.internalPointer()): stable while rows are inserted; dropped on change/removal/reset.
    mutable QHash<const void *, SortGroup::GroupInfo> m_groups;
    mutable QDateTime m_now; // taken when the cache starts filling, i.e. once per sort pass
};

Q_DECLARE_METATYPE(Tag)

class TagsColumns : public KExtraColumnsProxyModel
{
    Q_OBJECT
public:
    static constexpr int TagsColumn = KDirModel::ColumnCount; // 7
    static constexpr int TagsRole = Qt::UserRole + 101; // QList<Tag>
    explicit TagsColumns(FolderModel *source, QObject *parent = nullptr); // chain: source -> TagGroupProxy -> this
    FolderModel *folderModel() const;
    TagGroupProxy *groupProxy() const; // also holds the tag cache
    KFileItem itemFor(const QModelIndex &) const; // maps through all three proxies
    QModelIndex indexFor(const QUrl &) const; // column 0 of the url's first row
    QVariant extraColumnData(const QModelIndex &parent, int row, int extraColumn, int role) const override;
    QVariant headerData(int section, Qt::Orientation, int role = Qt::DisplayRole) const override; // columnTitle()
    void invalidateTags(const QList<QUrl> &); // drops the cached tags, repaints column 0 and the Tags column of those rows (regroups in tag mode)
};

class PreviewLoader : public QObject
{
    Q_OBJECT
public:
    PreviewLoader(FolderModel *model, QObject *parent = nullptr);
    void setView(QAbstractItemView *); // nullptr = no scheduling
    void setEnabled(bool);
    void setSize(int px); // size change clears the cache
    QPixmap previewFor(const KFileItem &) const; // invalid pixmap when not loaded
    void requestVisible(); // debounced 50 ms

Q_SIGNALS:
    void previewReady(const QUrl &);

private:
    void start();
    void startNextBatch();
    void stop();
    void clearCache();

    QPointer<QAbstractItemView> m_view;
    bool m_enabled = false;
    int m_size = 48;
    QHash<std::pair<QUrl, qint64>, QPixmap> m_cache; // url + mtime; null pixmap = failed, not retried
    QTimer m_debounce;
    QPointer<KIO::PreviewJob> m_job;
    KFileItemList m_queue;
    int m_nextBatch = 0;
    const QStringList m_plugins;
};

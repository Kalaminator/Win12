#pragma once
#include <KFileItem>
#include <QAbstractTableModel>
#include <QHash>
#include <QUrl>
#include <memory>
#include <vector>

// A flat list of found files that FolderModel sorts in place of its KDirModel: KDirModel's columns, headers and roles
// (DisplayRole per column, DecorationRole on column 0, FileItemRole). Each row's internalPointer is its own heap
// KFileItem, stable until clear(), so FolderModel's group cache can key on it.
// ponytail: results are not watched for changes; a rename inside results refreshes nothing until the search reruns
class ResultsModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    using QAbstractTableModel::QAbstractTableModel;
    void clear();
    void append(const KFileItemList &); // one rowsInserted per call
    KFileItem itemAt(int row) const;
    int count() const;
    QModelIndex indexForUrl(const QUrl &) const; // column 0; invalid when not listed

    QModelIndex index(int row, int column, const QModelIndex &parent = {}) const override;
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation, int role = Qt::DisplayRole) const override;

private:
    std::vector<std::unique_ptr<KFileItem>> m_items;
    QHash<QUrl, int> m_rows;
};

#include "resultsmodel.h"
#include <KDirModel>
#include <KIO/Global>
#include <KLocalizedString>
#include <QIcon>

void ResultsModel::clear()
{
    beginResetModel();
    m_items.clear();
    m_rows.clear();
    endResetModel();
}

void ResultsModel::append(const KFileItemList &items)
{
    if (items.isEmpty())
        return;
    const int first = count();
    beginInsertRows({}, first, first + int(items.size()) - 1);
    for (const KFileItem &item : items) {
        m_rows.insert(item.url(), count());
        m_items.push_back(std::make_unique<KFileItem>(item));
    }
    endInsertRows();
}

KFileItem ResultsModel::itemAt(int row) const
{
    return row >= 0 && row < count() ? *m_items[row] : KFileItem();
}

int ResultsModel::count() const
{
    return int(m_items.size());
}

QModelIndex ResultsModel::indexForUrl(const QUrl &url) const
{
    const auto it = m_rows.constFind(url);
    return it == m_rows.cend() ? QModelIndex() : index(*it, 0);
}

QModelIndex ResultsModel::index(int row, int column, const QModelIndex &parent) const
{
    if (parent.isValid() || row < 0 || row >= count() || column < 0 || column >= KDirModel::ColumnCount)
        return {};
    return createIndex(row, column, m_items[row].get());
}

int ResultsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : count();
}

int ResultsModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(KDirModel::ColumnCount);
}

QVariant ResultsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return {};
    const KFileItem &item = *static_cast<const KFileItem *>(index.internalPointer());
    switch (role) {
    case KDirModel::FileItemRole:
        return QVariant::fromValue(item);
    case Qt::DecorationRole:
        if (index.column() == KDirModel::Name)
            return QIcon::fromTheme(item.iconName());
        break;
    case Qt::DisplayRole:
        switch (index.column()) {
        case KDirModel::Name:
            return item.text();
        case KDirModel::Size:
            return item.isDir() ? QString() : KIO::convertSize(item.size());
        case KDirModel::ModifiedTime:
            return item.timeString(KFileItem::ModificationTime);
        case KDirModel::Permissions:
            return item.permissionsString();
        case KDirModel::Owner:
            return item.user();
        case KDirModel::Group:
            return item.group();
        case KDirModel::Type:
            return item.mimeComment();
        }
        break;
    }
    return {};
}

QVariant ResultsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QAbstractTableModel::headerData(section, orientation, role);
    // KDirModel's strings, from its catalogue, so both sources translate alike.
    switch (section) {
    case KDirModel::Name:
        return i18ndc("kio6", "@title:column", "Name");
    case KDirModel::Size:
        return i18ndc("kio6", "@title:column", "Size");
    case KDirModel::ModifiedTime:
        return i18ndc("kio6", "@title:column", "Date");
    case KDirModel::Permissions:
        return i18ndc("kio6", "@title:column", "Permissions");
    case KDirModel::Owner:
        return i18ndc("kio6", "@title:column", "Owner");
    case KDirModel::Group:
        return i18ndc("kio6", "@title:column", "Group");
    case KDirModel::Type:
        return i18ndc("kio6", "@title:column", "Type");
    }
    return {};
}

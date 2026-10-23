#include "folderview.h"
#include <KDirLister>
#include <KIO/Global>
#include <KLocalizedString>
#include <QApplication>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QScopedValueRollback>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QTreeView>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <functional>

namespace {
constexpr int CheckSize = 16;
constexpr int MaxHistory = 50;

bool isIconMode(ViewMode m)
{
    return m == ViewMode::MediumIcons || m == ViewMode::LargeIcons || m == ViewMode::ExtraLargeIcons;
}

KFileItem itemOf(const QModelIndex &index)
{
    return static_cast<const TagsColumns *>(index.model())->itemFor(index);
}

// Drops on any folder, not only where the model says it accepts them; the drop itself goes out through onDrop.
template<class Base>
class FileItemView : public Base
{
public:
    std::function<void(QDropEvent *, const QModelIndex &)> onDrop;

protected:
    void dragEnterEvent(QDragEnterEvent *e) override
    {
        Base::dragEnterEvent(e);
        if (e->mimeData()->hasUrls()) {
            e->acceptProposedAction();
            this->setState(QAbstractItemView::DraggingState);
        }
    }
    void dragMoveEvent(QDragMoveEvent *e) override
    {
        Base::dragMoveEvent(e);
        if (e->mimeData()->hasUrls())
            e->acceptProposedAction();
    }
    void dropEvent(QDropEvent *e) override
    {
        this->stopAutoScroll();
        this->setState(QAbstractItemView::NoState);
        this->viewport()->update();
        onDrop(e, this->indexAt(e->position().toPoint()));
    }
};

class FolderDelegate : public QStyledItemDelegate
{
public:
    FolderDelegate(const ViewSettings *settings, FolderView *view)
        : QStyledItemDelegate(view)
        , m_settings(settings)
        , m_view(view)
    {
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        const ViewMode mode = m_settings->mode;
        const int px = iconSizeFor(mode);
        const int lh = option.fontMetrics.height();
        QSize s;
        if (isIconMode(mode)) {
            s = QSize(px + 40, px + 2 * lh + 10); // ponytail: fixed cell, two text lines; uniform sizes keep 20k items cheap
        } else if (mode == ViewMode::Tiles) {
            s = QSize(260, qMax(px, 3 * lh) + 8);
        } else if (mode == ViewMode::Content) {
            const auto *v = qobject_cast<const QAbstractItemView *>(option.widget);
            s = QSize(v ? v->viewport()->width() - 1 : 600, qMax(px, 2 * lh) + 12);
        } else if (mode == ViewMode::SmallIcons) {
            s = QSize(220, qMax(px, lh) + 6);
        } else {
            s = QStyledItemDelegate::sizeHint(option, index);
            if (hasGutter(index))
                s.rwidth() += CheckSize + 4;
        }
        s.rheight() += stripHeight(option, index);
        return s;
    }

    void paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        const QWidget *w = opt.widget;
        QStyle *style = w ? w->style() : QApplication::style();
        if (const int strip = stripHeight(opt, index)) {
            paintGroupHeader(p, opt, index, strip);
            opt.rect.setTop(opt.rect.top() + strip);
        }
        const QRect check = checkRect(opt.rect, index);
        if (hasGutter(index))
            opt.rect.setLeft(opt.rect.left() + CheckSize + 4);
        if (m_settings->mode == ViewMode::Tiles || m_settings->mode == ViewMode::Content)
            paintRich(p, opt, style, index);
        else
            style->drawControl(QStyle::CE_ItemViewItem, &opt, p, w);
        if (m_settings->checkBoxes && index.column() == 0 && (opt.state & (QStyle::State_MouseOver | QStyle::State_Selected))) {
            QStyleOptionButton cb;
            cb.rect = check;
            cb.state = QStyle::State_Enabled | ((opt.state & QStyle::State_Selected) ? QStyle::State_On : QStyle::State_Off);
            style->drawPrimitive(QStyle::PE_IndicatorCheckBox, &cb, p, w);
        }
    }

    // A click on the check box toggles that row and nothing else (no current-index move, no clearing).
    bool editorEvent(QEvent *e, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index) override
    {
        const auto t = e->type();
        if (m_settings->checkBoxes && index.column() == 0
            && (t == QEvent::MouseButtonPress || t == QEvent::MouseButtonRelease || t == QEvent::MouseButtonDblClick)) {
            const auto *me = static_cast<QMouseEvent *>(e);
            QRect r = option.rect;
            r.setTop(r.top() + stripHeight(option, index));
            const auto *view = qobject_cast<const QAbstractItemView *>(option.widget);
            if (view && me->button() == Qt::LeftButton && checkRect(r, index).contains(me->position().toPoint())) {
                if (t == QEvent::MouseButtonPress)
                    view->selectionModel()->select(index, QItemSelectionModel::Toggle | QItemSelectionModel::Rows);
                return true;
            }
        }
        return QStyledItemDelegate::editorEvent(e, model, option, index);
    }

    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        auto *le = qobject_cast<QLineEdit *>(editor);
        if (!le)
            return QStyledItemDelegate::setEditorData(editor, index);
        const KFileItem item = itemOf(index);
        le->setText(item.name());
        // Windows selects the name without its extension; the view's own selectAll() runs after us.
        const int dot = item.isDir() ? -1 : item.name().lastIndexOf(QLatin1Char('.'));
        const int len = dot > 0 ? dot : int(item.name().size());
        QTimer::singleShot(0, le, [le, len] { le->setSelection(0, len); });
    }

    // The model is never asked to rename: the window runs the KIO job.
    void setModelData(QWidget *editor, QAbstractItemModel *, const QModelIndex &index) const override
    {
        const auto *le = qobject_cast<QLineEdit *>(editor);
        const KFileItem item = itemOf(index);
        const QString name = le ? le->text().trimmed() : QString();
        if (!name.isEmpty() && name != item.name())
            Q_EMIT m_view->renameRequested(item, name);
    }

protected:
    void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override
    {
        QStyledItemDelegate::initStyleOption(option, index);
        if (index.column() == 0)
            option->text = FolderView::displayName(itemOf(index), m_settings->showExtensions);
    }

private:
    int stripHeight(const QStyleOptionViewItem &o, const QModelIndex &index) const
    {
        return m_settings->groupRole != GroupRole::None && index.siblingAtColumn(0).data(FolderModel::GroupFirstRole).toBool()
            ? o.fontMetrics.height() * 2
            : 0;
    }
    // Row-like modes put the check box in a gutter left of the icon; icon and tile modes overlay it top-left.
    bool hasGutter(const QModelIndex &index) const
    {
        const ViewMode m = m_settings->mode;
        return m_settings->checkBoxes && index.column() == 0 && !isIconMode(m) && m != ViewMode::Tiles;
    }
    QRect checkRect(const QRect &r, const QModelIndex &index) const
    {
        if (hasGutter(index))
            return QRect(r.left() + 2, r.top() + (r.height() - CheckSize) / 2, CheckSize, CheckSize);
        return QRect(r.left() + 2, r.top() + 2, CheckSize, CheckSize);
    }

    void paintGroupHeader(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &index, int strip) const
    {
        const QRect r(opt.rect.left(), opt.rect.top(), opt.rect.width(), strip);
        QColor c = opt.palette.color(QPalette::Highlight);
        p->save();
        int lineStart = r.left();
        if (index.column() == 0) {
            const QString label = index.data(FolderModel::GroupLabelRole).toString();
            p->setPen(c);
            p->drawText(r.adjusted(6, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, label);
            lineStart += 6 + opt.fontMetrics.horizontalAdvance(label) + 8;
        }
        c.setAlphaF(0.35);
        p->setPen(c);
        if (lineStart < r.right())
            p->drawLine(lineStart, r.center().y(), r.right(), r.center().y());
        p->restore();
    }

    // Tiles: icon, then name / type / size stacked. Content: icon, name over type, then modified and size.
    void paintRich(QPainter *p, const QStyleOptionViewItem &opt, QStyle *style, const QModelIndex &index) const
    {
        style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, p, opt.widget);
        const int px = iconSizeFor(m_settings->mode);
        const QRect r = opt.rect.adjusted(4, 2, -4, -2);
        const QRect iconRect(r.left(), r.top() + (r.height() - px) / 2, px, px);
        const bool selected = opt.state & QStyle::State_Selected;
        opt.icon.paint(p, iconRect, Qt::AlignCenter, selected ? QIcon::Selected : QIcon::Normal);
        const KFileItem item = itemOf(index);
        const QString type = SortGroup::groupInfo(item, GroupRole::Type, {}).label; // cheap: never mimeComment()
        const QString size = item.isDir() ? QString() : KIO::convertSize(item.size());
        const QColor main = opt.palette.color(selected ? QPalette::HighlightedText : QPalette::Text);
        const QColor dim = selected ? main : opt.palette.color(QPalette::PlaceholderText);
        const QFontMetrics &fm = opt.fontMetrics;
        const int lh = fm.height();
        const QRect text(iconRect.right() + 8, r.top(), r.right() - iconRect.right() - 8, r.height());
        const auto line = [&](const QRect &box, const QString &s, const QColor &c, Qt::Alignment v) {
            p->setPen(c);
            p->drawText(box, Qt::AlignLeft | v, fm.elidedText(s, Qt::ElideRight, box.width()));
        };
        p->save();
        if (m_settings->mode == ViewMode::Tiles) {
            const QStringList lines = size.isEmpty() ? QStringList{opt.text, type} : QStringList{opt.text, type, size};
            int y = text.top() + (text.height() - int(lines.size()) * lh) / 2;
            for (int i = 0; i < lines.size(); ++i, y += lh)
                line(QRect(text.left(), y, text.width(), lh), lines.at(i), i ? dim : main, Qt::AlignVCenter);
        } else {
            const int third = text.width() / 3;
            const QRect name(text.left(), text.top(), third - 8, text.height() / 2);
            line(name, opt.text, main, Qt::AlignBottom);
            line(name.translated(0, name.height()), type, dim, Qt::AlignTop);
            const QString modified = QLocale().toString(item.time(KFileItem::ModificationTime), QLocale::ShortFormat);
            const QRect col(text.left() + third, text.top(), third - 8, text.height());
            line(col, i18n("Date modified: %1", modified), dim, Qt::AlignVCenter);
            if (!size.isEmpty())
                line(col.translated(third, 0), i18n("Size: %1", size), dim, Qt::AlignVCenter);
        }
        p->restore();
    }

    const ViewSettings *m_settings;
    FolderView *m_view;
};
} // namespace

FolderView::FolderView(ViewSettingsStore *store, QWidget *parent)
    : QWidget(parent)
    , m_store(store)
    , m_folderModel(new FolderModel(this))
    , m_model(new TagsColumns(m_folderModel, this))
    , m_previews(new PreviewLoader(m_folderModel, this))
    , m_stack(new QStackedWidget(this))
{
    m_folderModel->setPreviewLoader(m_previews);
    m_folderModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    auto *list = new FileItemView<QListView>;
    auto *tree = new FileItemView<QTreeView>;
    m_list = list;
    m_tree = tree;
    const auto onDrop = [this](QDropEvent *e, const QModelIndex &index) {
        const KFileItem target = index.isValid() ? m_model->itemFor(index) : KFileItem();
        const QUrl dest = !target.isNull() && target.isDir() ? target.url() : m_url;
        if ((e->source() == m_list || e->source() == m_tree) && dest == m_url)
            return e->ignore(); // dropped back onto its own folder
        e->acceptProposedAction();
        Q_EMIT dropRequested(e, dest);
    };
    list->onDrop = onDrop;
    tree->onDrop = onDrop;

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins({});
    layout->addWidget(m_stack);
    m_stack->addWidget(m_list);
    m_stack->addWidget(m_tree);

    for (QAbstractItemView *v : {static_cast<QAbstractItemView *>(m_list), static_cast<QAbstractItemView *>(m_tree)}) {
        v->setModel(m_model);
        v->setItemDelegate(new FolderDelegate(&m_settings, this)); // one each: a delegate's commitData reaches every view it serves
        v->setSelectionMode(QAbstractItemView::ExtendedSelection);
        v->setSelectionBehavior(QAbstractItemView::SelectRows);
        v->setEditTriggers(QAbstractItemView::EditKeyPressed);
        v->setDragEnabled(true);
        v->setDragDropMode(QAbstractItemView::DragDrop);
        v->setDefaultDropAction(Qt::MoveAction);
        v->setContextMenuPolicy(Qt::CustomContextMenu);
        v->viewport()->setAttribute(Qt::WA_Hover);
        v->viewport()->installEventFilter(this);
        connect(v, &QWidget::customContextMenuRequested, this, [this, v](const QPoint &pos) {
            showContextMenu(v, pos);
        });
        connect(v, &QAbstractItemView::activated, this, &FolderView::activate);
    }
    m_selection = m_list->selectionModel();
    m_tree->setSelectionModel(m_selection);
    m_list->setSelectionRectVisible(true);
    m_list->setLayoutMode(QListView::Batched);
    m_tree->setRootIsDecorated(false);
    m_tree->setItemsExpandable(false);
    m_tree->setAllColumnsShowFocus(true);
    m_tree->setSortingEnabled(true);

    QHeaderView *header = m_tree->header();
    header->setSectionsMovable(true);
    header->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(header, &QWidget::customContextMenuRequested, this, &FolderView::showHeaderMenu);
    connect(header, &QHeaderView::sortIndicatorChanged, this, [this](int column, Qt::SortOrder order) {
        if (m_applying)
            return;
        ViewSettings s = m_settings;
        for (SortRole r : {SortRole::Name, SortRole::Modified, SortRole::Type, SortRole::Size})
            if (SortGroup::sortColumnFor(r) == column) {
                s.sortRole = r;
                s.sortOrder = order;
            }
        applySettings(s); // ponytail: columns without a SortRole (permissions, owner, group, tags) snap back
    });
    connect(header, &QHeaderView::sectionMoved, this, [this] {
        if (m_applying)
            return;
        ViewSettings s = m_settings;
        s.columns.clear();
        QHeaderView *h = m_tree->header();
        for (int v = 0; v < h->count(); ++v)
            if (!h->isSectionHidden(h->logicalIndex(v)))
                for (const QString &id : knownColumnIds())
                    if (modelColumnFor(id) == h->logicalIndex(v))
                        s.columns << id;
        applySettings(s);
    });

    connect(m_selection, &QItemSelectionModel::selectionChanged, this, [this] {
        Q_EMIT selectionChanged();
        emitCounts();
    });
    connect(m_model, &QAbstractItemModel::rowsInserted, this, &FolderView::emitCounts);
    connect(m_model, &QAbstractItemModel::rowsInserted, m_previews, &PreviewLoader::requestVisible);
    connect(m_model, &QAbstractItemModel::rowsRemoved, this, &FolderView::emitCounts);
    connect(m_model, &QAbstractItemModel::modelReset, this, [this] {
        Q_EMIT selectionChanged(); // a reset drops the selection without selectionChanged
        emitCounts();
    });
    connect(m_folderModel->lister(), &KCoreDirLister::listingDirCompleted, m_previews, &PreviewLoader::requestVisible);
    apply(m_settings);
}

void FolderView::openUrl(const QUrl &url)
{
    const QUrl u = url.adjusted(QUrl::StripTrailingSlash);
    if (u == m_url)
        return;
    m_history.resize(m_historyIndex + 1);
    m_history.append(u);
    if (m_history.size() > MaxHistory)
        m_history.removeFirst();
    m_historyIndex = int(m_history.size()) - 1;
    navigate(u);
}

void FolderView::navigate(const QUrl &url)
{
    m_url = url;
    apply(m_store->load(url)); // before listing, so hidden files and sorting apply to the new rows
    m_folderModel->openUrl(url);
    Q_EMIT urlChanged(url);
}

QUrl FolderView::url() const
{
    return m_url;
}

bool FolderView::goBack()
{
    if (!canGoBack())
        return false;
    navigate(m_history.at(--m_historyIndex));
    return true;
}

bool FolderView::goForward()
{
    if (!canGoForward())
        return false;
    navigate(m_history.at(++m_historyIndex));
    return true;
}

bool FolderView::goUp()
{
    const QUrl up = KIO::upUrl(m_url);
    if (up.isEmpty() || up.adjusted(QUrl::StripTrailingSlash) == m_url)
        return false;
    openUrl(up);
    return true;
}

bool FolderView::canGoBack() const
{
    return m_historyIndex > 0;
}

bool FolderView::canGoForward() const
{
    return m_historyIndex + 1 < m_history.size();
}

void FolderView::refresh()
{
    m_folderModel->lister()->updateDirectory(m_url);
}

void FolderView::applySettings(const ViewSettings &s)
{
    apply(s);
    if (m_url.isValid())
        m_store->save(m_url, s);
}

void FolderView::apply(const ViewSettings &s)
{
    const QScopedValueRollback guard(m_applying, true);
    const bool relayout = s.mode != m_settings.mode || s.groupRole != m_settings.groupRole;
    m_settings = s;
    KDirLister *lister = m_folderModel->lister();
    if (lister->showHiddenFiles() != s.hiddenFiles) {
        lister->setShowHiddenFiles(s.hiddenFiles);
        lister->emitChanges();
    }
    if (m_folderModel->groupRole() != s.groupRole)
        m_folderModel->setGroupRole(s.groupRole);
    const int sortColumn = SortGroup::sortColumnFor(s.sortRole);
    if (m_folderModel->sortColumn() != sortColumn || m_folderModel->sortOrder() != s.sortOrder)
        m_folderModel->setSorting(s.sortRole, s.sortOrder);
    m_tree->header()->setSortIndicator(sortColumn, s.sortOrder);

    const bool grouped = s.groupRole != GroupRole::None;
    const int px = iconSizeFor(s.mode);
    if (s.mode == ViewMode::Details) {
        m_tree->setIconSize(QSize(px, px));
        m_tree->setUniformRowHeights(!grouped);
        applyColumns();
        m_stack->setCurrentWidget(m_tree);
    } else {
        // setViewMode resets flow, wrapping, movement and resize mode, so it goes first.
        m_list->setViewMode(isIconMode(s.mode) ? QListView::IconMode : QListView::ListMode);
        m_list->setMovement(QListView::Static);
        m_list->setResizeMode(QListView::Adjust);
        m_list->setFlow(s.mode == ViewMode::List || s.mode == ViewMode::Content ? QListView::TopToBottom : QListView::LeftToRight);
        m_list->setWrapping(s.mode != ViewMode::Content);
        m_list->setWordWrap(isIconMode(s.mode));
        // ponytail: groups only add a header strip to their first item; a grouped icon grid does not break rows.
        m_list->setUniformItemSizes(!grouped && s.mode != ViewMode::List);
        m_list->setHorizontalScrollBarPolicy(s.mode == ViewMode::Content ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
        m_list->setIconSize(QSize(px, px));
        if (relayout)
            m_list->doItemsLayout();
        m_stack->setCurrentWidget(m_list);
    }
    m_previews->setView(currentView());
    m_previews->setSize(px);
    m_previews->setEnabled(s.previews);
    m_previews->requestVisible();
    if (m_selection->currentIndex().isValid())
        currentView()->scrollTo(m_selection->currentIndex());
    currentView()->viewport()->update();
}

void FolderView::applyColumns()
{
    QHeaderView *h = m_tree->header();
    for (const QString &id : knownColumnIds())
        h->setSectionHidden(modelColumnFor(id), !m_settings.columns.contains(id));
    int visual = 0;
    for (const QString &id : std::as_const(m_settings.columns)) {
        const int column = modelColumnFor(id);
        if (column >= 0)
            h->moveSection(h->visualIndex(column), visual++);
    }
}

ViewSettings FolderView::settings() const
{
    return m_settings;
}

void FolderView::setViewMode(ViewMode mode)
{
    ViewSettings s = m_settings;
    s.mode = mode;
    applySettings(s);
}

void FolderView::zoom(int steps)
{
    setViewMode(zoomStep(m_settings.mode, steps));
}

KFileItemList FolderView::selectedItems() const
{
    KFileItemList items;
    const QModelIndexList rows = m_selection->selectedRows();
    for (const QModelIndex &index : rows)
        items << m_model->itemFor(index);
    return items;
}

KFileItem FolderView::currentItem() const
{
    const QModelIndex index = m_selection->currentIndex();
    return index.isValid() ? m_model->itemFor(index) : KFileItem();
}

int FolderView::itemCount() const
{
    return m_model->rowCount();
}

int FolderView::selectedCount() const
{
    return int(m_selection->selectedRows().size());
}

void FolderView::selectAll()
{
    const int rows = m_model->rowCount();
    if (rows > 0)
        m_selection->select(QItemSelection(m_model->index(0, 0), m_model->index(rows - 1, m_model->columnCount() - 1)),
                            QItemSelectionModel::ClearAndSelect);
}

void FolderView::selectNone()
{
    m_selection->clearSelection();
}

void FolderView::invertSelection()
{
    const int rows = m_model->rowCount();
    if (rows > 0)
        m_selection->select(QItemSelection(m_model->index(0, 0), m_model->index(rows - 1, 0)),
                            QItemSelectionModel::Toggle | QItemSelectionModel::Rows);
}

void FolderView::selectUrls(const QList<QUrl> &urls)
{
    QItemSelection selection;
    QModelIndex first;
    for (const QUrl &url : urls) {
        const QModelIndex index = m_model->mapFromSource(m_folderModel->indexFor(url));
        if (!index.isValid())
            continue;
        selection.select(index, index);
        if (!first.isValid())
            first = index;
    }
    m_selection->select(selection, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    if (first.isValid()) {
        m_selection->setCurrentIndex(first, QItemSelectionModel::NoUpdate);
        currentView()->scrollTo(first);
    }
}

void FolderView::renameCurrent()
{
    QModelIndex index = m_selection->currentIndex();
    if (!index.isValid() || !m_selection->isSelected(index)) {
        const QModelIndexList rows = m_selection->selectedRows();
        if (rows.isEmpty())
            return;
        index = rows.first();
    }
    index = index.siblingAtColumn(0);
    currentView()->scrollTo(index);
    currentView()->edit(index);
}

void FolderView::setNameFilter(const QString &text)
{
    m_folderModel->setFilterFixedString(text);
}

QAbstractItemView *FolderView::currentView() const
{
    return static_cast<QAbstractItemView *>(m_stack->currentWidget());
}

TagsColumns *FolderView::model() const
{
    return m_model;
}

QString FolderView::displayName(const KFileItem &item, bool showExtensions)
{
    const QString name = item.text();
    if (showExtensions || item.isDir())
        return name;
    const int dot = name.lastIndexOf(QLatin1Char('.'));
    return dot > 0 ? name.left(dot) : name;
}

bool FolderView::eventFilter(QObject *watched, QEvent *e)
{
    QAbstractItemView *view = watched == m_list->viewport() ? static_cast<QAbstractItemView *>(m_list)
        : watched == m_tree->viewport()                     ? static_cast<QAbstractItemView *>(m_tree)
                                                            : nullptr;
    if (!view)
        return QWidget::eventFilter(watched, e);
    switch (e->type()) {
    case QEvent::Wheel: {
        const auto *we = static_cast<QWheelEvent *>(e);
        if (we->modifiers() & Qt::ControlModifier) {
            if (const int dy = we->angleDelta().y())
                zoom(dy > 0 ? 1 : -1);
            return true;
        }
        break;
    }
    case QEvent::MouseButtonRelease: {
        const auto *me = static_cast<QMouseEvent *>(e);
        if (me->button() == Qt::MiddleButton) {
            const QModelIndex index = view->indexAt(me->position().toPoint());
            const KFileItem item = index.isValid() ? m_model->itemFor(index) : KFileItem();
            if (!item.isNull() && item.isDir())
                Q_EMIT folderMiddleClicked(item.url());
        }
        break;
    }
    case QEvent::Resize:
        m_previews->requestVisible();
        break;
    default:
        break;
    }
    return QWidget::eventFilter(watched, e);
}

void FolderView::emitCounts()
{
    Q_EMIT countsChanged(itemCount(), selectedCount());
}

void FolderView::activate(const QModelIndex &index)
{
    const KFileItem item = m_model->itemFor(index);
    if (item.isNull())
        return;
    if (item.isDir())
        openUrl(item.url());
    else
        Q_EMIT fileActivated(item);
}

void FolderView::showContextMenu(QAbstractItemView *view, const QPoint &pos)
{
    const QModelIndex index = view->indexAt(pos);
    if (!index.isValid()) {
        m_selection->clearSelection();
    } else if (!m_selection->isSelected(index)) {
        m_selection->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        m_selection->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
    }
    Q_EMIT contextMenuRequested(view->viewport()->mapToGlobal(pos), index.isValid() ? selectedItems() : KFileItemList());
}

// "Add columns": every known column with a check mark; Name stays.
void FolderView::showHeaderMenu(const QPoint &pos)
{
    QMenu menu;
    for (const QString &id : knownColumnIds()) {
        QAction *a = menu.addAction(columnTitle(id));
        a->setCheckable(true);
        a->setChecked(m_settings.columns.contains(id));
        a->setEnabled(id != QLatin1String("name"));
        a->setData(id);
    }
    const QAction *chosen = menu.exec(m_tree->header()->mapToGlobal(pos));
    if (!chosen)
        return;
    ViewSettings s = m_settings;
    const QString id = chosen->data().toString();
    if (chosen->isChecked())
        s.columns << id;
    else
        s.columns.removeAll(id);
    applySettings(s);
}

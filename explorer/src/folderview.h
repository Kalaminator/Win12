#pragma once
#include "foldermodel.h"
#include "viewsettings.h"
#include <KFileItem>
#include <QWidget>

class QAbstractItemView;
class QDropEvent;
class QItemSelectionModel;
class QListView;
class QStackedWidget;
class QTreeView;

// The file pane: a QListView (icon/list/tiles/content) and a QTreeView (details) over one model and selection.
class FolderView : public QWidget
{
    Q_OBJECT
public:
    explicit FolderView(ViewSettingsStore *store, QWidget *parent = nullptr);
    void openUrl(const QUrl &); // pushes history, loads store->load(url), applies it
    QUrl url() const;
    bool goBack();
    bool goForward();
    bool goUp();
    bool canGoBack() const;
    bool canGoForward() const;
    void refresh();
    void applySettings(const ViewSettings &); // also store->save(url(), s)
    ViewSettings settings() const;
    void setViewMode(ViewMode);
    void zoom(int steps); // applySettings with zoomStep
    KFileItemList selectedItems() const;
    KFileItem currentItem() const;
    int itemCount() const;
    int selectedCount() const;
    void selectAll();
    void selectNone();
    void invertSelection();
    void selectUrls(const QList<QUrl> &);
    void renameCurrent(); // opens the inline editor (F2)
    void setNameFilter(const QString &); // case-insensitive substring
    QAbstractItemView *currentView() const;
    TagsColumns *model() const;
    // The delegate's text: the suffix of non-folders hidden when !showExtensions; dot-files keep their name.
    static QString displayName(const KFileItem &, bool showExtensions);

Q_SIGNALS:
    void urlChanged(const QUrl &);
    void selectionChanged();
    void countsChanged(int items, int selected);
    void fileActivated(const KFileItem &); // double-click/Enter on a non-folder
    void folderMiddleClicked(const QUrl &); // tab request
    void contextMenuRequested(const QPoint &globalPos, const KFileItemList &items); // empty = folder background
    void renameRequested(const KFileItem &, const QString &newName);
    void dropRequested(QDropEvent *, const QUrl &destination); // folder under the cursor, else url()

protected:
    bool eventFilter(QObject *, QEvent *) override;

private:
    void navigate(const QUrl &);
    void apply(const ViewSettings &); // applySettings without saving
    void applyColumns();
    void emitCounts();
    void activate(const QModelIndex &);
    void showContextMenu(QAbstractItemView *, const QPoint &);
    void showHeaderMenu(const QPoint &);

    ViewSettingsStore *m_store;
    FolderModel *m_folderModel;
    TagsColumns *m_model;
    PreviewLoader *m_previews;
    QItemSelectionModel *m_selection;
    QStackedWidget *m_stack;
    QListView *m_list;
    QTreeView *m_tree;
    ViewSettings m_settings;
    QUrl m_url;
    QList<QUrl> m_history; // NavHistory: at most 50 entries
    int m_historyIndex = -1;
    bool m_applying = false;
};

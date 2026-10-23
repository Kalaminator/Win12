#pragma once
#include "viewsettings.h"
#include <KFileItem>
#include <QHash>
#include <QObject>
#include <QPointer>

class FolderView;
class KNewFileMenu;
class QAction;
class QMenu;
class QWidget;
namespace Purpose { class Menu; }

// One QAction per ribbon id (text/icon from Ribbon::spec()); the work runs as KIO jobs recorded in
// KIO::FileUndoManager, always against the active view's folder or selection (disabled without a view).
// Extra ids beyond the spec: "home_delete_trash" (Delete) and "home_delete_permanent" (Shift+Delete),
// the entries of home_delete's menu; add them to the window so their shortcuts work.
class ExplorerActions : public QObject
{
    Q_OBJECT
public:
    explicit ExplorerActions(QWidget *windowParent);
    QAction *action(const QString &id) const; // nullptr for unknown ids
    void setActiveView(FolderView *); // the view the actions operate on
    QMenu *contextMenu(const KFileItemList &items, const QUrl &folder); // caller owns
    QMenu *newItemMenu() const; // KNewFileMenu's menu; its folder follows setActiveView
    QMenu *historyMenu() const; // filled by ExplorerWindow
    // Synchronous KIO::moveAs; false and no change when the target exists or newName is empty/contains '/'.
    static bool rename(const KFileItem &item, const QString &newName, QWidget *parent);

Q_SIGNALS:
    void newWindowRequested();
    void closeRequested();
    void optionsRequested(); // after win12-settings was launched
    void helpRequested(); // after the help page was opened
    void layoutRequested(ViewMode);
    void sortRequested(SortRole, Qt::SortOrder);
    void groupRequested(GroupRole);
    void columnsToggled(const QString &id, bool on);
    void paneToggled(const QString &pane, bool on); // "nav" | "preview" | "details"
    void splitToggled(bool);
    void showToggled(const QString &what, bool on); // "checkboxes" | "extensions" | "hidden"
    void fillWin12Menu(QMenu *, const KFileItemList &); // seam for phases 2-4

private:
    QAction *add(const QString &id, const QString &text, const QString &icon);
    void connectActions();
    void buildViewMenus();
    void updateEnabled();
    void syncViewState(); // check marks of layout/sort/group/columns/show from the view's settings
    KFileItemList selection() const;
    QUrl folder() const;

    // The work, on explicit items/folders so context menus act on what they were opened for.
    void copyToClipboard(const KFileItemList &, bool cut);
    void paste(const QUrl &folder);
    void pasteShortcut(const QUrl &folder);
    void transferTo(const KFileItemList &, bool move);
    void trash(const KFileItemList &);
    void deletePermanently(const KFileItemList &);
    void open(const KFileItemList &);
    void properties(const KFileItemList &, const QUrl &folder);
    void launch(const QString &exe, const QStringList &args);
    void openTerminal(const QUrl &folder);

    QWidget *m_window;
    QHash<QString, QAction *> m_actions;
    QPointer<FolderView> m_view;
    KNewFileMenu *m_newFileMenu;
    Purpose::Menu *m_shareMenu;
};

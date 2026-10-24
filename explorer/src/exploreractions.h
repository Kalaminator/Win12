#pragma once
#include "viewsettings.h"
#include <KFileItem>
#include <QHash>
#include <QObject>
#include "selectbyruledialog.h"
#include <QPointer>
#include "renamemanydialog.h"
#include "renameplan.h"
#include <functional>
#include "newtagdialog.h"
#include "tagregistry.h"
#include "tagstore.h"
#include "rulelibrary.h"
#include "scriptdialog.h"


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
    // Synchronous KIO::moveAs. Empty on success (or same name), else why nothing changed: invalid name
    // (empty, '/', "." or ".."), target exists, or the job's own error.
    // Public workers: act on explicit items (context menus, rule matches).
    // Asks for the folder; a valid keepStructureFrom keeps each item's subfolder relative to it.
    void transferTo(const KFileItemList &, bool move, const QUrl &keepStructureFrom = QUrl());
    void trash(const KFileItemList &);
    void deletePermanently(const KFileItemList &);
    void zip(const KFileItemList &); // ark --add --dialog --changetofirstpath <local paths>
    void copyPaths(const KFileItemList &); // one path per line, local path or display url
    static KFileItemList withoutNested(const KFileItemList &); // drops items inside another listed folder
    void selectByRule(SelectByRuleDialog::Action defaultAction);
    void selectByFormula(const QString &formula = QString()); // Select by rule in Formula mode; non-empty: setFormula + find()
    void renameMany(const KFileItemList &items, const QString &formula); // Formula mode preloaded
    void openScript(const QString &text = QString()); // ScriptDialog on folder(); non-empty: setScript + buildPlan()
    QMenu *myRulesMenu(const KFileItemList &items, QWidget *parent = nullptr); // caller owns
    QMenu *myScriptsMenu(); // caller owns
    RuleLibrary &library() { return m_library; }
    void setExecScriptDialog(std::function<void(ScriptDialog &)> exec) { m_execScriptDialog = std::move(exec); } // test seam
    void setExecSelectDialog(std::function<bool(SelectByRuleDialog &)> exec) { m_execSelectDialog = std::move(exec); } // test seam
    void setPickFolder(std::function<QUrl(const QUrl &start)> pick) { m_pickFolder = std::move(pick); } // test seam
    // Opens the Rename many dialog for items (view order) and runs the Status::Rename rows of its plan.
    void renameMany(const KFileItemList &items);
    void undoRenameBatch(); // reverses the last batch, only what really renamed
    void setExecRenameDialog(std::function<bool(RenameManyDialog &)> exec) { m_execRenameDialog = std::move(exec); } // test seam
    // Tags: write the xattr, then touch the registry and repaint the rows; errors go through the showError seam.
    void addTag(const KFileItemList &, const Tag &);
    void removeTag(const KFileItemList &, const Tag &);
    void newTag(const KFileItemList &); // NewTagDialog, then addTag
    QMenu *tagsMenu(const KFileItemList &); // caller owns
    void fillRemoveMenu(QMenu *, const KFileItemList &); // Win12 tags only, never foreign ones
    TagRegistry &registry() { return m_registry; }
    void setExecNewTagDialog(std::function<bool(NewTagDialog &)> exec) { m_execNewTagDialog = std::move(exec); } // test seam
    void setShowError(std::function<void(const QString &)> show) { m_showError = std::move(show); } // test seam
    static QString rename(const KFileItem &item, const QString &newName, QWidget *parent);

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
    void selectUrlsRequested(const QList<QUrl> &); // Select by rule, Action::Select
    void manageTagsRequested();
    void fillWin12Menu(QMenu *, const KFileItemList &); // seam for phases 2-4

private:
    QAction *add(const QString &id, const QString &text, const QString &icon);
    void connectActions();
    void buildViewMenus();
    void updateEnabled();
    void syncViewState(); // check marks of layout/sort/group/columns/show from the view's settings
    KFileItemList selection() const;
    QUrl folder() const;
    QString runRenames(const QList<PlannedRename> &, QList<PlannedRename> *executed); // sequential; stops at the first error

    // The work, on explicit items/folders so context menus act on what they were opened for.
    void copyToClipboard(const KFileItemList &, bool cut);
    void paste(const QUrl &folder);
    void pasteShortcut(const QUrl &folder);
    void open(const KFileItemList &);
    void properties(const KFileItemList &, const QUrl &folder);
    void launch(const QString &exe, const QStringList &args);
    void openTerminal(const QUrl &folder);

    QHash<QString, QAction *> m_actions;
    QPointer<FolderView> m_view;
    KNewFileMenu *m_newFileMenu;
    Purpose::Menu *m_shareMenu;
    std::function<QUrl(const QUrl &)> m_pickFolder;
    std::function<bool(RenameManyDialog &)> m_execRenameDialog = [](RenameManyDialog &d) { return d.exec() == QDialog::Accepted; };
    void fillTagsMenu(QMenu *, const KFileItemList &);
    QWidget *m_window;
    TagStore m_tagStore;
    RuleLibrary m_library;
    std::function<void(ScriptDialog &)> m_execScriptDialog = [](ScriptDialog &d) { d.exec(); };
    std::function<bool(SelectByRuleDialog &)> m_execSelectDialog = [](SelectByRuleDialog &d) { return d.exec() == QDialog::Accepted; };
    void fillRulesMenu(QMenu *, const KFileItemList &items, bool selectRename, bool scripts);
    void selectDialog(SelectByRuleDialog::Action defaultAction, const QString &formula, bool formulaMode);
    void renameManyImpl(const KFileItemList &items, const QString &formula, bool formulaMode);
    TagRegistry m_registry;
    std::function<bool(NewTagDialog &)> m_execNewTagDialog = [](NewTagDialog &d) { return d.exec() == QDialog::Accepted; };
    std::function<void(const QString &)> m_showError;
    QList<PlannedRename> m_lastBatch; // the rows that actually renamed
};

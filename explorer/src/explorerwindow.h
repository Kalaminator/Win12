#pragma once
#include "folderview.h" // complete type for QPointer<FolderView>
#include <KFileItem>
#include <KMainWindow>
#include <KSharedConfig>
#include <QList>
#include <QPointer>
#include <QUrl>
#include <QWidget>
#include <functional>

class ExplorerActions;
class KFilePlacesView;
class KUrlNavigator;
class QAction;
class QLabel;
class QLineEdit;
class QSplitter;
class QStackedWidget;
class QTabBar;
class QToolButton;
class RibbonBar;
namespace KIO { class PreviewJob; }

// One tab: a QSplitter with one or two FolderViews; the one that last had focus is active().
class TabPage : public QWidget
{
    Q_OBJECT
public:
    explicit TabPage(ViewSettingsStore *store, const QUrl &url, QWidget *parent = nullptr);
    FolderView *active() const { return m_active; }
    FolderView *primary() const { return m_primary; }
    FolderView *secondary() const { return m_secondary; } // nullptr when not split
    bool isSplit() const { return m_secondary; }
    void setSplit(bool); // the new pane opens primary()->url()
    void activate(FolderView *); // no-op unless the view is one of this page's panes
    QUrl url() const;
    QString title() const;
    static QString titleFor(const QUrl &); // last path segment, or the host for remote urls

Q_SIGNALS:
    void activeChanged(FolderView *);
    void titleChanged(const QString &);

private:
    FolderView *addView(const QUrl &); // inside a container with its 2 px "paneMark" above it
    void updateMarks(); // the mark shows on the active pane while split
    ViewSettingsStore *m_store;
    QSplitter *m_splitter;
    FolderView *m_primary;
    QPointer<FolderView> m_secondary;
    FolderView *m_active;
};

// Right-hand pane: big preview of the current item (Preview) or its name/type/size/modified (Details).
class SidePane : public QWidget
{
    Q_OBJECT
public:
    enum Mode { Preview, Details };
    explicit SidePane(QWidget *parent = nullptr);
    Mode mode() const { return m_mode; }
    void setMode(Mode);
    void showItem(const KFileItem &); // null item = nothing selected

private:
    void refresh();
    Mode m_mode = Preview;
    KFileItem m_item;
    QPointer<KIO::PreviewJob> m_job;
    QLabel *m_image;
    QLabel *m_name;
    QLabel *m_details;
};

class ExplorerWindow : public KMainWindow
{
    Q_OBJECT
public:
    explicit ExplorerWindow(const QList<QUrl> &urls = {}, QWidget *parent = nullptr); // empty: the saved session
    void openTab(const QUrl &, bool activate = true);
    void closeTab(int); // closing the last tab closes the window
    TabPage *currentPage() const;
    FolderView *activeView() const;
    void selectItems(const QList<QUrl> &); // opens the parent folder in the current tab, then selects
    void showProperties(const QList<QUrl> &);

    static QList<QUrl> savedSession(const KSharedConfig::Ptr &); // [Session] Tabs
    static QList<QUrl> sessionUrlsToOpen(const QList<QUrl> &saved); // drops missing local folders; empty: Home
    static QStringList recentFolders(const KSharedConfig::Ptr &); // [Recent] Urls, newest first, max 20
    static void addRecentFolder(const KSharedConfig::Ptr &, const QUrl &);

protected:
    bool queryClose() override; // writes [Session] from the open tabs
    bool eventFilter(QObject *, QEvent *) override;

private:
    void buildAddressRow(QWidget *central);
    void buildStatusBar();
    void connectActions();
    void wireView(FolderView *);
    void syncToActiveView(); // address row, title, status, side pane, actions, places
    void updateStatus();
    void updateSidePane();
    void editSettings(const std::function<void(ViewSettings &)> &); // applySettings on the active view
    void togglePane(const QString &pane, bool on);
    void fillRecentMenu();

    KSharedConfig::Ptr m_config;
    ViewSettingsStore m_store;
    ExplorerActions *m_actions;
    RibbonBar *m_ribbon;
    QAction *m_back;
    QAction *m_forward;
    KUrlNavigator *m_navigator;
    QLineEdit *m_search;
    QTabBar *m_tabs;
    QStackedWidget *m_stack;
    KFilePlacesView *m_places;
    SidePane *m_side;
    QLabel *m_statusText;
    QToolButton *m_detailsButton;
    QToolButton *m_largeButton;
    QPointer<FolderView> m_filtered; // the view the search box last filtered
};

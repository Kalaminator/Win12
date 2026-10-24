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
class TagList;
class QAction;
class QLabel;
class QLineEdit;
class QSplitter;
class QToolButton;
class RibbonBar;
class TitleBar;
namespace KIO { class PreviewJob; }

// The window's page: a QSplitter with one or two FolderViews; the one that last had focus is active().
class FolderPage : public QWidget
{
    Q_OBJECT
public:
    explicit FolderPage(ViewSettingsStore *store, const QUrl &url, QWidget *parent = nullptr);
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
    explicit ExplorerWindow(const QUrl &url = {}, QWidget *parent = nullptr); // empty: the saved session
    ~ExplorerWindow() override;
    static constexpr int ResizeBorder = 6; // frameless: the band at the window edges that resizes
    static Qt::Edges resizeEdgesAt(const QPoint &pos, const QSize &size, int border = ResizeBorder); // corners: two edges
    Qt::Edges edgesForPress(const QPoint &windowPos) const; // none while maximised or full screen
    TitleBar *titleBar() const { return m_titleBar; }
    FolderPage *page() const;
    FolderView *activeView() const;
    void runSavedRule(const QString &name, const QList<QUrl> &urls); // Dolphin's Win12 > My rules; unknown name: error box
    static QUrl folderFor(const QList<QUrl> &urls); // urls[0] when a local folder, else its parent; empty: Home
    static QUrl folderFromStat(const QUrl &url, bool isDir); // the url itself for a folder, else its parent
    void withFolderFor(const QList<QUrl> &urls, std::function<void(const QUrl &)> then); // remote urls are stat-ed async; then runs once the folder is known
    void selectByRuleOn(const QList<QUrl> &urls); // Dolphin's Win12 > Select by rule
    void renameManyOn(const QList<QUrl> &urls); // Dolphin's Win12 > Rename many
    static ExplorerWindow *last(); // newest living window, or null
    ExplorerActions *explorerActions() const { return m_actions; } // not actions(): QWidget has one
    void selectItems(const QList<QUrl> &); // opens the parent folder in the page, then selects
    void showProperties(const QList<QUrl> &);

    static QList<QUrl> savedSession(const KSharedConfig::Ptr &); // [Session] Url (+ SecondUrl when split); no Url: {}
    static QList<QUrl> sessionUrlsToOpen(const QList<QUrl> &saved); // drops missing local folders; empty: Home
    static QStringList recentFolders(const KSharedConfig::Ptr &); // [Recent] Urls, newest first, max 20
    static void addRecentFolder(const KSharedConfig::Ptr &, const QUrl &);

protected:
    bool queryClose() override; // writes [Session] from the page
    bool eventFilter(QObject *watched, QEvent *event) override; // app-wide: resize band cursor and press
    void changeEvent(QEvent *) override;
    void leaveEvent(QEvent *) override;
    void paintEvent(QPaintEvent *) override; // the 1 px outline in place of KWin's frame

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
    TitleBar *m_titleBar = nullptr;
    Qt::Edges m_cursorEdges; // edges whose resize cursor is overridden, none: not overridden
    void setResizeCursor(Qt::Edges);
    QAction *m_back;
    QAction *m_forward;
    KUrlNavigator *m_navigator;
    QLineEdit *m_search;
    FolderPage *m_page;
    KFilePlacesView *m_places;
    TagList *m_tagList;
    QLabel *m_resultsTitle;
    QAction *m_resultsTitleAction;
    SidePane *m_side;
    QLabel *m_statusText;
    QToolButton *m_detailsButton;
    QToolButton *m_largeButton;
    QPointer<FolderView> m_filtered; // the view the search box last filtered
};

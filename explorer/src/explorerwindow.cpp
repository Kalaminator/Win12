#include "explorerwindow.h"
#include "exploreractions.h"
#include "foldermodel.h"
#include "folderview.h"
#include "taglist.h"
#include "tagregistry.h"
#include "tagsearch.h"
#include "ribbonbar.h"
#include "ribbonspec.h"
#include <KConfigGroup>
#include <KDirLister>
#include <KFilePlacesModel>
#include <KFilePlacesView>
#include <KIO/DropJob>
#include <KIO/FileUndoManager>
#include <KIO/JobUiDelegateFactory>
#include <KIO/OpenUrlJob>
#include <KIO/PreviewJob>
#include <KIO/StatJob>
#include <KJobWidgets>
#include <KLocalizedString>
#include <KMessageBox>
#include <KPropertiesDialog>
#include <KStandardAction>
#include <KUrlComboBox>
#include <KUrlNavigator>
#include <QAbstractItemView>
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTabBar>
#include <QTimer>
#include <algorithm>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

// ---------------------------------------------------------------- TabPage

TabPage::TabPage(ViewSettingsStore *store, const QUrl &url, QWidget *parent)
    : QWidget(parent)
    , m_store(store)
    , m_splitter(new QSplitter(Qt::Horizontal, this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_splitter);
    m_primary = addView(url);
    m_active = m_primary;
    // A click (any button) or Tab into a pane focuses it, which makes it the active one.
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget *, QWidget *now) {
        if (!now)
            return;
        if (m_secondary && m_secondary->isAncestorOf(now))
            activate(m_secondary);
        else if (m_primary->isAncestorOf(now))
            activate(m_primary);
    });
}

namespace {
struct PaneMark : QWidget {
    using QWidget::QWidget;
    void paintEvent(QPaintEvent *) override { QPainter(this).fillRect(rect(), palette().highlight()); }
};
}

FolderView *TabPage::addView(const QUrl &url)
{
    auto *box = new QWidget(m_splitter);
    auto *layout = new QVBoxLayout(box);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    auto *mark = new PaneMark(box);
    mark->setObjectName(QStringLiteral("paneMark"));
    mark->setFixedHeight(2);
    mark->hide();
    layout->addWidget(mark);
    auto *view = new FolderView(m_store, box);
    layout->addWidget(view);
    m_splitter->addWidget(box);
    connect(view, &FolderView::urlChanged, this, [this, view] {
        if (view == m_active)
            Q_EMIT titleChanged(title());
    });
    view->openUrl(url);
    return view;
}

void TabPage::setSplit(bool on)
{
    if (on == isSplit())
        return;
    if (on) {
        m_secondary = addView(m_primary->url());
        const int half = m_splitter->width() / 2;
        m_splitter->setSizes({half, half});
        updateMarks();
        return;
    }
    FolderView *gone = m_secondary;
    m_secondary = nullptr;
    if (m_active == gone)
        activate(m_primary);
    delete gone->parentWidget();
    updateMarks();
}

void TabPage::updateMarks()
{
    for (FolderView *view : {m_primary, m_secondary.data()})
        if (view)
            view->parentWidget()->findChild<QWidget *>(QStringLiteral("paneMark"), Qt::FindDirectChildrenOnly)->setVisible(isSplit() && view == m_active);
}

void TabPage::activate(FolderView *view)
{
    if (view == m_active || (view != m_primary && view != m_secondary))
        return;
    m_active = view;
    updateMarks();
    Q_EMIT activeChanged(view);
    Q_EMIT titleChanged(title());
}

QUrl TabPage::url() const
{
    return m_active->url();
}

QString TabPage::title() const
{
    return titleFor(url());
}

QString TabPage::titleFor(const QUrl &url)
{
    QString name = url.adjusted(QUrl::StripTrailingSlash).fileName();
    if (name.isEmpty())
        name = url.isLocalFile() ? u"/"_s : url.host();
    return name.isEmpty() ? url.toDisplayString() : name;
}

// ---------------------------------------------------------------- SidePane

SidePane::SidePane(QWidget *parent)
    : QWidget(parent)
    , m_image(new QLabel(this))
    , m_name(new QLabel(this))
    , m_details(new QLabel(this))
{
    auto *layout = new QVBoxLayout(this);
    m_image->setAlignment(Qt::AlignCenter);
    m_image->setMinimumSize(64, 64);
    m_name->setAlignment(Qt::AlignHCenter);
    m_name->setWordWrap(true);
    m_details->setWordWrap(true);
    m_details->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QFont bold = m_name->font();
    bold.setBold(true);
    m_name->setFont(bold);
    layout->addWidget(m_image);
    layout->addWidget(m_name);
    layout->addWidget(m_details);
    layout->addStretch(1);
    setMinimumWidth(200);
    refresh();
}

void SidePane::setMode(Mode mode)
{
    m_mode = mode;
    refresh();
}

void SidePane::showItem(const KFileItem &item)
{
    m_item = item;
    refresh();
}

void SidePane::refresh()
{
    if (m_job)
        m_job->kill();
    m_details->setVisible(m_mode == Details);
    if (m_item.isNull()) {
        m_image->clear();
        m_name->setText(m_mode == Preview ? i18n("Select a file to preview.") : i18n("Select an item to see its details."));
        m_details->clear();
        return;
    }
    m_name->setText(m_item.name());
    const int size = m_mode == Preview ? 256 : 64;
    m_image->setPixmap(QIcon::fromTheme(m_item.iconName()).pixmap(size));
    if (m_mode == Details) {
        QStringList lines{i18n("Type: %1", m_item.mimeComment())};
        if (!m_item.isDir())
            lines << i18n("Size: %1", KIO::convertSize(m_item.size()));
        lines << i18n("Modified: %1", QLocale().toString(m_item.time(KFileItem::ModificationTime), QLocale::ShortFormat));
        m_details->setText(lines.join(QLatin1Char('\n')));
        return;
    }
    m_job = KIO::filePreview(KFileItemList{m_item}, QSize(size, size));
    connect(m_job, &KIO::PreviewJob::gotPreview, this, [this](const KFileItem &, const QPixmap &pixmap) { m_image->setPixmap(pixmap); });
}

// ---------------------------------------------------------------- ExplorerWindow

static ExplorerWindow *s_last = nullptr; // newest living window

ExplorerWindow::ExplorerWindow(const QList<QUrl> &urls, QWidget *parent)
    : KMainWindow(parent)
    , m_config(KSharedConfig::openConfig())
    , m_store(m_config)
    , m_actions(new ExplorerActions(this))
{
    s_last = this;
    connect(this, &QObject::destroyed, [this] {
        if (s_last == this)
            s_last = nullptr;
    });
    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    m_ribbon = new RibbonBar(m_actions, central);
    layout->addWidget(m_ribbon);
    buildAddressRow(central);

    m_tabs = new QTabBar(central);
    m_tabs->setTabsClosable(true);
    m_tabs->setMovable(true);
    m_tabs->setDocumentMode(true);
    m_tabs->setExpanding(false);
    m_tabs->setElideMode(Qt::ElideRight);
    m_tabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed); // the empty strip takes double-clicks
    m_tabs->installEventFilter(this);
    layout->addWidget(m_tabs);

    auto *splitter = new QSplitter(Qt::Horizontal, central);
    auto *left = new QWidget(splitter);
    auto *leftBox = new QVBoxLayout(left);
    leftBox->setContentsMargins(0, 0, 0, 0);
    m_places = new KFilePlacesView(left);
    m_places->setModel(new KFilePlacesModel(m_places));
    m_places->setDropOnPlaceEnabled(true);
    m_tagList = new TagList(&m_actions->registry(), left);
    m_tagList->setVisible(!m_actions->registry().tags().isEmpty());
    leftBox->addWidget(m_places, 1);
    leftBox->addWidget(m_tagList);
    connect(&m_actions->registry(), &TagRegistry::changed, this, [this] { m_tagList->setVisible(!m_actions->registry().tags().isEmpty()); });
    connect(m_tagList, &TagList::filterChanged, this, [this](const QList<Tag> &picked, TagSearch::Mode mode) {
        FolderView *v = activeView();
        if (!v)
            return;
        if (picked.isEmpty()) {
            v->clearResults();
            return;
        }
        QStringList words;
        for (const Tag &t : picked)
            words << t.display();
        auto *search = new TagSearch;
        v->showResults(i18n("Tagged: %1", words.join(u", "_s)), search);
        search->start(v->url(), picked, mode);
    });
    m_stack = new QStackedWidget(splitter);
    m_side = new SidePane(splitter);
    m_side->hide();
    splitter->setStretchFactor(1, 1);
    splitter->setCollapsible(1, false);
    splitter->setSizes({200, 900, 260});
    layout->addWidget(splitter, 1);
    setCentralWidget(central);
    buildStatusBar();

    connect(m_places, &KFilePlacesView::urlChanged, this, [this](const QUrl &url) {
        if (FolderView *v = activeView(); v && v->url() != url) {
            v->openUrl(url);
            v->currentView()->setFocus(); // file keys (Delete, Ctrl+C...) act on the pane again
        }
    });
    connect(m_places, &KFilePlacesView::tabRequested, this, [this](const QUrl &url) { openTab(url, false); });
    connect(m_places, &KFilePlacesView::urlsDropped, this, [this](const QUrl &dest, QDropEvent *e, QWidget *) {
        KIO::DropJob *job = KIO::drop(e, dest);
        KJobWidgets::setWindow(job, this);
        connect(job, &KIO::DropJob::copyJobStarted, job, [](KIO::CopyJob *copy) { KIO::FileUndoManager::self()->recordCopyJob(copy); });
    });
    connect(m_tabs, &QTabBar::currentChanged, this, [this](int index) {
        m_stack->setCurrentIndex(index);
        syncToActiveView();
        if (FolderView *v = activeView())
            v->currentView()->setFocus();
    });
    connect(m_tabs, &QTabBar::tabCloseRequested, this, &ExplorerWindow::closeTab);
    connect(m_tabs, &QTabBar::tabMoved, this, [this](int from, int to) {
        QWidget *page = m_stack->widget(from);
        m_stack->removeWidget(page);
        m_stack->insertWidget(to, page);
        m_stack->setCurrentIndex(m_tabs->currentIndex());
    });
    connectActions();

    KConfigGroup windowGroup(m_config, u"MainWindow"_s);
    m_ribbon->setCollapsed(windowGroup.readEntry("RibbonCollapsed", false));
    connect(m_ribbon, &RibbonBar::collapsedChanged, this, [this](bool collapsed) {
        KConfigGroup(m_config, u"MainWindow"_s).writeEntry("RibbonCollapsed", collapsed);
    });

    QList<QUrl> toOpen = urls;
    int active = 0;
    if (toOpen.isEmpty()) {
        toOpen = sessionUrlsToOpen(savedSession(m_config));
        active = qBound(0, KConfigGroup(m_config, u"Session"_s).readEntry("Active", 0), int(toOpen.size()) - 1);
    }
    for (const QUrl &url : std::as_const(toOpen))
        openTab(url, false);
    m_tabs->setCurrentIndex(active);
    syncToActiveView();
    if (FolderView *v = activeView()) {
        addRecentFolder(m_config, v->url()); // urlChanged ran before wireView for the start folder
        v->currentView()->setFocus();
    }

    resize(1100, 700);
    setAutoSaveSettings(u"MainWindow"_s);
}

void ExplorerWindow::buildAddressRow(QWidget *central)
{
    auto *bar = new QToolBar(central);
    bar->setMovable(false);
    bar->setFloatable(false);
    bar->setIconSize(QSize(16, 16));
    central->layout()->addWidget(bar);

    m_back = KStandardAction::back(this, [this] { if (FolderView *v = activeView()) v->goBack(); }, bar);
    m_forward = KStandardAction::forward(this, [this] { if (FolderView *v = activeView()) v->goForward(); }, bar);
    bar->addAction(m_back);
    bar->addAction(m_forward);

    QMenu *recentMenu = m_actions->historyMenu();
    connect(recentMenu, &QMenu::aboutToShow, this, &ExplorerWindow::fillRecentMenu);
    auto *recent = new QToolButton(bar);
    recent->setMenu(recentMenu);
    recent->setPopupMode(QToolButton::InstantPopup);
    recent->setArrowType(Qt::DownArrow);
    recent->setAutoRaise(true);
    recent->setToolTip(i18n("Recent locations"));
    recent->setStyleSheet(u"QToolButton::menu-indicator { image: none; }"_s);
    bar->addWidget(recent);
    bar->addAction(KStandardAction::up(this, [this] { if (FolderView *v = activeView()) v->goUp(); }, bar));

    m_navigator = new KUrlNavigator(new KFilePlacesModel(this), QUrl(), bar);
    m_navigator->setUrlEditable(false);
    m_navigator->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    bar->addWidget(m_navigator);
    m_resultsTitle = new QLabel(bar);
    m_resultsTitleAction = bar->addWidget(m_resultsTitle);
    m_resultsTitleAction->setVisible(false);
    connect(m_navigator, &KUrlNavigator::urlChanged, this, [this](const QUrl &url) {
        if (FolderView *v = activeView(); v && v->url() != url)
            v->openUrl(url);
    });
    connect(m_navigator, &KUrlNavigator::returnPressed, this, [this] {
        QTimer::singleShot(0, this, [this] { // after the navigator applied the typed url
            m_navigator->setUrlEditable(false);
            if (FolderView *v = activeView())
                v->currentView()->setFocus();
        });
    });
    auto *editAddress = new QAction(this);
    editAddress->setShortcuts({QKeySequence(Qt::CTRL | Qt::Key_L), QKeySequence(Qt::Key_F6)});
    addAction(editAddress);
    connect(editAddress, &QAction::triggered, this, [this] {
        const bool edit = !m_navigator->isUrlEditable();
        m_navigator->setUrlEditable(edit);
        if (edit) {
            m_navigator->editor()->setFocus();
            m_navigator->editor()->lineEdit()->selectAll();
        } else if (FolderView *v = activeView()) {
            v->currentView()->setFocus();
        }
    });

    bar->addAction(KStandardAction::redisplay(this, [this] { if (FolderView *v = activeView()) v->refresh(); }, bar));

    m_search = new QLineEdit(bar);
    m_search->setFixedWidth(180);
    m_search->addAction(QIcon::fromTheme(u"edit-find"_s), QLineEdit::TrailingPosition);
    bar->addWidget(m_search);
    connect(m_search, &QLineEdit::textChanged, this, [this](const QString &text) {
        FolderView *v = activeView();
        if (m_filtered && m_filtered != v)
            m_filtered->setNameFilter(QString());
        if (v)
            v->setNameFilter(text);
        m_filtered = v;
    });
    auto *clearSearch = new QAction(m_search);
    clearSearch->setShortcut(Qt::Key_Escape);
    clearSearch->setShortcutContext(Qt::WidgetShortcut);
    m_search->addAction(clearSearch); // QWidget::addAction: no icon in the line edit
    connect(clearSearch, &QAction::triggered, m_search, &QLineEdit::clear);
}

void ExplorerWindow::buildStatusBar()
{
    m_statusText = new QLabel(this);
    statusBar()->addWidget(m_statusText, 1);
    const auto modeButton = [this](const QString &icon, const QString &tip, ViewMode mode) {
        auto *button = new QToolButton(this);
        button->setIcon(QIcon::fromTheme(icon));
        button->setToolTip(tip);
        button->setCheckable(true);
        button->setAutoRaise(true);
        statusBar()->addPermanentWidget(button);
        connect(button, &QToolButton::clicked, this, [this, mode] { editSettings([mode](ViewSettings &s) { s.mode = mode; }); });
        return button;
    };
    m_detailsButton = modeButton(u"view-list-details"_s, i18n("Details"), ViewMode::Details);
    m_largeButton = modeButton(u"view-list-icons"_s, i18n("Large icons"), ViewMode::LargeIcons);
}

void ExplorerWindow::connectActions()
{
    // Keys that also mean text editing / place handling act on files only while focus is in the file panes.
    const QStringList paneOnly{u"home_delete_trash"_s, u"home_delete_permanent"_s, u"home_rename"_s, u"home_select_all"_s,
                               u"home_copy"_s, u"home_cut"_s, u"home_paste"_s};
    for (const QString &id : Ribbon::actionIds() + QStringList{u"home_delete_trash"_s, u"home_delete_permanent"_s}) {
        QAction *a = m_actions->action(id);
        if (paneOnly.contains(id)) {
            a->setShortcutContext(Qt::WidgetWithChildrenShortcut);
            m_stack->addAction(a);
        } else {
            addAction(a);
        }
    }

    const auto shortcut = [this](const QKeySequence &keys, auto fn) {
        auto *a = new QAction(this);
        a->setShortcut(keys);
        addAction(a);
        connect(a, &QAction::triggered, this, fn);
    };
    shortcut(QKeySequence(Qt::CTRL | Qt::Key_T), [this] { openTab(currentPage() ? currentPage()->url() : QUrl::fromLocalFile(QDir::homePath())); });
    shortcut(QKeySequence(Qt::CTRL | Qt::Key_W), [this] { closeTab(m_tabs->currentIndex()); });
    shortcut(QKeySequence(Qt::CTRL | Qt::Key_Tab), [this] { m_tabs->setCurrentIndex((m_tabs->currentIndex() + 1) % m_tabs->count()); });
    shortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Backtab),
             [this] { m_tabs->setCurrentIndex((m_tabs->currentIndex() + m_tabs->count() - 1) % m_tabs->count()); });

    connect(m_actions, &ExplorerActions::newWindowRequested, this, [this] {
        (new ExplorerWindow({activeView() ? activeView()->url() : QUrl::fromLocalFile(QDir::homePath())}))->show();
    });
    connect(m_actions, &ExplorerActions::closeRequested, this, &QWidget::close);
    // optionsRequested / helpRequested: ExplorerActions already launched settings / help.
    connect(m_actions, &ExplorerActions::layoutRequested, this, [this](ViewMode mode) { editSettings([mode](ViewSettings &s) { s.mode = mode; }); });
    connect(m_actions, &ExplorerActions::sortRequested, this, [this](SortRole role, Qt::SortOrder order) {
        editSettings([role, order](ViewSettings &s) {
            s.sortRole = role;
            s.sortOrder = order;
        });
    });
    connect(m_actions, &ExplorerActions::selectUrlsRequested, this, [this](const QList<QUrl> &urls) {
        if (FolderView *v = activeView())
            v->selectUrls(urls);
    });
    connect(m_actions, &ExplorerActions::groupRequested, this, [this](GroupRole role) { editSettings([role](ViewSettings &s) { s.groupRole = role; }); });
    connect(m_actions, &ExplorerActions::columnsToggled, this, [this](const QString &id, bool on) {
        editSettings([id, on](ViewSettings &s) {
            s.columns.removeAll(id);
            if (on)
                s.columns << id;
        });
    });
    connect(m_actions, &ExplorerActions::showToggled, this, [this](const QString &what, bool on) {
        editSettings([what, on](ViewSettings &s) {
            if (what == QLatin1String("checkboxes"))
                s.checkBoxes = on;
            else if (what == QLatin1String("extensions"))
                s.showExtensions = on;
            else if (what == QLatin1String("hidden"))
                s.hiddenFiles = on;
        });
    });
    connect(m_actions, &ExplorerActions::paneToggled, this, &ExplorerWindow::togglePane);
    connect(m_actions, &ExplorerActions::splitToggled, this, [this](bool on) {
        TabPage *page = currentPage();
        if (!page || page->isSplit() == on)
            return;
        page->setSplit(on);
        if (on)
            wireView(page->secondary());
        syncToActiveView();
    });
}

void ExplorerWindow::wireView(FolderView *v)
{
    connect(v, &FolderView::urlChanged, this, [this, v](const QUrl &url) {
        if (v != activeView())
            return;
        addRecentFolder(m_config, url);
        m_tagList->clearPicks(); // real navigation drops the tag filter
        v->clearResults();       // also when the list had no picks (results from an earlier tab visit)
        syncToActiveView();
    });
    connect(v, &FolderView::resultsTitleChanged, this, [this, v](const QString &title) {
        if (v != activeView())
            return;
        m_resultsTitle->setText(title);
        m_resultsTitleAction->setVisible(!title.isEmpty());
    });
    connect(v, &FolderView::countsChanged, this, [this, v] {
        if (v == activeView())
            updateStatus();
    });
    connect(v, &FolderView::selectionChanged, this, [this, v] {
        if (v == activeView())
            updateSidePane();
    });
    connect(v, &FolderView::folderMiddleClicked, this, [this](const QUrl &url) { openTab(url, false); });
    connect(v, &FolderView::fileActivated, this, [this](const KFileItem &item) {
        auto *job = new KIO::OpenUrlJob(item.url(), item.mimetype());
        job->setRunExecutables(false);
        job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, this));
        job->start();
    });
    connect(v, &FolderView::contextMenuRequested, this, [this, v](const QPoint &pos, const KFileItemList &items) {
        if (TabPage *page = currentPage())
            page->activate(v); // the menu's View entries act on the active view
        QMenu *menu = m_actions->contextMenu(items, v->url());
        connect(menu, &QMenu::aboutToHide, menu, &QObject::deleteLater);
        menu->popup(pos);
    });
    connect(v, &FolderView::renameRequested, this, [this](const KFileItem &item, const QString &name) {
        if (const QString error = ExplorerActions::rename(item, name, this); !error.isEmpty())
            KMessageBox::error(this, error);
    });
}

void ExplorerWindow::openTab(const QUrl &url, bool activate)
{
    auto *page = new TabPage(&m_store, url, m_stack);
    wireView(page->primary());
    connect(page, &TabPage::activeChanged, this, [this, page] {
        if (page == currentPage())
            syncToActiveView();
    });
    connect(page, &TabPage::titleChanged, this, [this, page](const QString &title) {
        const int index = m_stack->indexOf(page);
        m_tabs->setTabText(index, title);
        m_tabs->setTabToolTip(index, page->url().toDisplayString(QUrl::PreferLocalFile));
    });
    m_stack->addWidget(page); // before addTab: the first tab emits currentChanged
    const int index = m_tabs->addTab(page->title());
    m_tabs->setTabToolTip(index, url.toDisplayString(QUrl::PreferLocalFile));
    if (activate)
        m_tabs->setCurrentIndex(index);
}

void ExplorerWindow::closeTab(int index)
{
    if (m_tabs->count() <= 1) {
        close();
        return;
    }
    QWidget *page = m_stack->widget(index);
    if (!page)
        return;
    m_stack->removeWidget(page); // first, so currentChanged from removeTab finds the stack in step
    m_tabs->removeTab(index); // emits currentChanged when the current tab changes
    page->deleteLater();
}

TabPage *ExplorerWindow::currentPage() const
{
    return qobject_cast<TabPage *>(m_stack->currentWidget());
}

FolderView *ExplorerWindow::activeView() const
{
    TabPage *page = currentPage();
    return page ? page->active() : nullptr;
}

void ExplorerWindow::selectItems(const QList<QUrl> &urls)
{
    FolderView *v = activeView();
    if (!v || urls.isEmpty())
        return;
    const QUrl parent = urls.first().adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);
    if (v->url().adjusted(QUrl::StripTrailingSlash) != parent)
        v->openUrl(parent);
    KDirLister *lister = v->model()->folderModel()->lister();
    if (lister->isFinished())
        v->selectUrls(urls);
    else
        connect(lister, &KCoreDirLister::completed, v, [v, urls] { v->selectUrls(urls); }, Qt::SingleShotConnection);
}

QUrl ExplorerWindow::folderFor(const QList<QUrl> &urls)
{
    QUrl folder = urls.value(0);
    if (folder.isEmpty())
        return QUrl::fromLocalFile(QDir::homePath());
    return folderFromStat(folder, folder.isLocalFile() && QFileInfo(folder.toLocalFile()).isDir()); // remote: the parent guess
}

QUrl ExplorerWindow::folderFromStat(const QUrl &url, bool isDir)
{
    return isDir ? url : url.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);
}

void ExplorerWindow::withFolderFor(const QList<QUrl> &urls, std::function<void(const QUrl &)> then)
{
    const QUrl first = urls.value(0);
    if (first.isEmpty() || first.isLocalFile()) {
        then(folderFor(urls));
        return;
    }
    auto *job = KIO::stat(first, KIO::StatJob::SourceSide, KIO::StatBasic, KIO::HideProgressInfo);
    connect(job, &KJob::result, this, [this, job, first, then] {
        then(folderFromStat(first, !job->error() && job->statResult().isDir()));
    });
}

void ExplorerWindow::selectByRuleOn(const QList<QUrl> &urls)
{
    withFolderFor(urls, [this](const QUrl &folder) {
        if (FolderView *v = activeView())
            v->openUrl(folder);
        m_actions->selectByRule(SelectByRuleDialog::Action::Select);
    });
}

void ExplorerWindow::renameManyOn(const QList<QUrl> &urls)
{
    KFileItemList items;
    for (const QUrl &u : urls)
        items << KFileItem(u);
    if (items.isEmpty()) {
        KMessageBox::information(this, i18n("Select the files to rename first"));
        return;
    }
    withFolderFor(urls, [this, items](const QUrl &folder) {
        if (FolderView *v = activeView())
            v->openUrl(folder);
        m_actions->renameMany(items);
    });
}

ExplorerWindow *ExplorerWindow::last()
{
    return s_last;
}

void ExplorerWindow::runSavedRule(const QString &name, const QList<QUrl> &urls)
{
    const auto it = m_actions->library().find(name);
    if (!it) {
        KMessageBox::error(this, i18n("No saved rule named %1", name));
        return;
    }
    if (it->kind == SavedRule::Kind::Rename) {
        KFileItemList items;
        for (const QUrl &u : urls)
            items << KFileItem(u);
        if (items.isEmpty()) {
            KMessageBox::information(this, i18n("Select the files to rename first"));
            return;
        }
        m_actions->renameMany(items, it->text);
        return;
    }
    const SavedRule rule = *it;
    withFolderFor(urls, [this, rule](const QUrl &folder) {
        if (FolderView *v = activeView())
            v->openUrl(folder);
        if (rule.kind == SavedRule::Kind::Select)
            m_actions->selectByFormula(rule.text);
        else
            m_actions->openScript(rule.text);
    });
}

void ExplorerWindow::showProperties(const QList<QUrl> &urls)
{
    KPropertiesDialog::showDialog(urls, this, false);
}

void ExplorerWindow::syncToActiveView()
{
    FolderView *v = activeView();
    if (!v)
        return;
    m_actions->setActiveView(v);
    m_tagList->clearPicks(true); // each tab keeps its own results; the list just starts clean
    m_resultsTitle->setText(v->resultsTitle());
    m_resultsTitleAction->setVisible(!v->resultsTitle().isEmpty());
    const QUrl url = v->url();
    if (m_navigator->locationUrl() != url)
        m_navigator->setLocationUrl(url);
    m_places->setUrl(url);
    m_back->setEnabled(v->canGoBack());
    m_forward->setEnabled(v->canGoForward());
    m_actions->action(u"view_split"_s)->setChecked(currentPage()->isSplit());
    const QString title = currentPage()->title();
    setWindowTitle(i18n("%1 - Win12 Explorer", title));
    m_search->clear(); // a new folder or view drops the filter
    m_search->setPlaceholderText(i18n("Search %1", title));
    updateStatus();
    updateSidePane();
}

void ExplorerWindow::updateStatus()
{
    FolderView *v = activeView();
    if (!v)
        return;
    QString text = i18np("%1 item", "%1 items", v->itemCount());
    if (const int selected = v->selectedCount())
        text += i18np(" (%1 selected)", " (%1 selected)", selected);
    m_statusText->setText(text);
    const ViewMode mode = v->settings().mode;
    m_detailsButton->setChecked(mode == ViewMode::Details);
    m_largeButton->setChecked(mode == ViewMode::LargeIcons);
}

void ExplorerWindow::updateSidePane()
{
    FolderView *v = activeView();
    if (!v || m_side->isHidden())
        return;
    const KFileItemList items = v->selectedItems();
    m_side->showItem(items.size() == 1 ? items.first() : KFileItem());
}

void ExplorerWindow::editSettings(const std::function<void(ViewSettings &)> &change)
{
    FolderView *v = activeView();
    if (!v)
        return;
    ViewSettings s = v->settings();
    change(s);
    v->applySettings(s);
    m_actions->setActiveView(v); // re-syncs the ribbon's check marks
    updateStatus();
}

void ExplorerWindow::togglePane(const QString &pane, bool on)
{
    if (pane == QLatin1String("nav")) {
        m_places->setVisible(on);
        return;
    }
    const SidePane::Mode mode = pane == QLatin1String("preview") ? SidePane::Preview : SidePane::Details;
    if (on) {
        m_actions->action(mode == SidePane::Preview ? u"view_details_pane"_s : u"view_preview_pane"_s)->setChecked(false);
        m_side->setMode(mode);
        m_side->show();
        updateSidePane();
    } else if (m_side->mode() == mode) {
        m_side->hide();
    }
}

void ExplorerWindow::fillRecentMenu()
{
    QMenu *menu = m_actions->historyMenu();
    menu->clear();
    const QStringList recent = recentFolders(m_config);
    for (const QString &entry : recent) {
        const QUrl url(entry);
        QAction *a = menu->addAction(QIcon::fromTheme(u"folder"_s), url.toDisplayString(QUrl::PreferLocalFile));
        connect(a, &QAction::triggered, this, [this, url] {
            if (FolderView *v = activeView())
                v->openUrl(url);
        });
    }
    if (recent.isEmpty())
        menu->addAction(i18n("No recent folders"))->setEnabled(false);
}

bool ExplorerWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_tabs) {
        if (event->type() == QEvent::MouseButtonRelease) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::MiddleButton && m_tabs->tabAt(me->position().toPoint()) >= 0) {
                closeTab(m_tabs->tabAt(me->position().toPoint()));
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonDblClick) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton && m_tabs->tabAt(me->position().toPoint()) < 0) {
                openTab(QUrl::fromLocalFile(QDir::homePath()));
                return true;
            }
        }
    }
    return KMainWindow::eventFilter(watched, event);
}

bool ExplorerWindow::queryClose()
{
    QStringList tabs;
    for (int i = 0; i < m_tabs->count(); ++i)
        tabs << static_cast<TabPage *>(m_stack->widget(i))->url().toString();
    KConfigGroup session(m_config, u"Session"_s);
    session.writeEntry("Tabs", tabs);
    session.writeEntry("Active", m_tabs->currentIndex());
    m_config->sync();
    return true;
}

QList<QUrl> ExplorerWindow::savedSession(const KSharedConfig::Ptr &config)
{
    QList<QUrl> urls;
    const QStringList tabs = KConfigGroup(config, u"Session"_s).readEntry("Tabs", QStringList());
    for (const QString &tab : tabs)
        urls << QUrl(tab);
    return urls;
}

QList<QUrl> ExplorerWindow::sessionUrlsToOpen(const QList<QUrl> &saved)
{
    QList<QUrl> urls;
    for (const QUrl &url : saved) {
        if (!url.isLocalFile() || QFileInfo::exists(url.toLocalFile())) // remote urls are kept untested
            urls << url;
    }
    if (urls.isEmpty())
        urls << QUrl::fromLocalFile(QDir::homePath());
    return urls;
}

QStringList ExplorerWindow::recentFolders(const KSharedConfig::Ptr &config)
{
    return KConfigGroup(config, u"Recent"_s).readEntry("Urls", QStringList()).mid(0, 20);
}

void ExplorerWindow::addRecentFolder(const KSharedConfig::Ptr &config, const QUrl &url)
{
    if (!url.isValid())
        return;
    QStringList recent = recentFolders(config);
    recent.removeAll(url.toString());
    recent.prepend(url.toString());
    KConfigGroup(config, u"Recent"_s).writeEntry("Urls", recent.mid(0, 20));
}

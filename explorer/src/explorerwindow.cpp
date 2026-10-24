#include "explorerwindow.h"
#include "exploreractions.h"
#include "foldermodel.h"
#include "folderview.h"
#include "taglist.h"
#include "tagregistry.h"
#include "tagsearch.h"
#include "ribbonbar.h"
#include "ribbonspec.h"
#include "titlebar.h"
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
#include <QGuiApplication>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QSplitter>
#include <QStatusBar>
#include <QTimer>
#include <algorithm>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWindow>

using namespace Qt::StringLiterals;

// ---------------------------------------------------------------- FolderPage

FolderPage::FolderPage(ViewSettingsStore *store, const QUrl &url, QWidget *parent)
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

FolderView *FolderPage::addView(const QUrl &url)
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

void FolderPage::setSplit(bool on)
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

void FolderPage::updateMarks()
{
    for (FolderView *view : {m_primary, m_secondary.data()})
        if (view)
            view->parentWidget()->findChild<QWidget *>(QStringLiteral("paneMark"), Qt::FindDirectChildrenOnly)->setVisible(isSplit() && view == m_active);
}

void FolderPage::activate(FolderView *view)
{
    if (view == m_active || (view != m_primary && view != m_secondary))
        return;
    m_active = view;
    updateMarks();
    Q_EMIT activeChanged(view);
    Q_EMIT titleChanged(title());
}

QUrl FolderPage::url() const
{
    return m_active->url();
}

QString FolderPage::title() const
{
    return titleFor(url());
}

QString FolderPage::titleFor(const QUrl &url)
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

ExplorerWindow::ExplorerWindow(const QUrl &url, QWidget *parent)
    : KMainWindow(parent)
    , m_config(KSharedConfig::openConfig())
    , m_store(m_config)
    , m_actions(new ExplorerActions(this))
{
    setWindowFlag(Qt::FramelessWindowHint); // the title bar is ours (TitleBar); KWin still snaps, tiles, shadows
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
    m_titleBar = new TitleBar(m_ribbon, KSharedConfig::openConfig(u"kdeglobals"_s), central); // relative name: KConfigWatcher needs it
    layout->addWidget(m_titleBar);
    layout->addWidget(m_ribbon);
    buildAddressRow(central);

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
    const QList<QUrl> urls = url.isValid() ? QList<QUrl>{url} : sessionUrlsToOpen(savedSession(m_config));
    m_page = new FolderPage(&m_store, urls[0], splitter);
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
    connect(m_places, &KFilePlacesView::tabRequested, this, [this](const QUrl &url) { (new ExplorerWindow(url))->show(); });
    connect(m_places, &KFilePlacesView::urlsDropped, this, [this](const QUrl &dest, QDropEvent *e, QWidget *) {
        KIO::DropJob *job = KIO::drop(e, dest);
        KJobWidgets::setWindow(job, this);
        connect(job, &KIO::DropJob::copyJobStarted, job, [](KIO::CopyJob *copy) { KIO::FileUndoManager::self()->recordCopyJob(copy); });
    });
    connectActions();

    KConfigGroup windowGroup(m_config, u"MainWindow"_s);
    m_ribbon->setCollapsed(windowGroup.readEntry("RibbonCollapsed", false));
    connect(m_ribbon, &RibbonBar::collapsedChanged, this, [this](bool collapsed) {
        KConfigGroup(m_config, u"MainWindow"_s).writeEntry("RibbonCollapsed", collapsed);
    });

    wireView(m_page->primary());
    connect(m_page, &FolderPage::activeChanged, this, &ExplorerWindow::syncToActiveView);
    if (urls.size() == 2) {
        m_page->setSplit(true);
        wireView(m_page->secondary());
        m_page->secondary()->openUrl(urls[1]);
    }
    syncToActiveView();
    if (FolderView *v = activeView()) {
        addRecentFolder(m_config, v->url()); // urlChanged ran before wireView for the start folder
        v->currentView()->setFocus();
    }

    resize(1100, 700);
    setAutoSaveSettings(u"MainWindow"_s);
    setContentsMargins(1, 1, 1, 1); // room for the outline
    qApp->installEventFilter(this); // child widgets cover the edges: the band is found app-wide
}

ExplorerWindow::~ExplorerWindow()
{
    qApp->removeEventFilter(this);
    setResizeCursor({});
}

Qt::Edges ExplorerWindow::resizeEdgesAt(const QPoint &pos, const QSize &size, int border)
{
    Qt::Edges e;
    if (pos.x() < border)
        e |= Qt::LeftEdge;
    else if (pos.x() >= size.width() - border)
        e |= Qt::RightEdge;
    if (pos.y() < border)
        e |= Qt::TopEdge;
    else if (pos.y() >= size.height() - border)
        e |= Qt::BottomEdge;
    return e;
}

Qt::Edges ExplorerWindow::edgesForPress(const QPoint &windowPos) const
{
    if (isMaximized() || isFullScreen())
        return {};
    return resizeEdgesAt(windowPos, size());
}

void ExplorerWindow::setResizeCursor(Qt::Edges edges)
{
    if (edges == m_cursorEdges)
        return;
    Qt::CursorShape shape = Qt::SizeVerCursor;
    if (edges == (Qt::LeftEdge | Qt::TopEdge) || edges == (Qt::RightEdge | Qt::BottomEdge))
        shape = Qt::SizeFDiagCursor;
    else if (edges == (Qt::RightEdge | Qt::TopEdge) || edges == (Qt::LeftEdge | Qt::BottomEdge))
        shape = Qt::SizeBDiagCursor;
    else if (edges & (Qt::LeftEdge | Qt::RightEdge))
        shape = Qt::SizeHorCursor;
    if (!edges)
        QGuiApplication::restoreOverrideCursor();
    else if (!m_cursorEdges)
        QGuiApplication::setOverrideCursor(shape);
    else
        QGuiApplication::changeOverrideCursor(shape);
    m_cursorEdges = edges;
}

bool ExplorerWindow::eventFilter(QObject *watched, QEvent *event)
{
    const QEvent::Type type = event->type();
    if (type != QEvent::MouseMove && type != QEvent::MouseButtonPress)
        return KMainWindow::eventFilter(watched, event);
    auto *widget = qobject_cast<QWidget *>(watched);
    auto *me = static_cast<QMouseEvent *>(event);
    const bool ours = widget && widget->window() == this;
    const Qt::Edges edges = ours ? edgesForPress(mapFromGlobal(me->globalPosition().toPoint())) : Qt::Edges();
    if (type == QEvent::MouseMove) {
        if (me->buttons() == Qt::NoButton || !ours) // a drag or rubber band keeps its cursor
            setResizeCursor(edges);
        return false;
    }
    if (edges && me->button() == Qt::LeftButton && windowHandle() && windowHandle()->startSystemResize(edges))
        return true; // offscreen / no support: the press goes on to the widget
    return false;
}

void ExplorerWindow::changeEvent(QEvent *e)
{
    KMainWindow::changeEvent(e);
    if (!m_titleBar)
        return;
    if (e->type() == QEvent::WindowStateChange) {
        const bool full = isMaximized() || isFullScreen();
        setContentsMargins(full ? QMargins() : QMargins(1, 1, 1, 1));
        m_titleBar->maximizeButton()->setIcon(QIcon::fromTheme(isMaximized() ? u"window-restore"_s : u"window-maximize"_s));
        if (full)
            setResizeCursor({});
    } else if (e->type() == QEvent::ActivationChange) {
        m_titleBar->setActive(isActiveWindow());
        update(); // the outline follows the title colour
    }
}

void ExplorerWindow::leaveEvent(QEvent *e)
{
    setResizeCursor({});
    KMainWindow::leaveEvent(e);
}

void ExplorerWindow::paintEvent(QPaintEvent *e)
{
    KMainWindow::paintEvent(e);
    if (isMaximized() || isFullScreen() || !m_titleBar)
        return;
    QPainter p(this);
    p.setPen(m_titleBar->background().darker(140));
    p.drawRect(rect().adjusted(0, 0, -1, -1));
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
            m_page->addAction(a);
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
    shortcut(QKeySequence(Qt::CTRL | Qt::Key_W), [this] { close(); });

    connect(m_actions, &ExplorerActions::newWindowRequested, this, [this] {
        (new ExplorerWindow(activeView() ? activeView()->url() : QUrl::fromLocalFile(QDir::homePath())))->show();
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
    connect(m_actions, &ExplorerActions::selectUrlsRequested, this, [this](const QList<QUrl> &urls) { selectItems(urls); });
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
        FolderPage *page = m_page;
        if (!page || page->isSplit() == on)
            return;
        page->setSplit(on);
        if (on)
            wireView(page->secondary());
        syncToActiveView();
    });
    connect(m_actions, &ExplorerActions::openInNewWindowRequested, this, [](const QUrl &url) { (new ExplorerWindow(url))->show(); });
    connect(m_actions, &ExplorerActions::openInSplitRequested, this, [this](const QUrl &url) {
        if (!m_page)
            return;
        if (!m_page->isSplit()) {
            m_page->setSplit(true);
            wireView(m_page->secondary());
            syncToActiveView();
        }
        m_page->secondary()->openUrl(url);
    });
}

void ExplorerWindow::wireView(FolderView *v)
{
    connect(v, &FolderView::urlChanged, this, [this, v](const QUrl &url) {
        if (v != activeView())
            return;
        addRecentFolder(m_config, url);
        m_tagList->clearPicks(); // real navigation drops the tag filter
        v->clearResults();       // also when the list had no picks 
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
    connect(v, &FolderView::folderMiddleClicked, this, [](const QUrl &url) { (new ExplorerWindow(url))->show(); });
    connect(v, &FolderView::fileActivated, this, [this](const KFileItem &item) {
        auto *job = new KIO::OpenUrlJob(item.url(), item.mimetype());
        job->setRunExecutables(false);
        job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, this));
        job->start();
    });
    connect(v, &FolderView::contextMenuRequested, this, [this, v](const QPoint &pos, const KFileItemList &items) {
        if (FolderPage *page = m_page)
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

FolderPage *ExplorerWindow::page() const
{
    return m_page;
}

FolderView *ExplorerWindow::activeView() const
{
    FolderPage *page = m_page;
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
    m_tagList->clearPicks(true); // the list just starts clean
    m_resultsTitle->setText(v->resultsTitle());
    m_resultsTitleAction->setVisible(!v->resultsTitle().isEmpty());
    const QUrl url = v->url();
    if (m_navigator->locationUrl() != url)
        m_navigator->setLocationUrl(url);
    m_places->setUrl(url);
    m_back->setEnabled(v->canGoBack());
    m_forward->setEnabled(v->canGoForward());
    m_actions->action(u"view_split"_s)->setChecked(m_page->isSplit());
    const QString title = m_page->title();
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

bool ExplorerWindow::queryClose()
{
    KConfigGroup session(m_config, u"Session"_s);
    session.writeEntry("Url", m_page->primary()->url().toString());
    if (m_page->isSplit())
        session.writeEntry("SecondUrl", m_page->secondary()->url().toString());
    else
        session.deleteEntry("SecondUrl");
    session.deleteEntry("Tabs");
    session.deleteEntry("Active");
    m_config->sync();
    return true;
}

QList<QUrl> ExplorerWindow::savedSession(const KSharedConfig::Ptr &config)
{
    QList<QUrl> urls;
    const KConfigGroup session(config, u"Session"_s);
    if (!session.hasKey("Url"))
        return urls;
    urls << QUrl(session.readEntry("Url", QString()));
    if (session.hasKey("SecondUrl"))
        urls << QUrl(session.readEntry("SecondUrl", QString()));
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

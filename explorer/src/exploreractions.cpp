#include "exploreractions.h"
#include "managetagsdialog.h"
#include <algorithm>
#include "folderview.h"
#include "ribbonspec.h"
#include "selectbyruledialog.h"
#include "tagribbon.h"
#include <KSharedConfig>
#include <KConfigGroup>
#include <QPainter>
#include <QPixmap>
#include <KFileItemActions>
#include <KFileItemListProperties>
#include <KFilePlacesModel>
#include <KIO/CommandLauncherJob>
#include <KIO/CopyJob>
#include <KIO/DeleteOrTrashJob>
#include <KIO/FileUndoManager>
#include <KIO/MkpathJob>
#include <KIO/Global>
#include <KIO/JobUiDelegate>
#include <KIO/JobUiDelegateFactory>
#include <KIO/OpenUrlJob>
#include <KIO/Paste>
#include <KIO/PasteJob>
#include <KJobWidgets>
#include <KLocalizedString>
#include <KMessageBox>
#include <KNewFileMenu>
#include <KPageWidget>
#include <KPageWidgetModel>
#include <KPropertiesDialog>
#include <KTerminalLauncherJob>
#include <KUrlMimeData>
#include <Purpose/AlternativesModel>
#include <Purpose/Menu>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFileDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QMenu>
#include <QMimeData>

namespace {
const QList<std::pair<const char *, ViewMode>> layoutIds = {
    {"view_layout_xlarge", ViewMode::ExtraLargeIcons}, {"view_layout_large", ViewMode::LargeIcons},
    {"view_layout_medium", ViewMode::MediumIcons},     {"view_layout_small", ViewMode::SmallIcons},
    {"view_layout_list", ViewMode::List},              {"view_layout_details", ViewMode::Details},
    {"view_layout_tiles", ViewMode::Tiles},            {"view_layout_content", ViewMode::Content},
};

// Ids that act on the selection, and ids that only need a folder to act in.
const QStringList selectionIds = {
    QStringLiteral("home_copy"), QStringLiteral("home_cut"), QStringLiteral("home_copy_path"), QStringLiteral("home_move_to"),
    QStringLiteral("home_copy_to"), QStringLiteral("home_delete"), QStringLiteral("home_delete_trash"),
    QStringLiteral("home_delete_permanent"), QStringLiteral("home_rename"), QStringLiteral("home_open"), QStringLiteral("home_edit"),
    QStringLiteral("share_share"), QStringLiteral("share_email"), QStringLiteral("share_zip"),
    QStringLiteral("tags_add"), QStringLiteral("tags_remove"),
};
const QStringList folderIds = {
    QStringLiteral("home_pin"), QStringLiteral("home_new_folder"), QStringLiteral("home_new_item"), QStringLiteral("home_properties"),
    QStringLiteral("home_select_all"), QStringLiteral("home_select_none"), QStringLiteral("home_select_invert"),
    QStringLiteral("share_security"),
};

// A menu without a widget parent dies with its owner (unless something else deleted it first).
QMenu *ownedMenu(QObject *owner, QMenu *menu)
{
    if (!menu->parent())
        QObject::connect(owner, &QObject::destroyed, menu, [menu] { delete menu; });
    return menu;
}

bool clipboardHasUrls()
{
    const QMimeData *mime = QApplication::clipboard()->mimeData();
    return mime && mime->hasUrls();
}

QStringList localPaths(const KFileItemList &items)
{
    QStringList paths;
    for (const KFileItem &item : items)
        if (!item.localPath().isEmpty())
            paths << item.localPath();
    return paths;
}

void startOpen(const QUrl &url, QWidget *window)
{
    auto *job = new KIO::OpenUrlJob(url);
    job->setRunExecutables(false);
    job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, window));
    job->start();
}
}

ExplorerActions::ExplorerActions(QWidget *windowParent)
    : QObject(windowParent)
    , m_window(windowParent)
    , m_registry(this)
    , m_newFileMenu(new KNewFileMenu(this))
    , m_shareMenu(new Purpose::Menu(windowParent))
{
    ownedMenu(this, m_shareMenu);
    if (m_window)
        m_newFileMenu->setParentWidget(m_window);
    for (const QString &id : Ribbon::actionIds()) {
        const Ribbon::Button *b = Ribbon::button(id);
        QAction *a = add(id, b->text, b->icon);
        if (id == QLatin1String("home_new_item"))
            a->setMenu(m_newFileMenu->menu());
        else if (id == QLatin1String("share_share"))
            a->setMenu(m_shareMenu);
        else if (b->menu)
            a->setMenu(ownedMenu(this, new QMenu(m_window)));
    }
    QMenu *deleteMenu = action(QStringLiteral("home_delete"))->menu();
    deleteMenu->addAction(add(QStringLiteral("home_delete_trash"), i18n("Move to Trash"), QStringLiteral("user-trash")));
    deleteMenu->addAction(add(QStringLiteral("home_delete_permanent"), i18n("Delete permanently"), QStringLiteral("edit-delete")));

    const QList<std::pair<const char *, QKeySequence>> shortcuts = {
        {"home_copy", QKeySequence::Copy},
        {"home_cut", QKeySequence::Cut},
        {"home_paste", QKeySequence::Paste},
        {"home_undo", QKeySequence::Undo},
        {"home_redo", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z)},
        {"home_rename", QKeySequence(Qt::Key_F2)},
        {"home_delete_trash", QKeySequence(Qt::Key_Delete)},
        {"home_delete_permanent", QKeySequence(Qt::SHIFT | Qt::Key_Delete)},
        {"home_select_all", QKeySequence(Qt::CTRL | Qt::Key_A)},
        {"home_new_folder", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N)},
        {"home_properties", QKeySequence(Qt::ALT | Qt::Key_Return)},
        {"view_hidden", QKeySequence(Qt::CTRL | Qt::Key_H)},
        {"view_split", QKeySequence(Qt::Key_F4)},
        {"view_nav_pane", QKeySequence(Qt::Key_F11)},
    };
    for (const auto &[id, keys] : shortcuts)
        action(QString::fromLatin1(id))->setShortcut(keys);

    buildViewMenus();
    connectActions();
    connect(QApplication::clipboard(), &QClipboard::dataChanged, this, &ExplorerActions::updateEnabled);
    auto *undo = KIO::FileUndoManager::self();
    connect(undo, &KIO::FileUndoManager::undoAvailable, this, &ExplorerActions::updateEnabled);
    connect(undo, &KIO::FileUndoManager::redoAvailable, this, &ExplorerActions::updateEnabled);
    updateEnabled();
}

QAction *ExplorerActions::add(const QString &id, const QString &text, const QString &icon)
{
    auto *a = new QAction(QIcon::fromTheme(icon), text, this);
    a->setObjectName(id);
    m_actions.insert(id, a);
    return a;
}

QAction *ExplorerActions::action(const QString &id) const
{
    return m_actions.value(id);
}

QMenu *ExplorerActions::newItemMenu() const
{
    return m_newFileMenu->menu();
}

QMenu *ExplorerActions::historyMenu() const
{
    return action(QStringLiteral("home_history"))->menu();
}

void ExplorerActions::setActiveView(FolderView *view)
{
    if (m_view)
        disconnect(m_view, nullptr, this, nullptr);
    m_view = view;
    if (view) {
        connect(view, &FolderView::selectionChanged, this, &ExplorerActions::updateEnabled);
        connect(view, &FolderView::urlChanged, this, [this](const QUrl &url) {
            m_newFileMenu->setWorkingDirectory(url);
            updateEnabled();
            syncViewState();
        });
        // ~QWidget emits destroyed before the QPointer clears: drop it here so nothing touches a dying view.
        connect(view, &QObject::destroyed, this, [this] {
            m_view = nullptr;
            updateEnabled();
        });
        m_newFileMenu->setWorkingDirectory(view->url());
    }
    updateEnabled();
    syncViewState();
}

KFileItemList ExplorerActions::selection() const
{
    return m_view ? m_view->selectedItems() : KFileItemList();
}

QUrl ExplorerActions::folder() const
{
    return m_view ? m_view->url() : QUrl();
}

void ExplorerActions::updateEnabled()
{
    const bool hasView = m_view && m_view->url().isValid();
    const KFileItemList sel = selection();
    for (const QString &id : selectionIds)
        action(id)->setEnabled(hasView && !sel.isEmpty());
    for (const QString &id : folderIds)
        action(id)->setEnabled(hasView);
    action(QStringLiteral("adv_rename_undo"))->setEnabled(!m_lastBatch.isEmpty());
    action(QStringLiteral("adv_rename_many"))->setEnabled(hasView);
    for (const char *id : {"adv_select_rule", "adv_select_matches", "adv_copy_matches", "adv_move_matches", "adv_rename_matches", "adv_trash_matches"})
        action(QString::fromLatin1(id))->setEnabled(hasView);
    const bool canPaste = hasView && clipboardHasUrls();
    action(QStringLiteral("home_paste"))->setEnabled(canPaste);
    action(QStringLiteral("home_paste_shortcut"))->setEnabled(canPaste);
    const bool localFiles = !sel.isEmpty() && std::all_of(sel.begin(), sel.end(), [](const KFileItem &i) {
        return !i.isDir() && !i.localPath().isEmpty();
    });
    action(QStringLiteral("share_print"))->setEnabled(hasView && localFiles);
    const bool localFolder = hasView && folder().isLocalFile();
    action(QStringLiteral("share_terminal"))->setEnabled(localFolder);
    action(QStringLiteral("file_terminal"))->setEnabled(localFolder);

    auto *undo = KIO::FileUndoManager::self();
    action(QStringLiteral("home_undo"))->setEnabled(undo->isUndoAvailable());
    action(QStringLiteral("home_undo"))->setToolTip(undo->undoText());
    action(QStringLiteral("home_redo"))->setEnabled(undo->isRedoAvailable());
    action(QStringLiteral("home_redo"))->setToolTip(undo->redoText());
}

void ExplorerActions::syncViewState()
{
    if (!m_view)
        return;
    const ViewSettings s = m_view->settings();
    for (const auto &[id, mode] : layoutIds)
        action(QString::fromLatin1(id))->setChecked(s.mode == mode);
    action(QStringLiteral("view_checkboxes"))->setChecked(s.checkBoxes);
    action(QStringLiteral("view_extensions"))->setChecked(s.showExtensions);
    action(QStringLiteral("view_hidden"))->setChecked(s.hiddenFiles);
    action(QStringLiteral("tags_group_name"))->setChecked(s.groupRole == GroupRole::TagName);
    action(QStringLiteral("tags_group_colour"))->setChecked(s.groupRole == GroupRole::TagColour);
    action(QStringLiteral("tags_flags"))->setChecked(FolderView::showFlags());
}

void ExplorerActions::buildViewMenus()
{
    auto *layouts = new QActionGroup(this);
    for (const auto &[id, mode] : layoutIds) {
        QAction *a = action(QString::fromLatin1(id));
        a->setCheckable(true);
        layouts->addAction(a);
        connect(a, &QAction::triggered, this, [this, mode] { Q_EMIT layoutRequested(mode); });
    }
    const auto current = [this] { return m_view ? m_view->settings() : ViewSettings(); };

    QMenu *sort = action(QStringLiteral("view_sort"))->menu();
    const QList<std::pair<QString, SortRole>> sortRoles = {
        {i18n("Name"), SortRole::Name}, {i18n("Date modified"), SortRole::Modified}, {i18n("Type"), SortRole::Type}, {i18n("Size"), SortRole::Size}};
    auto *roleGroup = new QActionGroup(sort);
    for (const auto &[text, role] : sortRoles) {
        QAction *a = sort->addAction(text);
        a->setCheckable(true);
        a->setData(int(role));
        roleGroup->addAction(a);
        connect(a, &QAction::triggered, this, [this, role, current] { Q_EMIT sortRequested(role, current().sortOrder); });
    }
    sort->addSeparator();
    auto *orderGroup = new QActionGroup(sort);
    for (const auto &[text, order] : {std::pair{i18n("Ascending"), Qt::AscendingOrder}, std::pair{i18n("Descending"), Qt::DescendingOrder}}) {
        QAction *a = sort->addAction(text);
        a->setCheckable(true);
        a->setData(int(order) + 100);
        orderGroup->addAction(a);
        connect(a, &QAction::triggered, this, [this, order, current] { Q_EMIT sortRequested(current().sortRole, order); });
    }
    connect(sort, &QMenu::aboutToShow, this, [sort, current] {
        const ViewSettings s = current();
        for (QAction *a : sort->actions())
            if (!a->isSeparator())
                a->setChecked(a->data().toInt() == (a->data().toInt() >= 100 ? int(s.sortOrder) + 100 : int(s.sortRole)));
    });

    QMenu *group = action(QStringLiteral("view_group"))->menu();
    const QList<std::pair<QString, GroupRole>> groupRoles = {{i18n("(None)"), GroupRole::None}, {i18n("Name"), GroupRole::Name},
                                                             {i18n("Date modified"), GroupRole::Modified}, {i18n("Type"), GroupRole::Type},
                                                             {i18n("Size"), GroupRole::Size},     {i18n("Tag name"), GroupRole::TagName},
                                                             {i18n("Tag colour"), GroupRole::TagColour}};
    auto *groupGroup = new QActionGroup(group);
    for (const auto &[text, role] : groupRoles) {
        QAction *a = group->addAction(text);
        a->setCheckable(true);
        a->setData(int(role));
        groupGroup->addAction(a);
        connect(a, &QAction::triggered, this, [this, role] { Q_EMIT groupRequested(role); });
    }
    connect(group, &QMenu::aboutToShow, this, [group, current] {
        for (QAction *a : group->actions())
            a->setChecked(a->data().toInt() == int(current().groupRole));
    });

    QMenu *columns = action(QStringLiteral("view_columns"))->menu();
    for (const QString &id : knownColumnIds()) {
        QAction *a = columns->addAction(columnTitle(id));
        a->setCheckable(true);
        a->setData(id);
        a->setEnabled(id != QLatin1String("name")); // the name column always stays
        connect(a, &QAction::triggered, this, [this, id](bool on) { Q_EMIT columnsToggled(id, on); });
    }
    connect(columns, &QMenu::aboutToShow, this, [columns, current] {
        const QStringList shown = current().columns;
        for (QAction *a : columns->actions())
            a->setChecked(shown.contains(a->data().toString()));
    });

    const QList<std::pair<const char *, QString>> panes = {{"view_nav_pane", QStringLiteral("nav")},
                                                          {"view_preview_pane", QStringLiteral("preview")},
                                                          {"view_details_pane", QStringLiteral("details")}};
    for (const auto &[id, pane] : panes) {
        QAction *a = action(QString::fromLatin1(id));
        a->setCheckable(true);
        connect(a, &QAction::triggered, this, [this, pane](bool on) { Q_EMIT paneToggled(pane, on); });
    }
    action(QStringLiteral("view_nav_pane"))->setChecked(true);
    const QList<std::pair<const char *, QString>> shows = {{"view_checkboxes", QStringLiteral("checkboxes")},
                                                          {"view_extensions", QStringLiteral("extensions")},
                                                          {"view_hidden", QStringLiteral("hidden")}};
    for (const auto &[id, what] : shows) {
        QAction *a = action(QString::fromLatin1(id));
        a->setCheckable(true);
        connect(a, &QAction::triggered, this, [this, what](bool on) { Q_EMIT showToggled(what, on); });
    }
    action(QStringLiteral("view_extensions"))->setChecked(true);
    action(QStringLiteral("view_split"))->setCheckable(true);
    connect(action(QStringLiteral("view_split")), &QAction::triggered, this, &ExplorerActions::splitToggled);

    const QList<std::pair<const char *, GroupRole>> tagGroups = {{"tags_group_name", GroupRole::TagName}, {"tags_group_colour", GroupRole::TagColour}};
    for (const auto &[id, role] : tagGroups) {
        QAction *a = action(QString::fromLatin1(id));
        a->setCheckable(true);
        connect(a, &QAction::triggered, this, [this, role] { Q_EMIT groupRequested(role); });
    }
    QAction *flags = action(QStringLiteral("tags_flags"));
    flags->setCheckable(true);
    flags->setChecked(FolderView::showFlags());
    connect(flags, &QAction::triggered, this, [](bool on) {
        FolderView::setShowFlags(on);
        KConfigGroup(KSharedConfig::openConfig(QStringLiteral("win12-explorerrc")), QStringLiteral("Tags")).writeEntry("showFlags", on, KConfigGroup::Notify);
        KSharedConfig::openConfig(QStringLiteral("win12-explorerrc"))->sync();
    });
    connect(action(QStringLiteral("tags_manage")), &QAction::triggered, this, &ExplorerActions::manageTagsRequested);
    connect(this, &ExplorerActions::manageTagsRequested, this, [this] {
        ManageTagsDialog dialog(&m_registry, &m_tagStore, m_view ? m_view->url() : QUrl::fromLocalFile(QDir::homePath()), m_window);
        dialog.exec();
    });
    connect(&m_tagStore, &TagStore::tagsChanged, this, [this](const QList<QUrl> &urls) {
        if (m_view)
            m_view->model()->invalidateTags(urls);
    });
    QMenu *add = action(QStringLiteral("tags_add"))->menu();
    connect(add, &QMenu::aboutToShow, this, [this, add] { fillTagsMenu(add, selection()); });
    QMenu *remove = action(QStringLiteral("tags_remove"))->menu();
    connect(remove, &QMenu::aboutToShow, this, [this, remove] { fillRemoveMenu(remove, selection()); });
}

void ExplorerActions::connectActions()
{
    const auto on = [this](const char *id, auto fn) {
        connect(action(QString::fromLatin1(id)), &QAction::triggered, this, fn);
    };
    on("file_new_window", [this] { Q_EMIT newWindowRequested(); });
    on("file_close", [this] { Q_EMIT closeRequested(); });
    on("file_terminal", [this] { openTerminal(folder()); });
    on("share_terminal", [this] { openTerminal(folder()); });
    on("file_options", [this] {
        launch(QStringLiteral("win12-settings"), {});
        Q_EMIT optionsRequested();
    });
    on("file_help", [this] {
        startOpen(QUrl(QStringLiteral("https://github.com/Kalaminator/Win12")), m_window);
        Q_EMIT helpRequested();
    });

    on("home_pin", [this] {
        const KFileItemList sel = selection();
        const QUrl url = sel.size() == 1 && sel.first().isDir() ? sel.first().url() : folder();
        if (url.isValid())
            KFilePlacesModel().addPlace(url.fileName().isEmpty() ? url.toDisplayString(QUrl::PreferLocalFile) : url.fileName(), url);
    });
    on("home_copy", [this] { copyToClipboard(selection(), false); });
    on("home_cut", [this] { copyToClipboard(selection(), true); });
    on("home_paste", [this] { paste(folder()); });
    on("home_paste_shortcut", [this] { pasteShortcut(folder()); });
    on("home_copy_path", [this] { copyPaths(selection()); });
    on("home_undo", [this] {
        KIO::FileUndoManager::self()->uiInterface()->setParentWidget(m_window);
        KIO::FileUndoManager::self()->undo();
    });
    on("home_redo", [this] {
        KIO::FileUndoManager::self()->uiInterface()->setParentWidget(m_window);
        KIO::FileUndoManager::self()->redo();
    });
    using A = SelectByRuleDialog::Action;
    on("adv_select_rule", [this] { selectByRule(A::Trash); });
    on("adv_select_matches", [this] { selectByRule(A::Select); });
    on("adv_copy_matches", [this] { selectByRule(A::CopyTo); });
    on("adv_move_matches", [this] { selectByRule(A::MoveTo); });
    on("adv_rename_many", [this] {
        if (m_view)
            renameMany(m_view->itemsInViewOrder(m_view->selectedCount() > 0));
    });
    on("adv_rename_undo", [this] { undoRenameBatch(); });
    on("adv_rename_matches", [this] { selectByRule(A::Rename); });
    on("adv_tag_matches", [this] { selectByRule(A::AddTag); });
    on("adv_trash_matches", [this] { selectByRule(A::Trash); });
    on("home_move_to", [this] { transferTo(selection(), true); });
    on("home_copy_to", [this] { transferTo(selection(), false); });
    on("home_delete", [this] { trash(selection()); }); // the big button is the safe default
    on("home_delete_trash", [this] { trash(selection()); });
    on("home_delete_permanent", [this] { deletePermanently(selection()); });
    on("home_rename", [this] {
        if (m_view)
            m_view->renameCurrent();
    });
    on("home_new_folder", [this] {
        if (m_view)
            m_newFileMenu->createDirectory();
    });
    on("home_properties", [this] { properties(selection(), folder()); });
    on("home_open", [this] { open(selection()); });
    on("home_edit", [this] { open(selection()); });
    on("home_select_all", [this] {
        if (m_view)
            m_view->selectAll();
    });
    on("home_select_none", [this] {
        if (m_view)
            m_view->selectNone();
    });
    on("home_select_invert", [this] {
        if (m_view)
            m_view->invertSelection();
    });

    QMenu *openMenu = action(QStringLiteral("home_open"))->menu();
    connect(openMenu, &QMenu::aboutToShow, this, [this, openMenu] {
        openMenu->clear();
        qDeleteAll(openMenu->findChildren<KFileItemActions *>(Qt::FindDirectChildrenOnly));
        const KFileItemList sel = selection();
        if (sel.isEmpty())
            return;
        auto *fia = new KFileItemActions(openMenu);
        fia->setParentWidget(m_window);
        fia->setItemListProperties(KFileItemListProperties(sel));
        fia->insertOpenWithActionsTo(nullptr, openMenu, {});
    });

    connect(m_shareMenu, &QMenu::aboutToShow, this, [this] {
        const KFileItemList sel = selection();
        QJsonArray urls;
        for (const KFileItem &item : sel)
            urls.append(item.url().toString());
        m_shareMenu->model()->setInputData(QJsonObject{{QStringLiteral("urls"), urls},
                                                       {QStringLiteral("mimeType"), sel.isEmpty() ? QString() : sel.first().mimetype()}});
        m_shareMenu->model()->setPluginType(QStringLiteral("Export"));
        m_shareMenu->reload();
    });
    on("share_email", [this] {
        QStringList args;
        for (const QString &path : localPaths(selection()))
            args << QStringLiteral("--attach") << path;
        launch(QStringLiteral("xdg-email"), args);
    });
    on("share_zip", [this] { zip(selection()); });
    on("share_print", [this] { launch(QStringLiteral("lp"), localPaths(selection())); });
    on("share_security", [this] {
        const KFileItemList sel = selection();
        auto *dialog = sel.isEmpty() ? new KPropertiesDialog(folder(), m_window) : new KPropertiesDialog(sel, m_window);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        // The page title is KIO's string, so compare against KIO's catalog, not ours.
        const QString permissions = i18nd("kio6", "Permissions");
        if (auto *pages = dialog->findChild<KPageWidget *>()) {
            if (auto *model = qobject_cast<KPageWidgetModel *>(pages->model())) {
                KPageWidgetItem *target = nullptr;
                for (int row = 0; row < model->rowCount(); ++row) {
                    KPageWidgetItem *item = model->item(model->index(row, 0));
                    if (!target || KLocalizedString::removeAcceleratorMarker(item->name()) == permissions)
                        target = item;
                    if (KLocalizedString::removeAcceleratorMarker(item->name()) == permissions)
                        break;
                }
                if (target)
                    dialog->setCurrentPage(target);
            }
        }
        dialog->show();
    });
}

void ExplorerActions::copyToClipboard(const KFileItemList &items, bool cut)
{
    if (items.isEmpty())
        return;
    auto *mime = new QMimeData;
    QList<QUrl> mostLocal;
    for (const KFileItem &item : items)
        mostLocal << item.mostLocalUrl();
    KUrlMimeData::setUrls(items.urlList(), mostLocal, mime);
    KIO::setClipboardDataCut(mime, cut);
    QApplication::clipboard()->setMimeData(mime);
}

void ExplorerActions::paste(const QUrl &folder)
{
    const QMimeData *mime = QApplication::clipboard()->mimeData();
    if (!folder.isValid() || !mime || !mime->hasUrls())
        return;
    KIO::PasteJob *job = KIO::paste(mime, folder);
    KJobWidgets::setWindow(job, m_window);
    connect(job, &KIO::PasteJob::copyJobStarted, this, [](KIO::CopyJob *copy) {
        KIO::FileUndoManager::self()->recordCopyJob(copy);
    });
}

void ExplorerActions::pasteShortcut(const QUrl &folder)
{
    const QMimeData *mime = QApplication::clipboard()->mimeData();
    if (!folder.isValid() || !mime || !mime->hasUrls())
        return;
    KIO::CopyJob *job = KIO::link(mime->urls(), folder);
    KJobWidgets::setWindow(job, m_window);
    KIO::FileUndoManager::self()->recordCopyJob(job);
}

void ExplorerActions::transferTo(const KFileItemList &items, bool move, const QUrl &keepStructureFrom)
{
    if (items.isEmpty())
        return;
    const QString title = move ? i18n("Move to") : i18n("Copy to");
    const QUrl dest = m_pickFolder ? m_pickFolder(folder()) : QFileDialog::getExistingDirectoryUrl(m_window, title, folder());
    if (!dest.isValid())
        return;
    const auto run = [this, move](const QList<QUrl> &urls, const QUrl &to) {
        KIO::CopyJob *job = move ? KIO::move(urls, to) : KIO::copy(urls, to);
        KJobWidgets::setWindow(job, m_window);
        KIO::FileUndoManager::self()->recordCopyJob(job);
    };
    if (!keepStructureFrom.isValid()) {
        run(items.urlList(), dest);
        return;
    }
    // One mkpath + copy/move per distinct relative subfolder (ruling 3), each recorded for Undo.
    QMap<QString, QList<QUrl>> groups;
    for (const KFileItem &item : items) {
        QString rel = QDir(QDir::cleanPath(keepStructureFrom.path())).relativeFilePath(KIO::upUrl(item.url()).path());
        if (rel == QLatin1String(".") || rel.startsWith(QLatin1String("..")))
            rel.clear();
        groups[rel] << item.url();
    }
    for (auto it = groups.cbegin(); it != groups.cend(); ++it) {
        if (it.key().isEmpty()) {
            run(it.value(), dest);
            continue;
        }
        QUrl sub = dest;
        sub.setPath(QDir::cleanPath(dest.path() + QLatin1Char('/') + it.key()));
        KIO::Job *mk = KIO::mkpath(sub, dest);
        KJobWidgets::setWindow(mk, m_window);
        const QList<QUrl> urls = it.value();
        connect(mk, &KJob::result, this, [run, urls, sub](KJob *j) {
            if (!j->error())
                run(urls, sub);
        });
    }
}

void ExplorerActions::zip(const KFileItemList &items)
{
    launch(QStringLiteral("ark"),
           QStringList{QStringLiteral("--add"), QStringLiteral("--dialog"), QStringLiteral("--changetofirstpath")} + localPaths(items));
}

void ExplorerActions::copyPaths(const KFileItemList &items)
{
    QStringList paths;
    for (const KFileItem &item : items)
        paths << (item.localPath().isEmpty() ? item.url().toDisplayString() : item.localPath());
    QApplication::clipboard()->setText(paths.join(QLatin1Char('\n')));
}

KFileItemList ExplorerActions::withoutNested(const KFileItemList &items)
{
    QStringList dirs;
    for (const KFileItem &i : items)
        if (i.isDir())
            dirs << QDir::cleanPath(i.url().path()) + QLatin1Char('/');
    KFileItemList out;
    for (const KFileItem &i : items) {
        const QString p = QDir::cleanPath(i.url().path());
        const bool nested = std::any_of(dirs.cbegin(), dirs.cend(), [&](const QString &d) { return p.startsWith(d); });
        if (!nested)
            out << i;
    }
    return out;
}

void ExplorerActions::selectByRule(SelectByRuleDialog::Action defaultAction)
{
    if (!folder().isValid())
        return;
    SelectByRuleDialog dialog(folder(), defaultAction, m_window);
    if (dialog.exec() != QDialog::Accepted)
        return;
    KFileItemList items = dialog.ticked();
    if (items.isEmpty())
        return;
    using A = SelectByRuleDialog::Action;
    const A chosen = dialog.chosenAction();
    if (chosen != A::Select && chosen != A::Rename && chosen != A::CopyPaths && chosen != A::AddTag)
        items = withoutNested(items); // a ticked folder already carries what is inside it
    switch (chosen) {
    case A::Select: Q_EMIT selectUrlsRequested(items.urlList()); break;
    case A::CopyTo: transferTo(items, false, dialog.keepStructure() ? dialog.lookIn() : QUrl()); break;
    case A::MoveTo: transferTo(items, true, dialog.keepStructure() ? dialog.lookIn() : QUrl()); break;
    case A::Rename: renameMany(items); break;
    case A::Trash: trash(items); break;
    case A::DeleteForever: deletePermanently(items); break;
    case A::Zip: zip(items); break;
    case A::CopyPaths: copyPaths(items); break;
    case A::AddTag: newTag(items); break;
    }
}

void ExplorerActions::trash(const KFileItemList &items)
{
    if (items.isEmpty())
        return;
    KIO::Job *job = KIO::trash(items.urlList());
    KJobWidgets::setWindow(job, m_window);
    KIO::FileUndoManager::self()->recordJob(KIO::FileUndoManager::Trash, items.urlList(), QUrl(QStringLiteral("trash:/")), job);
}

void ExplorerActions::deletePermanently(const KFileItemList &items)
{
    if (items.isEmpty())
        return;
    // Asks first (always, whatever the user's "don't ask again" setting), then runs KIO::del.
    auto *job = new KIO::DeleteOrTrashJob(items.urlList(), KIO::AskUserActionInterface::Delete,
                                          KIO::AskUserActionInterface::ForceConfirmation, m_window);
    job->start();
}

void ExplorerActions::open(const KFileItemList &items)
{
    bool navigated = false;
    for (const KFileItem &item : items) {
        if (item.isDir() && m_view) {
            if (!navigated) // ponytail: the first folder navigates; more selected folders are not opened
                m_view->openUrl(item.url());
            navigated = true;
        } else {
            startOpen(item.url(), m_window);
        }
    }
}

void ExplorerActions::properties(const KFileItemList &items, const QUrl &folder)
{
    if (!items.isEmpty())
        KPropertiesDialog::showDialog(items, m_window, false);
    else if (folder.isValid())
        KPropertiesDialog::showDialog(folder, m_window, false);
}

void ExplorerActions::launch(const QString &exe, const QStringList &args)
{
    auto *job = new KIO::CommandLauncherJob(exe, args);
    job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, m_window));
    job->start();
}

void ExplorerActions::openTerminal(const QUrl &folder)
{
    if (!folder.isLocalFile())
        return;
    auto *job = new KTerminalLauncherJob(QString());
    job->setWorkingDirectory(folder.toLocalFile());
    job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, m_window));
    job->start();
}

QString ExplorerActions::runRenames(const QList<PlannedRename> &rows, QList<PlannedRename> *executed)
{
    // ponytail: sequential sync moveAs jobs; a progress dialog if 1000-file batches turn out slow on /mnt/hdd
    for (const PlannedRename &r : rows) {
        if (r.status != PlannedRename::Rename)
            continue;
        // rename() also records a FileUndoManager entry per file; acceptable, the batch undo is the ribbon button
        const QString error = rename(KFileItem(r.url), r.newName, m_window);
        if (!error.isEmpty())
            return error;
        if (executed)
            *executed << r;
    }
    return QString();
}

void ExplorerActions::renameMany(const KFileItemList &items)
{
    if (items.isEmpty())
        return;
    QList<RenameItem> ritems;
    QSet<QString> existing;
    QSet<QString> dirs;
    for (const KFileItem &i : items) {
        ritems << RenameItem{i.url(), i.name(), i.time(KFileItem::ModificationTime), i};
        const QString dir = i.url().adjusted(QUrl::RemoveFilename).toLocalFile();
        if (!dir.isEmpty() && !dirs.contains(dir)) {
            dirs << dir;
            if (m_view && folder().isLocalFile() && QDir::cleanPath(dir) == QDir::cleanPath(folder().toLocalFile())) {
                // the active view's folder: names come from its model, no disk access
                for (const KFileItem &v : m_view->itemsInViewOrder(false))
                    existing << v.name();
                continue;
            }
            // ponytail: other folders (rule matches in subfolders) are read once each, KIO just listed them (warm cache)
            const QStringList names = QDir(dir).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot);
            existing.unite(QSet<QString>(names.begin(), names.end()));
        }
    }
    RenameManyDialog dialog(ritems, existing, m_window);
    if (!m_execRenameDialog(dialog))
        return;
    QList<PlannedRename> executed;
    const QString error = runRenames(dialog.plan(), &executed);
    m_lastBatch = executed; // what ran stays undoable even after an error
    updateEnabled();
    if (!error.isEmpty())
        KMessageBox::error(m_window, error);
}

void ExplorerActions::undoRenameBatch()
{
    QString error;
    int done = 0;
    for (const auto &[newUrl, oldName] : RenamePlan::undoPlan(m_lastBatch)) {
        error = rename(KFileItem(newUrl), oldName, m_window);
        if (!error.isEmpty())
            break;
        ++done;
    }
    // undoPlan walks from the back: the reversed rows are the tail; the rest stay undoable
    m_lastBatch = m_lastBatch.mid(0, m_lastBatch.size() - done);
    updateEnabled();
    if (!error.isEmpty())
        KMessageBox::error(m_window, error);
}

QString ExplorerActions::rename(const KFileItem &item, const QString &newName, QWidget *parent)
{
    if (item.isNull() || newName.isEmpty() || newName.contains(QLatin1Char('/')) || newName == QLatin1String(".")
        || newName == QLatin1String(".."))
        return i18n("\"%1\" is not a valid name.", newName);
    if (newName == item.name())
        return QString();
    const QUrl src = item.url();
    QUrl target = src.adjusted(QUrl::RemoveFilename);
    target.setPath(target.path() + newName); // setPath, not resolved(): names may hold '#' or '?'
    KIO::CopyJob *job = KIO::moveAs(src, target, KIO::HideProgressInfo);
    // No ask-user delegate: an existing target fails with ERR_FILE_ALREADY_EXIST instead of a rename/overwrite dialog.
    job->setUiDelegate(nullptr);
    KJobWidgets::setWindow(job, parent);
    KIO::FileUndoManager::self()->recordJob(KIO::FileUndoManager::Rename, {src}, target, job);
    if (job->exec())
        return QString();
    if (job->error() == KIO::ERR_FILE_ALREADY_EXIST || job->error() == KIO::ERR_DIR_ALREADY_EXIST)
        return i18n("A file named %1 already exists.", newName);
    return job->errorString();
}

void ExplorerActions::addTag(const KFileItemList &items, const Tag &tag)
{
    QList<QUrl> urls;
    for (const KFileItem &i : items)
        urls << i.url();
    const TagStore::Error e = m_tagStore.add(urls, tag);
    if (e != TagStore::Error::None) {
        const QString text = TagStore::errorText(e);
        if (m_showError)
            m_showError(text);
        else
            KMessageBox::error(m_window, text);
    } else if (!m_registry.touch(tag)) {
        if (m_showError)
            m_showError(m_registry.saveErrorText());
        else
            KMessageBox::error(m_window, m_registry.saveErrorText());
    }
    if (m_view) // earlier items may be tagged even after an error
        m_view->model()->invalidateTags(urls);
}

void ExplorerActions::removeTag(const KFileItemList &items, const Tag &tag)
{
    QList<QUrl> urls;
    for (const KFileItem &i : items)
        urls << i.url();
    const TagStore::Error e = m_tagStore.remove(urls, tag);
    if (e != TagStore::Error::None) {
        if (m_showError)
            m_showError(TagStore::errorText(e));
        else
            KMessageBox::error(m_window, TagStore::errorText(e));
    }
    if (m_view)
        m_view->model()->invalidateTags(urls);
}

void ExplorerActions::newTag(const KFileItemList &items)
{
    NewTagDialog dialog(m_window);
    if (m_execNewTagDialog(dialog))
        addTag(items, dialog.tag());
}

QMenu *ExplorerActions::tagsMenu(const KFileItemList &items)
{
    auto *menu = new QMenu(i18n("Tags"));
    fillTagsMenu(menu, items);
    return menu;
}

void ExplorerActions::fillTagsMenu(QMenu *menu, const KFileItemList &items)
{
    menu->clear();
    const bool supported = !items.isEmpty() && std::all_of(items.begin(), items.end(), [](const KFileItem &i) { return TagStore::supported(i.url()); });
    if (!supported) {
        menu->addAction(TagStore::errorText(TagStore::Error::Unsupported))->setEnabled(false);
        return;
    }
    QList<QList<Tag>> have;
    bool full = true; // every item already at MaxTags: only removing is left
    for (const KFileItem &i : items) {
        have << TagStore::read(i.url());
        full = full && win12TagCount(have.last()) >= MaxTags;
    }
    const QList<Tag> known = m_registry.tags();
    for (const Tag &t : known.mid(0, 8)) {
        QAction *a = menu->addAction(TagRibbon::icon(t), t.display());
        a->setCheckable(true);
        a->setChecked(std::all_of(have.begin(), have.end(), [&](const QList<Tag> &h) { return h.contains(t); }));
        if (full && !a->isChecked()) {
            a->setEnabled(false);
            a->setToolTip(i18n("max 3 tags"));
        }
        connect(a, &QAction::triggered, this, [this, items, t](bool on) {
            if (on)
                addTag(items, t);
            else
                removeTag(items, t);
        });
    }
    menu->addSeparator();
    QAction *newTagAction = menu->addAction(QIcon::fromTheme(QStringLiteral("tag-new")), i18n("New tag…"), this, [this, items] { newTag(items); });
    if (full) {
        newTagAction->setEnabled(false);
        newTagAction->setText(i18n("New tag… (max 3 tags)"));
    }
    menu->addAction(QIcon::fromTheme(QStringLiteral("tag-properties")), i18n("Manage tags…"), this, [this] { Q_EMIT manageTagsRequested(); });
}

void ExplorerActions::fillRemoveMenu(QMenu *menu, const KFileItemList &items)
{
    menu->clear();
    if (!items.isEmpty() && !std::all_of(items.begin(), items.end(), [](const KFileItem &i) { return TagStore::supported(i.url()); })) {
        menu->addAction(TagStore::errorText(TagStore::Error::Unsupported))->setEnabled(false);
        return;
    }
    QList<Tag> present;
    for (const KFileItem &i : items)
        for (const Tag &t : TagStore::read(i.url()))
            if (t.colour >= 0 && !present.contains(t))
                present << t;
    for (const Tag &t : present)
        menu->addAction(TagRibbon::icon(t), t.display(), this, [this, items, t] { removeTag(items, t); });
    if (present.isEmpty())
        return;
    menu->addSeparator();
    menu->addAction(i18n("All tags"), this, [this, items, present] {
        for (const Tag &t : present)
            removeTag(items, t);
    });
}

QMenu *ExplorerActions::contextMenu(const KFileItemList &items, const QUrl &folder)
{
    auto *menu = new QMenu(m_window);
    const auto entry = [&](const QString &id, auto fn, bool enabled = true) {
        QAction *src = action(id);
        QAction *a = menu->addAction(src->icon(), src->text());
        a->setEnabled(enabled);
        connect(a, &QAction::triggered, this, fn);
        return a;
    };
    const bool canPaste = folder.isValid() && clipboardHasUrls();
    const auto addWin12 = [&] {
        auto *win12 = new QMenu(i18n("Win12"), menu);
        win12->addAction(i18n("Select by rule…"), this, [this] { selectByRule(SelectByRuleDialog::Action::Trash); });
        if (!items.isEmpty())
            win12->addAction(i18n("Rename many…"), this, [this, items] { renameMany(items); });
        Q_EMIT fillWin12Menu(win12, items);
        if (win12->isEmpty())
            delete win12;
        else
            menu->addMenu(win12);
    };

    if (!items.isEmpty()) {
        QAction *open = menu->addAction(QIcon::fromTheme(QStringLiteral("document-open")), i18n("Open"));
        connect(open, &QAction::triggered, this, [this, items] { this->open(items); });
        menu->setDefaultAction(open);
        auto *fia = new KFileItemActions(menu);
        fia->setParentWidget(m_window);
        fia->setItemListProperties(KFileItemListProperties(items));
        fia->insertOpenWithActionsTo(nullptr, menu, {});
        menu->addSeparator();
        entry(QStringLiteral("home_cut"), [this, items] { copyToClipboard(items, true); });
        entry(QStringLiteral("home_copy"), [this, items] { copyToClipboard(items, false); });
        entry(QStringLiteral("home_paste_shortcut"), [this, folder] { pasteShortcut(folder); }, canPaste);
        menu->addSeparator();
        fia->addActionsTo(menu, KFileItemActions::MenuActionSource::Services);
        menu->addSeparator();
        auto *tags = tagsMenu(items);
        tags->setParent(menu, tags->windowFlags()); // dies with the context menu
        menu->addMenu(tags);
        addWin12();
        menu->addSeparator();
        entry(QStringLiteral("home_delete_trash"), [this, items] { trash(items); });
        entry(QStringLiteral("home_delete_permanent"), [this, items] { deletePermanently(items); });
        // Inline rename lives in the active view; only offer it for that view's own item.
        const bool canRename = items.size() == 1 && m_view && m_view->url() == folder;
        entry(QStringLiteral("home_rename"), [this, items] {
            if (m_view) {
                m_view->selectUrls({items.first().url()});
                m_view->renameCurrent();
            }
        }, canRename);
        menu->addSeparator();
        entry(QStringLiteral("home_properties"), [this, items, folder] { properties(items, folder); });
        return menu;
    }

    QMenu *view = menu->addMenu(i18n("View"));
    for (const auto &[id, mode] : layoutIds)
        view->addAction(action(QString::fromLatin1(id)));
    menu->addAction(action(QStringLiteral("view_sort")));
    menu->addAction(action(QStringLiteral("view_group")));
    QAction *refresh = menu->addAction(QIcon::fromTheme(QStringLiteral("view-refresh")), i18n("Refresh"));
    connect(refresh, &QAction::triggered, this, [this, folder] {
        if (m_view && m_view->url() == folder)
            m_view->refresh();
    });
    menu->addSeparator();
    entry(QStringLiteral("home_paste"), [this, folder] { paste(folder); }, canPaste);
    entry(QStringLiteral("home_paste_shortcut"), [this, folder] { pasteShortcut(folder); }, canPaste);
    // KNewFileMenu works in the active view's folder; anywhere else these stay disabled rather than create in the wrong place.
    const bool inActiveFolder = m_view && m_view->url() == folder;
    entry(QStringLiteral("home_new_folder"), [this] { m_newFileMenu->createDirectory(); }, inActiveFolder);
    QAction *newItem = action(QStringLiteral("home_new_item"));
    if (inActiveFolder)
        menu->addAction(newItem);
    menu->addSeparator();
    addWin12();
    menu->addSeparator();
    entry(QStringLiteral("home_properties"), [this, folder] { properties({}, folder); }, folder.isValid());
    return menu;
}

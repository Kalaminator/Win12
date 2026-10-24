#include "folderview.h"
#include "tagribbon.h"
#include "tagsearch.h"
#include "tagstore.h"
#include <KDirLister>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QHeaderView>
#include <QLineEdit>
#include <QMimeData>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeView>
#include <QWheelEvent>

class FolderViewTest : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;
    QTemporaryDir m_configDir;
    std::unique_ptr<ViewSettingsStore> m_store;
    std::unique_ptr<FolderView> m_view;
    std::unique_ptr<QTemporaryDir> m_tagDir;

    QUrl dirUrl() const { return QUrl::fromLocalFile(m_dir.path()); }
    QUrl fileUrl(const QString &name) const { return QUrl::fromLocalFile(m_dir.filePath(name)); }

    // Runs `nav` and waits for the listing it starts (cached folders also complete asynchronously).
    template<typename F>
    bool waitListed(F nav)
    {
        QSignalSpy done(m_view->model()->folderModel()->lister(), &KCoreDirLister::listingDirCompleted);
        nav();
        return done.count() > 0 || done.wait(5000);
    }


    static Tag T(const char *s) { return Tag::decode(QString::fromLatin1(s)); }
    static QColor colour(const char *tag) { return T(tag).paintColour(); }
    static int ribbonWidth(const char *tag, const QFont &base) { return TagRibbon::width(T(tag), QFontMetrics(TagRibbon::font(base))); }
    QUrl tagUrl(const QString &name) const { return QUrl::fromLocalFile(m_tagDir->filePath(name)); }

    // A fresh folder under ~/.cache (/tmp may lack user xattrs) holding `files` with their tags; false = no xattr support.
    bool writeTagged(const QList<std::pair<QString, QList<Tag>>> &files)
    {
        QDir().mkpath(QDir::homePath() + QStringLiteral("/.cache"));
        m_tagDir = std::make_unique<QTemporaryDir>(QDir::homePath() + QStringLiteral("/.cache/folderview-XXXXXX"));
        if (!m_tagDir->isValid() || !TagStore::supported(QUrl::fromLocalFile(m_tagDir->path())))
            return false;
        for (const auto &[name, tags] : files) {
            QFile f(m_tagDir->filePath(name));
            if (!f.open(QIODevice::WriteOnly))
                return false;
            for (const Tag &t : tags)
                if (TagStore().add({tagUrl(name)}, t) != TagStore::Error::None)
                    return false;
        }
        return true;
    }
    void show(ViewMode mode)
    {
        ViewSettings s = m_view->settings();
        s.mode = mode;
        s.columns = {QStringLiteral("name"), QStringLiteral("tags")};
        m_view->applySettings(s);
        m_view->currentView()->selectionModel()->clearCurrentIndex(); // Fusion tints the current row's focus frame over the cells
    }
    QModelIndex indexOf(const QString &name, int column = 0) const
    {
        TagsColumns *m = m_view->model();
        for (int r = 0; r < m->rowCount(); ++r)
            if (m->itemFor(m->index(r, 0)).name() == name)
                return m->index(r, column);
        return {};
    }
    QRect cell(const QString &name, int column = 0) const { return m_view->currentView()->visualRect(indexOf(name, column)); }
    // Bounding box of the viewport pixels painted exactly in `c` inside `r`.
    QRect painted(const QRect &r, const QColor &c) const
    {
        const QImage img = m_view->currentView()->viewport()->grab().toImage();
        const QRect area = r & img.rect();
        QRect box;
        for (int y = area.top(); y <= area.bottom(); ++y)
            for (int x = area.left(); x <= area.right(); ++x)
                if (img.pixel(x, y) == c.rgb())
                    box |= QRect(x, y, 1, 1);
        return box;
    }

private Q_SLOTS:
    void init()
    {
        QVERIFY(m_dir.isValid());
        for (const QString &n : {QStringLiteral("a.txt"), QStringLiteral("b.txt")}) {
            QFile f(m_dir.filePath(n));
            QVERIFY(f.open(QIODevice::WriteOnly));
        }
        QDir(m_dir.path()).mkdir(QStringLiteral("sub"));
        m_store = std::make_unique<ViewSettingsStore>(
            KSharedConfig::openConfig(m_configDir.filePath(QStringLiteral("rc")), KConfig::SimpleConfig));
        m_view = std::make_unique<FolderView>(m_store.get());
        m_view->resize(800, 600);
        m_view->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_view.get()));
    }

    void cleanup()
    {
        m_view.reset();
        m_store.reset();
        m_tagDir.reset();
    }

    void opensAndCounts()
    {
        QSignalSpy counts(m_view.get(), &FolderView::countsChanged);
        QVERIFY(waitListed([&] { m_view->openUrl(dirUrl()); }));
        QCOMPARE(m_view->itemCount(), 3);
        QCOMPARE(m_view->url(), dirUrl());
        bool seen = false;
        for (const auto &args : counts)
            seen |= args.at(0).toInt() == 3 && args.at(1).toInt() == 0;
        QVERIFY(seen);
    }

    void itemsInViewOrderFollowsSort()
    {
        QVERIFY(waitListed([&] { m_view->openUrl(dirUrl()); }));
        ViewSettings s = m_view->settings();
        s.sortRole = SortRole::Name;
        s.sortOrder = Qt::DescendingOrder;
        m_view->applySettings(s);
        const auto names = [](const KFileItemList &l) {
            QStringList r;
            for (const KFileItem &i : l)
                r << i.name();
            return r;
        };
        const QStringList all = names(m_view->itemsInViewOrder(false));
        QCOMPARE(all.size(), 3);
        QVERIFY(all.indexOf(QStringLiteral("b.txt")) < all.indexOf(QStringLiteral("a.txt")));
        m_view->selectUrls({fileUrl(QStringLiteral("a.txt")), fileUrl(QStringLiteral("b.txt"))});
        QCOMPARE(names(m_view->itemsInViewOrder(true)), QStringList({QStringLiteral("b.txt"), QStringLiteral("a.txt")}));
    }

    void selectionHelpers()
    {
        QVERIFY(waitListed([&] { m_view->openUrl(dirUrl()); }));
        m_view->selectAll();
        QCOMPARE(m_view->selectedCount(), 3);
        m_view->invertSelection();
        QCOMPARE(m_view->selectedCount(), 0);
        m_view->selectUrls({fileUrl(QStringLiteral("a.txt"))});
        QCOMPARE(m_view->selectedItems().first().name(), QStringLiteral("a.txt"));
        m_view->selectNone();
        QCOMPARE(m_view->selectedCount(), 0);
    }

    void modeSwitchKeepsSelection()
    {
        QVERIFY(waitListed([&] { m_view->openUrl(dirUrl()); }));
        m_view->selectUrls({fileUrl(QStringLiteral("a.txt"))});
        m_view->setViewMode(ViewMode::Details);
        QCOMPARE(m_view->selectedItems().first().name(), QStringLiteral("a.txt"));
        QVERIFY(qobject_cast<QTreeView *>(m_view->currentView()));
        QCOMPARE(m_view->settings().mode, ViewMode::Details);
        m_view->setViewMode(ViewMode::LargeIcons);
        QCOMPARE(m_view->selectedItems().first().name(), QStringLiteral("a.txt"));
        QVERIFY(!qobject_cast<QTreeView *>(m_view->currentView()));
        QCOMPARE(m_view->settings().mode, ViewMode::LargeIcons);
        QCOMPARE(m_store->load(m_view->url()).mode, ViewMode::LargeIcons);
    }

    void zoomFollowsSteps()
    {
        QVERIFY(waitListed([&] { m_view->openUrl(dirUrl()); }));
        m_view->setViewMode(ViewMode::Details);
        m_view->zoom(1);
        QCOMPARE(m_view->settings().mode, ViewMode::List);
        m_view->zoom(-1);
        QCOMPARE(m_view->settings().mode, ViewMode::Details);
    }

    void detailsColumnsFollowSettings()
    {
        QVERIFY(waitListed([&] { m_view->openUrl(dirUrl()); }));
        ViewSettings s = m_view->settings();
        s.mode = ViewMode::Details;
        s.columns = {QStringLiteral("name"), QStringLiteral("size"), QStringLiteral("tags")};
        m_view->applySettings(s);
        auto *tree = qobject_cast<QTreeView *>(m_view->currentView());
        QVERIFY(tree);
        QHeaderView *h = tree->header();
        QList<int> shown;
        for (int v = 0; v < h->count(); ++v)
            if (!h->isSectionHidden(h->logicalIndex(v)))
                shown << h->logicalIndex(v);
        QCOMPARE(shown, (QList<int>{0, 1, 7}));
        QCOMPARE(m_view->model()->headerData(7, Qt::Horizontal).toString(), QStringLiteral("Tags"));
    }

    // Name gets the room, the last section does not stretch, user widths survive navigation.
    void detailsColumnWidths()
    {
        QVERIFY(waitListed([&] { m_view->openUrl(dirUrl()); }));
        ViewSettings s = m_view->settings();
        s.mode = ViewMode::Details;
        s.columns = {QStringLiteral("name"), QStringLiteral("modified"), QStringLiteral("size")};
        m_view->applySettings(s);
        auto *tree = qobject_cast<QTreeView *>(m_view->currentView());
        QVERIFY(tree);
        QHeaderView *h = tree->header();
        QVERIFY(!h->stretchLastSection());
        QVERIFY(h->sectionSize(0) >= 250);
        QVERIFY(h->sectionSize(1) < h->sectionSize(0));
        const QString date = QLocale().toString(QDateTime(QDate(2024, 12, 28), QTime(23, 58)), QLocale::ShortFormat);
        QVERIFY(h->sectionSize(2) > tree->fontMetrics().horizontalAdvance(date));
        h->resizeSection(0, 400);
        QVERIFY(waitListed([&] { m_view->openUrl(fileUrl(QStringLiteral("sub"))); }));
        QCOMPARE(h->sectionSize(0), 400);
    }

    void navHistory()
    {
        QVERIFY(waitListed([&] { m_view->openUrl(dirUrl()); }));
        const QUrl sub = fileUrl(QStringLiteral("sub"));
        QVERIFY(waitListed([&] { m_view->openUrl(sub); }));
        QVERIFY(m_view->canGoBack());
        QVERIFY(waitListed([&] { QVERIFY(m_view->goBack()); }));
        QCOMPARE(m_view->url(), dirUrl());
        QVERIFY(m_view->canGoForward());
        QVERIFY(waitListed([&] { QVERIFY(m_view->goForward()); }));
        QCOMPARE(m_view->url(), sub);
        QVERIFY(waitListed([&] { QVERIFY(m_view->goUp()); }));
        QCOMPARE(m_view->url(), dirUrl());
    }

    void renameGoesThroughSignal()
    {
        QVERIFY(waitListed([&] { m_view->openUrl(dirUrl()); }));
        QSignalSpy spy(m_view.get(), &FolderView::renameRequested);
        m_view->selectUrls({fileUrl(QStringLiteral("a.txt"))});
        m_view->renameCurrent();
        auto *editor = m_view->currentView()->findChild<QLineEdit *>();
        QVERIFY(editor);
        editor->setText(QStringLiteral("c.txt"));
        QTest::keyClick(editor, Qt::Key_Return);
        QTRY_COMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).value<KFileItem>().name(), QStringLiteral("a.txt"));
        QCOMPARE(spy.at(0).at(1).toString(), QStringLiteral("c.txt"));
        QVERIFY(QFile::exists(m_dir.filePath(QStringLiteral("a.txt"))));
    }

    void nameFilter()
    {
        QVERIFY(waitListed([&] { m_view->openUrl(dirUrl()); }));
        m_view->setNameFilter(QStringLiteral("B")); // case-insensitive substring: b.txt and sub
        QCOMPARE(m_view->itemCount(), 2);
        m_view->setNameFilter(QStringLiteral("b.t"));
        QCOMPARE(m_view->itemCount(), 1);
        m_view->setNameFilter(QString());
        QCOMPARE(m_view->itemCount(), 3);
    }

    void checkBoxClickTogglesWithoutMovingCurrent()
    {
        QVERIFY(waitListed([&] { m_view->openUrl(dirUrl()); }));
        ViewSettings s = m_view->settings();
        s.mode = ViewMode::Details;
        s.checkBoxes = true;
        m_view->applySettings(s);
        m_view->selectUrls({fileUrl(QStringLiteral("a.txt"))});
        QAbstractItemView *v = m_view->currentView();
        const QModelIndex b = m_view->model()->indexFor(fileUrl(QStringLiteral("b.txt")));
        const QRect r = v->visualRect(b);
        QTest::mouseClick(v->viewport(), Qt::LeftButton, {}, QPoint(r.left() + 10, r.center().y()));
        QCOMPARE(m_view->selectedCount(), 2);
        QCOMPARE(m_view->currentItem().name(), QStringLiteral("a.txt"));
        QTest::mouseClick(v->viewport(), Qt::LeftButton, {}, QPoint(r.left() + 10, r.center().y()));
        QCOMPARE(m_view->selectedCount(), 1);
    }

    void ctrlWheelZooms()
    {
        QVERIFY(waitListed([&] { m_view->openUrl(dirUrl()); }));
        m_view->setViewMode(ViewMode::Details);
        QWidget *vp = m_view->currentView()->viewport();
        const QPointF pos(vp->width() / 2.0, vp->height() / 2.0);
        QWheelEvent up(pos, vp->mapToGlobal(pos), {}, QPoint(0, 120), Qt::NoButton, Qt::ControlModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(vp, &up);
        QCOMPARE(m_view->settings().mode, ViewMode::List);
    }

    void middleClickOnFolderRequestsTab()
    {
        QVERIFY(waitListed([&] { m_view->openUrl(dirUrl()); }));
        QSignalSpy spy(m_view.get(), &FolderView::folderMiddleClicked);
        const QUrl sub = fileUrl(QStringLiteral("sub"));
        QAbstractItemView *v = m_view->currentView();
        const QRect r = v->visualRect(m_view->model()->indexFor(sub));
        QTest::mouseClick(v->viewport(), Qt::MiddleButton, {}, r.center());
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toUrl(), sub);
    }

    void extensionsHidden()
    {
        const KFileItem a(fileUrl(QStringLiteral("a.txt")));
        const KFileItem sub(fileUrl(QStringLiteral("sub")));
        const KFileItem dot(fileUrl(QStringLiteral(".bashrc")));
        const KFileItem tar(fileUrl(QStringLiteral("archive.tar.gz")));
        QCOMPARE(FolderView::displayName(a, false), QStringLiteral("a"));
        QCOMPARE(FolderView::displayName(a, true), QStringLiteral("a.txt"));
        QCOMPARE(FolderView::displayName(sub, false), QStringLiteral("sub"));
        QCOMPARE(FolderView::displayName(dot, false), QStringLiteral(".bashrc"));
        QCOMPARE(FolderView::displayName(tar, false), QStringLiteral("archive.tar"));
    }
    // A folder dropped onto itself, or onto another dragged folder, never reaches the KIO drop; other drops
    // do, in the list modes too (QListView's Static movement switches viewport drops off).
    void dropOntoADraggedItemIsRejected_data()
    {
        QTest::addColumn<ViewMode>("mode");
        QTest::newRow("details") << ViewMode::Details;
        QTest::newRow("list") << ViewMode::List;
        QTest::newRow("large icons") << ViewMode::LargeIcons;
    }
    void dropOntoADraggedItemIsRejected()
    {
        QFETCH(ViewMode, mode);
        QVERIFY(waitListed([&] { m_view->openUrl(dirUrl()); }));
        m_view->setViewMode(mode);
        QAbstractItemView *v = m_view->currentView();
        QModelIndex sub;
        for (int row = 0; row < m_view->model()->rowCount(); ++row) {
            const QModelIndex i = m_view->model()->index(row, 0);
            if (m_view->model()->itemFor(i).name() == QLatin1String("sub"))
                sub = i;
        }
        QVERIFY(sub.isValid());
        qRegisterMetaType<QDropEvent *>();
        QSignalSpy spy(m_view.get(), &FolderView::dropRequested);
        const auto drop = [&](const QList<QUrl> &urls) {
            QMimeData mime;
            mime.setUrls(urls);
            const QPoint pos = v->visualRect(sub).center();
            // Drop goes to the target a DragEnter registered with the drag manager, as in a real drag.
            QDragEnterEvent enter(pos, Qt::CopyAction | Qt::MoveAction, &mime, Qt::LeftButton, Qt::ControlModifier);
            QApplication::sendEvent(v->viewport(), &enter);
            QDropEvent e(QPointF(pos), Qt::CopyAction | Qt::MoveAction, &mime, Qt::LeftButton, Qt::ControlModifier);
            QApplication::sendEvent(v->viewport(), &e);
        };
        drop({fileUrl(QStringLiteral("sub"))});
        drop({fileUrl(QStringLiteral("a.txt")), fileUrl(QStringLiteral("sub"))});
        QCOMPARE(spy.count(), 0);

        // Control: an outside file dropped (Ctrl = copy) onto sub reaches the default handler and is copied.
        QTemporaryDir outside;
        const QString name = QStringLiteral("x%1.txt").arg(int(mode));
        QFile f(outside.filePath(name));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.close();
        drop({QUrl::fromLocalFile(f.fileName())});
        QCOMPARE(spy.count(), 1);
        QTRY_VERIFY(QFile::exists(m_dir.filePath(QStringLiteral("sub/") + name)));
    }

    // Details: ribbons only in the Tags column, its text suppressed; the name column stays plain.
    void detailsTagsColumnShowsRibbon()
    {
        if (!writeTagged({{QStringLiteral("phone"), {T("Red/Xiaomi")}}}))
            QSKIP("xattr tags not supported here");
        QVERIFY(waitListed([&] { m_view->openUrl(QUrl::fromLocalFile(m_tagDir->path())); }));
        show(ViewMode::Details);
        const QColor red = colour("Red/Xiaomi");
        QTRY_VERIFY(!painted(cell(QStringLiteral("phone"), TagsColumns::TagsColumn), red).isEmpty());
        QVERIFY(painted(cell(QStringLiteral("phone")), red).isEmpty());
    }

    // List: the ribbon follows the name and the item grows to hold it whole.
    void listModeRibbonAfterName()
    {
        if (!writeTagged({{QStringLiteral("phone"), {T("Red/Xiaomi")}}, {QStringLiteral("radio-with-a-much-longer-name"), {}}}))
            QSKIP("xattr tags not supported here");
        QVERIFY(waitListed([&] { m_view->openUrl(QUrl::fromLocalFile(m_tagDir->path())); }));
        show(ViewMode::List);
        const QAbstractItemView *v = m_view->currentView();
        const QRect r = cell(QStringLiteral("phone"));
        QRect box;
        QTRY_VERIFY(!(box = painted(r, colour("Red/Xiaomi"))).isEmpty());
        QVERIFY(box.left() > r.left() + v->iconSize().width() + v->fontMetrics().horizontalAdvance(QStringLiteral("phone")));
        QVERIFY(box.width() >= ribbonWidth("Red/Xiaomi", m_view->font()) - 2);

        // Tagged while shown (the widest item, so nothing else makes its room): the item grows too.
        QCOMPARE(TagStore().add({tagUrl(QStringLiteral("radio-with-a-much-longer-name"))}, T("Gold/Go")), TagStore::Error::None);
        m_view->model()->invalidateTags({tagUrl(QStringLiteral("radio-with-a-much-longer-name"))});
        QTRY_VERIFY(painted(cell(QStringLiteral("radio-with-a-much-longer-name")), colour("Gold/Go")).width() >= ribbonWidth("Gold/Go", m_view->font()) - 2);
    }

    // Small icons, Tiles and Content: a ribbon after the name, on the name's line.
    void otherModesRibbonAfterName()
    {
        if (!writeTagged({{QStringLiteral("phone"), {T("Red/Xiaomi")}}}))
            QSKIP("xattr tags not supported here");
        QVERIFY(waitListed([&] { m_view->openUrl(QUrl::fromLocalFile(m_tagDir->path())); }));
        for (ViewMode mode : {ViewMode::SmallIcons, ViewMode::Tiles, ViewMode::Content}) {
            show(mode);
            const QRect r = cell(QStringLiteral("phone"));
            QRect box;
            QTRY_VERIFY(!(box = painted(r, colour("Red/Xiaomi"))).isEmpty());
            QVERIFY(box.left() > r.left() + m_view->currentView()->iconSize().width() + m_view->fontMetrics().horizontalAdvance(QStringLiteral("phone")));
        }
    }

    // Big icons: Dolphin-style 12x5 flags at the icon's bottom-right, no ribbon.
    void iconModeFlagsAtCorner()
    {
        if (!writeTagged({{QStringLiteral("phone"), {T("Red/Xiaomi")}}}))
            QSKIP("xattr tags not supported here");
        QVERIFY(waitListed([&] { m_view->openUrl(QUrl::fromLocalFile(m_tagDir->path())); }));
        show(ViewMode::LargeIcons);
        const QRect r = cell(QStringLiteral("phone"));
        QRect box;
        QTRY_VERIFY(!(box = painted(r, colour("Red/Xiaomi"))).isEmpty());
        QVERIFY2(box.width() <= 12 && box.height() <= 5, qPrintable(QStringLiteral("%1x%2").arg(box.width()).arg(box.height())));
        QVERIFY(box.left() > r.center().x());
        QVERIFY(box.top() < r.top() + m_view->currentView()->iconSize().height() + 8);
    }

    // One slot per tag, as wide as the longest tag in the folder: every row's slots start at the same x.
    void slotWidthIsLongestTag()
    {
        if (!writeTagged({{QStringLiteral("a"), {T("Red/Xiaomi"), T("Green/b")}}, {QStringLiteral("b"), {T("Gold/Go"), T("Green/b")}}}))
            QSKIP("xattr tags not supported here");
        QVERIFY(waitListed([&] { m_view->openUrl(QUrl::fromLocalFile(m_tagDir->path())); }));
        show(ViewMode::Details);
        QCOMPARE(m_view->tagSlotWidth(), ribbonWidth("Red/Xiaomi", m_view->font()));
        qobject_cast<QTreeView *>(m_view->currentView())->header()->resizeSection(TagsColumns::TagsColumn, 300);
        const QRect a = cell(QStringLiteral("a"), TagsColumns::TagsColumn), b = cell(QStringLiteral("b"), TagsColumns::TagsColumn);
        const QColor green = colour("Green/b");
        QTRY_VERIFY(!painted(b, colour("Gold/Go")).isEmpty());
        QCOMPARE(painted(b, colour("Gold/Go")).left(), painted(a, colour("Red/Xiaomi")).left());
        QVERIFY(!painted(a, green).isEmpty());
        QCOMPARE(painted(b, green).left(), painted(a, green).left());
        QCOMPARE(painted(a, green).left() - painted(a, colour("Red/Xiaomi")).left(), m_view->tagSlotWidth() + TagRibbon::Gap);
    }

    void slotWidthRecomputedAfterInvalidate()
    {
        if (!writeTagged({{QStringLiteral("a"), {T("Gold/Go")}}, {QStringLiteral("b"), {}}}))
            QSKIP("xattr tags not supported here");
        QVERIFY(waitListed([&] { m_view->openUrl(QUrl::fromLocalFile(m_tagDir->path())); }));
        show(ViewMode::Details);
        QCOMPARE(m_view->tagSlotWidth(), ribbonWidth("Gold/Go", m_view->font()));
        QCOMPARE(TagStore().add({tagUrl(QStringLiteral("b"))}, T("Red/Xiaomi")), TagStore::Error::None);
        m_view->model()->invalidateTags({tagUrl(QStringLiteral("b"))});
        QCOMPARE(m_view->tagSlotWidth(), ribbonWidth("Red/Xiaomi", m_view->font()));
    }
    // A longer tag arriving in a later batch widens a clean slot (the insert path adds no full rescan) and stays right.
    void insertWidensWithoutFullRescan()
    {
        if (!writeTagged({{QStringLiteral("a"), {T("Gold/Go")}}}))
            QSKIP("xattr tags not supported here");
        QVERIFY(waitListed([&] { m_view->openUrl(QUrl::fromLocalFile(m_tagDir->path())); }));
        show(ViewMode::Details);
        QCOMPARE(m_view->tagSlotWidth(), ribbonWidth("Gold/Go", m_view->font())); // clean now
        QFile f(m_tagDir->filePath(QStringLiteral("b")));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.close();
        QCOMPARE(TagStore().add({tagUrl(QStringLiteral("b"))}, T("Red/Xiaomi")), TagStore::Error::None);
        QTRY_COMPARE(m_view->tagSlotWidth(), ribbonWidth("Red/Xiaomi", m_view->font()));
        QTest::qWait(100);
        QCOMPARE(m_view->tagSlotWidth(), ribbonWidth("Red/Xiaomi", m_view->font()));
    }

    // Untagged rows paint no ribbons in the Tags column; the slot width is still right for the tagged ones.
    void untaggedRowsDoNotComputeSlot()
    {
        if (!writeTagged({{QStringLiteral("a"), {}}, {QStringLiteral("b"), {T("Gold/Go")}}}))
            QSKIP("xattr tags not supported here");
        QVERIFY(waitListed([&] { m_view->openUrl(QUrl::fromLocalFile(m_tagDir->path())); }));
        show(ViewMode::Details);
        QTRY_VERIFY(!painted(cell(QStringLiteral("b"), TagsColumns::TagsColumn), colour("Gold/Go")).isEmpty());
        QVERIFY(painted(cell(QStringLiteral("a"), TagsColumns::TagsColumn), colour("Gold/Go")).isEmpty());
        QCOMPARE(m_view->tagSlotWidth(), ribbonWidth("Gold/Go", m_view->font()));
    }

    void groupByTag(GroupRole role)
    {
        ViewSettings s = m_view->settings();
        s.groupRole = role;
        m_view->applySettings(s);
    }
    // A file with two tags shows twice under Group by tag name; selecting everything still yields each file once.
    void selectedItemsDedupeDuplicates()
    {
        if (!writeTagged({{QStringLiteral("a"), {T("Red/Xiaomi"), T("OrangeBright/Urgent")}}, {QStringLiteral("b"), {}}}))
            QSKIP("xattr tags not supported here");
        QVERIFY(waitListed([&] { m_view->openUrl(QUrl::fromLocalFile(m_tagDir->path())); }));
        groupByTag(GroupRole::TagName);
        QCOMPARE(m_view->model()->rowCount(), 3);
        QCOMPARE(m_view->itemCount(), 2);
        m_view->selectAll();
        QCOMPARE(m_view->selectedItems().size(), 2);
        QCOMPARE(m_view->selectedCount(), 2);
        QCOMPARE(m_view->itemsInViewOrder(false).size(), 2);
    }
    void groupByTagShowsHeaders()
    {
        if (!writeTagged({{QStringLiteral("a"), {T("Red/Xiaomi"), T("OrangeBright/Urgent")}}}))
            QSKIP("xattr tags not supported here");
        QVERIFY(waitListed([&] { m_view->openUrl(QUrl::fromLocalFile(m_tagDir->path())); }));
        groupByTag(GroupRole::TagName);
        TagsColumns *m = m_view->model();
        int firsts = 0;
        for (int r = 0; r < m->rowCount(); ++r)
            firsts += m->index(r, 0).data(FolderModel::GroupFirstRole).toBool();
        QCOMPARE(firsts, 2);
        QCOMPARE(m->folderModel()->groupRole(), GroupRole::None); // the proxy groups, FolderModel only sorts
        groupByTag(GroupRole::Name);
        QCOMPARE(m->rowCount(), 1);
        QCOMPARE(m->folderModel()->groupRole(), GroupRole::Name);
    }

    // Opens the tagged folder and shows the Red/Xiaomi search over it, walked to the end.
    bool showRedResults()
    {
        if (!writeTagged({{QStringLiteral("b"), {T("Red/Xiaomi")}},
                          {QStringLiteral("a"), {T("Red/Xiaomi"), T("OrangeBright/Urgent")}},
                          {QStringLiteral("c"), {}}}))
            return false;
        [&] { QVERIFY(waitListed([&] { m_view->openUrl(QUrl::fromLocalFile(m_tagDir->path())); })); }();
        auto *search = new TagSearch;
        QSignalSpy done(search, &TagSearch::finished);
        QSignalSpy urls(m_view.get(), &FolderView::urlChanged);
        QSignalSpy titles(m_view.get(), &FolderView::resultsTitleChanged);
        m_view->showResults(QStringLiteral("Red"), search);
        search->start(QUrl::fromLocalFile(m_tagDir->path()), {T("Red/Xiaomi")}, TagSearch::Mode::Any, TagSearch::Engine::Walk);
        [&] { QVERIFY(done.wait(10000)); }();
        [&] { QCOMPARE(urls.count(), 0); }();
        [&] { QCOMPARE(titles.count(), 1); }();
        [&] { QCOMPARE(titles.first().at(0).toString(), QStringLiteral("Red")); }();
        return true;
    }
    QStringList namesInViewOrder() const
    {
        QStringList names;
        for (const KFileItem &i : m_view->itemsInViewOrder(false))
            names << i.name();
        return names;
    }
    void showResultsListsFoundItems()
    {
        if (!showRedResults())
            QSKIP("xattr tags not supported here");
        QVERIFY(m_view->showingResults());
        QCOMPARE(m_view->model()->rowCount(), 2);
        QCOMPARE(m_view->itemCount(), 2);
        QCOMPARE(m_view->url(), QUrl::fromLocalFile(m_tagDir->path()));
        QCOMPARE(m_view->model()->index(0, TagsColumns::TagsColumn).data(TagsColumns::TagsRole).value<QList<Tag>>().isEmpty(), false);
        QVERIFY(m_view->model()->indexFor(tagUrl(QStringLiteral("b"))).isValid());
        QSignalSpy titles(m_view.get(), &FolderView::resultsTitleChanged);
        m_view->clearResults();
        QVERIFY(!m_view->showingResults());
        QCOMPARE(titles.count(), 1);
        QCOMPARE(titles.first().at(0).toString(), QString());
        QCOMPARE(m_view->itemCount(), 3);
    }
    void openUrlClearsResults()
    {
        if (!showRedResults())
            QSKIP("xattr tags not supported here");
        QVERIFY(waitListed([&] { m_view->openUrl(dirUrl()); }));
        QVERIFY(!m_view->showingResults());
        QCOMPARE(m_view->itemCount(), 3);
        QCOMPARE(m_view->url(), dirUrl());
    }
    void sortByNameWorksOnResults()
    {
        if (!showRedResults())
            QSKIP("xattr tags not supported here");
        ViewSettings s = m_view->settings();
        s.sortRole = SortRole::Name;
        s.sortOrder = Qt::AscendingOrder;
        m_view->applySettings(s);
        QCOMPARE(namesInViewOrder(), (QStringList{QStringLiteral("a"), QStringLiteral("b")}));
        s.sortOrder = Qt::DescendingOrder;
        m_view->applySettings(s);
        QCOMPARE(namesInViewOrder(), (QStringList{QStringLiteral("b"), QStringLiteral("a")}));
        s.sortRole = SortRole::Type; // FolderModel's own Type compare, then the name order
        s.sortOrder = Qt::AscendingOrder;
        m_view->applySettings(s);
        QCOMPARE(namesInViewOrder(), (QStringList{QStringLiteral("a"), QStringLiteral("b")}));
    }
};

QTEST_MAIN(FolderViewTest)
#include "folderview_test.moc"

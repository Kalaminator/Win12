#include "folderview.h"
#include <KDirLister>
#include <QApplication>
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
        const QModelIndex b = m_view->model()->mapFromSource(m_view->model()->folderModel()->indexFor(fileUrl(QStringLiteral("b.txt"))));
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
        const QRect r = v->visualRect(m_view->model()->mapFromSource(m_view->model()->folderModel()->indexFor(sub)));
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
};

QTEST_MAIN(FolderViewTest)
#include "folderview_test.moc"

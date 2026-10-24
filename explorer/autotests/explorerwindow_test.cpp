#include "explorerwindow.h"
#include "exploreractions.h"
#include "folderview.h"
#include "renamemanydialog.h"
#include "ribbonbar.h"
#include "rulelibrary.h"
#include "selectbyruledialog.h"
#include "tagregistry.h"
#include "taglist.h"
#include "tagstore.h"
#include "titlebar.h"
#include "windowshadow.h"
#include <QAbstractItemView>
#include <KConfigGroup>
#include <KSharedConfig>
#include <QAction>
#include <QFile>
#include <QLabel>
#include <QListWidget>
#include <QLineEdit>
#include <QStackedWidget>
#include <QStatusBar>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTabBar>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>

using namespace Qt::StringLiterals;

class ExplorerWindowTest : public QObject
{
    Q_OBJECT
    QTemporaryDir m_dir;
    ExplorerWindow *shownWindow()
    {
        auto *w = new ExplorerWindow({QUrl::fromLocalFile(m_dir.path())});
        w->show();
        w->activateWindow();
        if (!QTest::qWaitForWindowActive(w))
            qWarning("window not active");
        return w;
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void shadowImageIsSoftAndSymmetric()
    {
        const QImage img = WindowShadow::image(8, Qt::black);
        QCOMPARE(img.size(), QSize(17, 17));
        QVERIFY(img.pixelColor(8, 8).alpha() > img.pixelColor(1, 1).alpha());
        QVERIFY(img.pixelColor(1, 1).alpha() > 0);
        QCOMPARE(img.pixelColor(0, 8), img.pixelColor(16, 8));
        QVERIFY(img.pixelColor(0, 0).alpha() < 20);
    }

    void shadowInstallIsHarmlessOffscreen()
    {
        ExplorerWindow *w = shownWindow();
        QVERIFY(w->isVisible());
        delete w;
    }

    void tagPickShowsResultsAndNavigationClears()
    {
        QTemporaryDir tree(QDir::homePath() + QStringLiteral("/.cache/tagpick-XXXXXX"));
        if (!tree.isValid() || !TagStore::supported(QUrl::fromLocalFile(tree.path())))
            QSKIP("xattr tags not supported here");
        QFile f(tree.filePath(QStringLiteral("a.txt")));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.close();
        const Tag tag = Tag::decode(QStringLiteral("Red/pick"));
        TagStore s;
        QCOMPARE(s.add({QUrl::fromLocalFile(f.fileName())}, tag), TagStore::Error::None);
        TagRegistry().touch(tag); // test-mode tags.json, read by the window's own registry
        auto *w = new ExplorerWindow({QUrl::fromLocalFile(tree.path())});
        w->show();
        w->activateWindow();
        auto *list = w->findChild<TagList *>();
        QVERIFY(list);
        auto *lw = list->findChild<QListWidget *>();
        QTRY_VERIFY(!list->isHidden());
        QTest::mouseClick(lw->viewport(), Qt::LeftButton, {}, lw->visualItemRect(lw->item(0)).center());
        QVERIFY(w->activeView()->showingResults());
        w->activeView()->openUrl(QUrl::fromLocalFile(m_dir.path()));
        QVERIFY(!w->activeView()->showingResults());
        QVERIFY(list->picked().isEmpty());
        delete w;
    }

    void selectFromRuleOpensTheFolder()
    {
        QDir(m_dir.path()).mkpath(QStringLiteral("sub"));
        QFile f(m_dir.filePath(QStringLiteral("sub/f.txt")));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.close();
        ExplorerWindow *w = shownWindow();
        Q_EMIT w->explorerActions()->selectUrlsRequested({QUrl::fromLocalFile(f.fileName())});
        QTRY_COMPARE(w->activeView()->url().adjusted(QUrl::StripTrailingSlash), QUrl::fromLocalFile(m_dir.filePath(QStringLiteral("sub"))));
        delete w;
    }

    void fileKeysIgnoreTheSearchBox()
    {
        ExplorerWindow *w = shownWindow();
        // F2: QLineEdit does not claim it, so only the shortcut context keeps it away from the search box.
        auto *rename = w->findChild<QAction *>(QStringLiteral("home_rename"));
        rename->setEnabled(true); // an empty folder disables it
        QSignalSpy spy(rename, &QAction::triggered);
        QLineEdit *search = nullptr;
        for (QLineEdit *le : w->findChildren<QLineEdit *>())
            if (le->placeholderText().startsWith(QLatin1String("Search")))
                search = le;
        QVERIFY(search);
        search->setFocus();
        QTest::keyClick(search, Qt::Key_F2);
        QCOMPARE(spy.count(), 0);
        w->activeView()->currentView()->setFocus();
        QTest::keyClick(w->activeView()->currentView(), Qt::Key_F2);
        QCOMPARE(spy.count(), 1);
        delete w;
    }

    // The window built the real way lists its folder all the way to the status bar.
    void listsTheFolderIntoTheStatusBar()
    {
        QTemporaryDir dir;
        for (const char *name : {"a.txt", "b.txt", "c.txt"}) {
            QFile f(dir.filePath(QString::fromLatin1(name)));
            QVERIFY(f.open(QIODevice::WriteOnly));
        }
        ExplorerWindow w({QUrl::fromLocalFile(dir.path())});
        w.show();
        QTRY_COMPARE(w.activeView()->itemCount(), 3);
        QCOMPARE(w.statusBar()->findChild<QLabel *>()->text(), QStringLiteral("3 items"));
    }

    void theStartFolderIsRecent()
    {
        QTemporaryDir dir;
        ExplorerWindow w({QUrl::fromLocalFile(dir.path())});
        QCOMPARE(ExplorerWindow::recentFolders(KSharedConfig::openConfig()).value(0), QUrl::fromLocalFile(dir.path()).toString());
    }

    void openInSplitOpensSecondPane()
    {
        QTemporaryDir d2;
        ExplorerWindow *w = shownWindow();
        QVERIFY(!w->page()->isSplit());
        const QUrl dir2 = QUrl::fromLocalFile(d2.path());
        Q_EMIT w->explorerActions()->openInSplitRequested(dir2);
        QVERIFY(w->page()->isSplit());
        QTRY_COMPARE(w->page()->secondary()->url(), dir2);
        delete w;
    }
    void splitMarksTheActivePane()
    {
        ExplorerWindow *w = shownWindow();
        FolderPage *page = w->page();
        const auto marked = [](FolderView *v) {
            auto *mark = v->parentWidget()->findChild<QWidget *>(QStringLiteral("paneMark"), Qt::FindDirectChildrenOnly);
            return mark && mark->isVisible();
        };
        QVERIFY(!marked(page->primary())); // a single pane needs no mark
        page->setSplit(true);
        QVERIFY(marked(page->primary()));
        QVERIFY(!marked(page->secondary()));
        page->activate(page->secondary());
        QVERIFY(!marked(page->primary()));
        QVERIFY(marked(page->secondary()));
        page->setSplit(false);
        QVERIFY(!marked(page->primary()));
        delete w;
    }

    void noTabBarAndCtrlTDoesNothing()
    {
        ExplorerWindow *w = shownWindow();
        QVERIFY(!w->findChild<QTabBar *>());
        QTest::keyClick(w, Qt::Key_T, Qt::ControlModifier);
        QCOMPARE(ExplorerWindow::last(), w);
        delete w;
    }

    void ctrlWClosesWindow()
    {
        ExplorerWindow *w = shownWindow();
        QTest::keyClick(w, Qt::Key_W, Qt::ControlModifier);
        QTRY_VERIFY(!w->isVisible());
        delete w;
    }

    void middleClickOpensNewWindow()
    {
        ExplorerWindow *w = shownWindow();
        QTemporaryDir other;
        const QUrl dir = QUrl::fromLocalFile(other.path());
        Q_EMIT w->activeView()->folderMiddleClicked(dir);
        ExplorerWindow *n = ExplorerWindow::last();
        QVERIFY(n != w);
        QCOMPARE(n->activeView()->url(), dir);
        delete n;
        delete w;
    }

    void sessionRestoresFolderAndSplit()
    {
        QTemporaryDir other;
        const QUrl dir2 = QUrl::fromLocalFile(other.path());
        ExplorerWindow *w = shownWindow();
        w->page()->setSplit(true);
        w->page()->secondary()->openUrl(dir2);
        w->close();
        delete w;
        auto *r = new ExplorerWindow();
        QCOMPARE(r->page()->primary()->url(), QUrl::fromLocalFile(m_dir.path()));
        QVERIFY(r->page()->isSplit());
        QCOMPARE(r->page()->secondary()->url(), dir2);
        delete r;
    }

    void sessionDropsMissingPrimary()
    {
        QTemporaryDir other;
        const QUrl dir = QUrl::fromLocalFile(other.path());
        KConfigGroup g(KSharedConfig::openConfig(), QStringLiteral("Session"));
        g.writeEntry("Url", QStringLiteral("file:///nonexistent-xyz"));
        g.writeEntry("SecondUrl", dir.toString());
        g.sync();
        auto *r = new ExplorerWindow();
        QVERIFY(!r->page()->isSplit());
        QCOMPARE(r->page()->primary()->url(), dir);
        delete r;
    }

    static QUrl sel(ExplorerWindow *w)
    {
        const KFileItemList l = w->activeView()->selectedItems();
        return l.isEmpty() ? QUrl() : l.first().url();
    }

    void selectItemsWorksOnAnAlreadyListedFolder()
    {
        QDir d(m_dir.path());
        d.mkdir(QStringLiteral("sub"));
        for (const char *f : {"sub/a.txt", "sub/b.txt", "top.txt"})
            QVERIFY(QFile(d.filePath(QLatin1String(f))).open(QIODevice::WriteOnly));
        ExplorerWindow *w = shownWindow();
        const QUrl a = QUrl::fromLocalFile(d.filePath(QStringLiteral("sub/a.txt")));
        const QUrl b = QUrl::fromLocalFile(d.filePath(QStringLiteral("sub/b.txt")));
        w->selectItems({a});
        QTRY_COMPARE(w->activeView()->selectedItems().count(), 1);
        QCOMPARE(sel(w), a);
        w->selectItems({b}); // same folder, already listed
        QTRY_COMPARE(sel(w), b);
        w->selectItems({QUrl::fromLocalFile(d.filePath(QStringLiteral("top.txt")))}); // back to a listed folder
        QTRY_COMPARE(sel(w).fileName(), QStringLiteral("top.txt"));
        delete w;
    }
    void selectByRuleOnOpensFolderDialog()
    {
        ExplorerWindow *w = shownWindow();
        QUrl seen;
        w->explorerActions()->setExecSelectDialog([&](SelectByRuleDialog &d) {
            seen = d.lookIn();
            return false;
        });
        w->selectByRuleOn({QUrl::fromLocalFile(m_dir.path() + QStringLiteral("/x.txt"))});
        QCOMPARE(seen, QUrl::fromLocalFile(m_dir.path()));
        delete w;
    }
    void selectByRuleOnNoUrlsUsesHome()
    {
        ExplorerWindow *w = shownWindow();
        QUrl seen;
        w->explorerActions()->setExecSelectDialog([&](SelectByRuleDialog &d) {
            seen = d.lookIn();
            return false;
        });
        w->selectByRuleOn({});
        QCOMPARE(seen, QUrl::fromLocalFile(QDir::homePath()));
        delete w;
    }
    void renameManyOnSingleFolder()
    {
        QVERIFY(QDir(m_dir.path()).mkpath(QStringLiteral("sub")));
        ExplorerWindow *w = shownWindow();
        qsizetype rows = -1;
        w->explorerActions()->setExecRenameDialog([&](RenameManyDialog &d) {
            rows = d.plan().size();
            return false;
        });
        w->renameManyOn({QUrl::fromLocalFile(m_dir.path() + QStringLiteral("/sub"))});
        QCOMPARE(rows, 1);
        delete w;
    }
    void runSavedRuleSelectKindOpensFormula()
    {
        ExplorerWindow *w = shownWindow();
        QString err;
        QVERIFY(w->explorerActions()->library().save({QStringLiteral("r"), QStringLiteral("=ISFILE()"), SavedRule::Kind::Select}, &err));
        QString seen;
        w->explorerActions()->setExecSelectDialog([&](SelectByRuleDialog &d) {
            seen = d.formula();
            return false;
        });
        w->runSavedRule(QStringLiteral("r"), {});
        QCOMPARE(seen, QStringLiteral("=ISFILE()"));
        delete w;
    }
    void folderFromStatPicksFolderOrParent()
    {
        const QUrl x(QStringLiteral("smb://nas/share/x"));
        QCOMPARE(ExplorerWindow::folderFromStat(x, true), x);
        QCOMPARE(ExplorerWindow::folderFromStat(x, false), QUrl(QStringLiteral("smb://nas/share")));
    }
    void lastWindowTracksNewest()
    {
        ExplorerWindow *a = shownWindow();
        ExplorerWindow *b = shownWindow();
        QCOMPARE(ExplorerWindow::last(), b);
        delete b;
        QVERIFY(ExplorerWindow::last() == nullptr);
        delete a;
    }

    void windowIsFramelessWithTitleBar()
    {
        ExplorerWindow *w = shownWindow();
        QVERIFY(w->windowFlags() & Qt::FramelessWindowHint);
        TitleBar *tb = w->titleBar();
        QVERIFY(tb);
        auto *ribbon = w->findChild<RibbonBar *>();
        QVERIFY(tb->isAncestorOf(ribbon->tabStrip()));
        QWidget *strip = ribbon->tabStrip();
        QVERIFY(strip->sizeHint().width() > 100);
        QVERIFY(strip->minimumSizeHint().width() > 100);
        QVERIFY(strip->width() >= strip->sizeHint().width());
        QCOMPARE(tb->closeButton()->toolTip(), u"Close"_s);
        QCOMPARE(tb->maximizeButton()->toolTip(), u"Maximise"_s);
        auto *body = ribbon->findChild<QStackedWidget *>();
        QVERIFY(body);
        QCOMPARE(body->mapTo(w, QPoint(0, 0)).y(), tb->mapTo(w, QPoint(0, tb->height())).y());
        delete w;
    }

    void resizeEdgesAtTable()
    {
        const QSize s(800, 600);
        QCOMPARE(ExplorerWindow::resizeEdgesAt({3, 3}, s, 6), Qt::TopEdge | Qt::LeftEdge);
        QCOMPARE(ExplorerWindow::resizeEdgesAt({400, 2}, s, 6), Qt::Edges(Qt::TopEdge));
        QCOMPARE(ExplorerWindow::resizeEdgesAt({798, 300}, s, 6), Qt::Edges(Qt::RightEdge));
        QCOMPARE(ExplorerWindow::resizeEdgesAt({797, 598}, s, 6), Qt::BottomEdge | Qt::RightEdge);
        QCOMPARE(ExplorerWindow::resizeEdgesAt({400, 300}, s, 6), Qt::Edges());
        QCOMPARE(ExplorerWindow::resizeEdgesAt({6, 300}, s, 6), Qt::Edges());
    }

    void scrollbarsGetANarrowBand()
    {
        ExplorerWindow *w = shownWindow();
        const int x = w->width() - 4;
        QCOMPARE(w->edgesForPress({x, 300}, true), Qt::Edges());
        QCOMPARE(w->edgesForPress({w->width() - 1, 300}, true), Qt::Edges(Qt::RightEdge));
        QCOMPARE(w->edgesForPress({x, 300}), Qt::Edges(Qt::RightEdge));
        delete w;
    }

    void maximizeIconFollowsState()
    {
        ExplorerWindow *w = shownWindow();
        const QIcon normal = w->titleBar()->maximizeButton()->icon();
        w->showMaximized();
        QTRY_VERIFY(w->isMaximized());
        QCOMPARE_NE(w->titleBar()->maximizeButton()->icon().name(), normal.name());
        delete w;
    }

    void noResizeEdgesWhenMaximized()
    {
        ExplorerWindow *w = shownWindow();
        QCOMPARE(w->edgesForPress({2, 300}), Qt::Edges(Qt::LeftEdge));
        w->showMaximized();
        QTRY_VERIFY(w->isMaximized());
        QCOMPARE(w->edgesForPress({2, 300}), Qt::Edges());
        delete w;
    }

    void maximizeButtonToggles()
    {
        ExplorerWindow *w = shownWindow();
        QTest::mouseClick(w->titleBar()->maximizeButton(), Qt::LeftButton);
        QTRY_VERIFY(w->isMaximized());
        QTest::mouseClick(w->titleBar()->maximizeButton(), Qt::LeftButton);
        QTRY_VERIFY(!w->isMaximized());
        delete w;
    }

    void closeButtonClosesWindow()
    {
        ExplorerWindow *w = shownWindow();
        QTest::mouseClick(w->titleBar()->closeButton(), Qt::LeftButton);
        QTRY_VERIFY(!w->isVisible());
        delete w;
    }

    void windowTitleStillSet()
    {
        ExplorerWindow *w = shownWindow();
        QVERIFY(w->windowTitle().endsWith(u"Win12 Explorer"_s));
        delete w;
    }

    void clickingViewTabInTitleBarSwitchesRibbon()
    {
        ExplorerWindow *w = shownWindow();
        auto *ribbon = w->findChild<RibbonBar *>();
        QWidget *strip = ribbon->tabStrip();
        const QFontMetrics fm = strip->fontMetrics();
        const int x = fm.horizontalAdvance(u"File"_s) + 28 + fm.horizontalAdvance(u"Home"_s) + 28 + 10; // inside View
        QVERIFY(strip->width() > x); // the strip really has room for the View tab
        QCOMPARE(w->childAt(strip->mapTo(w, QPoint(x, 20))), strip); // and is what sits there in the window
        QTest::mouseClick(strip, Qt::LeftButton, {}, QPoint(x, 20));
        QCOMPARE(ribbon->currentTab(), 1);
        delete w;
    }
};

QTEST_MAIN(ExplorerWindowTest)
#include "explorerwindow_test.moc"

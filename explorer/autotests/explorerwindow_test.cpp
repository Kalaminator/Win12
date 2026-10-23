#include "explorerwindow.h"
#include "folderview.h"
#include <QAbstractItemView>
#include <QAction>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QStatusBar>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTabBar>
#include <QTemporaryDir>
#include <QTest>

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

    void splitMarksTheActivePane()
    {
        ExplorerWindow *w = shownWindow();
        TabPage *page = w->currentPage();
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

    void ctrlTAndCtrlWOpenAndCloseTabs()
    {
        ExplorerWindow *w = shownWindow();
        auto *tabs = w->findChild<QTabBar *>();
        QCOMPARE(tabs->count(), 1);
        QTest::keyClick(w, Qt::Key_T, Qt::ControlModifier);
        QCOMPARE(tabs->count(), 2);
        QCOMPARE(w->currentPage()->url(), QUrl::fromLocalFile(m_dir.path()));
        QTest::keyClick(w, Qt::Key_W, Qt::ControlModifier);
        QCOMPARE(tabs->count(), 1);
        delete w;
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
};

QTEST_MAIN(ExplorerWindowTest)
#include "explorerwindow_test.moc"

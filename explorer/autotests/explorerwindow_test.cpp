#include "explorerwindow.h"
#include <QAbstractItemView>
#include <QAction>
#include <QLineEdit>
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
};

QTEST_MAIN(ExplorerWindowTest)
#include "explorerwindow_test.moc"

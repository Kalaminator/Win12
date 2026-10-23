#include "exploreractions.h"
#include "folderview.h"
#include "ribbonspec.h"
#include <KDirLister>
#include <QAction>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMenu>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

namespace {
void writeFile(const QString &path, const QByteArray &data)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

KFileItem itemFor(const QString &path)
{
    return KFileItem(QUrl::fromLocalFile(path));
}

QList<QAction *> win12Entries(QMenu *menu)
{
    QList<QAction *> found;
    for (QAction *a : menu->actions())
        if (a->text() == QLatin1String("Win12"))
            found << a;
    return found;
}
}

class ExplorerActionsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void everySpecIdHasAnAction()
    {
        ExplorerActions actions(nullptr);
        const QStringList menuIds = {QStringLiteral("home_history"), QStringLiteral("view_sort"), QStringLiteral("view_group"),
                                     QStringLiteral("view_columns"), QStringLiteral("home_new_item"), QStringLiteral("home_delete"),
                                     QStringLiteral("home_open"), QStringLiteral("share_share")};
        for (const QString &id : Ribbon::actionIds()) {
            QAction *a = actions.action(id);
            QVERIFY2(a, qPrintable(id));
            const Ribbon::Button *b = Ribbon::button(id);
            QCOMPARE(a->text(), b->text);
            if (QIcon::hasThemeIcon(b->icon))
                QCOMPARE(a->icon().name(), b->icon);
            else
                QVERIFY2(!a->icon().isNull() || a->icon().name() == b->icon, qPrintable(id));
            QCOMPARE(a->menu() != nullptr, b->menu);
            QCOMPARE(b->menu, menuIds.contains(id));
        }
        QCOMPARE(actions.action(QStringLiteral("nope")), nullptr);
    }

    void shortcuts()
    {
        ExplorerActions actions(nullptr);
        QCOMPARE(actions.action(QStringLiteral("home_copy"))->shortcut(), QKeySequence(QKeySequence::Copy));
        QCOMPARE(actions.action(QStringLiteral("home_undo"))->shortcut(), QKeySequence(QKeySequence::Undo));
        QCOMPARE(actions.action(QStringLiteral("home_rename"))->shortcut(), QKeySequence(Qt::Key_F2));
        QCOMPARE(actions.action(QStringLiteral("view_split"))->shortcut(), QKeySequence(Qt::Key_F4));
    }

    void renameRefusesExistingName()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath(QStringLiteral("a.txt")), "aaa");
        writeFile(dir.filePath(QStringLiteral("b.txt")), "bbbbbb");
        const KFileItem a = itemFor(dir.filePath(QStringLiteral("a.txt")));

        QCOMPARE(ExplorerActions::rename(a, QStringLiteral("b.txt"), nullptr), QStringLiteral("A file named b.txt already exists."));
        QCOMPARE(QFileInfo(dir.filePath(QStringLiteral("a.txt"))).size(), 3);
        QCOMPARE(QFileInfo(dir.filePath(QStringLiteral("b.txt"))).size(), 6);

        QCOMPARE(ExplorerActions::rename(a, QStringLiteral("c.txt"), nullptr), QString());
        QVERIFY(QFile::exists(dir.filePath(QStringLiteral("c.txt"))));
        QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("a.txt"))));

        const KFileItem c = itemFor(dir.filePath(QStringLiteral("c.txt")));
        QVERIFY(!ExplorerActions::rename(c, QStringLiteral("x/y"), nullptr).isEmpty());
        QVERIFY(!ExplorerActions::rename(c, QString(), nullptr).isEmpty());
        QVERIFY(!ExplorerActions::rename(c, QStringLiteral(".."), nullptr).isEmpty());
        QVERIFY(QFile::exists(dir.filePath(QStringLiteral("c.txt"))));
    }

    void renameRefusesExistingFolder()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkdir(QStringLiteral("dirA"));
        QDir(dir.path()).mkdir(QStringLiteral("dirB"));
        writeFile(dir.filePath(QStringLiteral("dirA/a.txt")), "a");
        writeFile(dir.filePath(QStringLiteral("dirB/b.txt")), "b");
        QVERIFY(!ExplorerActions::rename(itemFor(dir.filePath(QStringLiteral("dirA"))), QStringLiteral("dirB"), nullptr).isEmpty());
        QVERIFY(QFile::exists(dir.filePath(QStringLiteral("dirA/a.txt"))));
        QVERIFY(QFile::exists(dir.filePath(QStringLiteral("dirB/b.txt"))));
        QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("dirB/a.txt"))));
    }

    void renameReportsTheRealError()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath(QStringLiteral("gone.txt")), "x");
        const KFileItem gone = itemFor(dir.filePath(QStringLiteral("gone.txt")));
        QFile::remove(dir.filePath(QStringLiteral("gone.txt")));
        const QString err = ExplorerActions::rename(gone, QStringLiteral("new.txt"), nullptr);
        QVERIFY(!err.isEmpty());
        QVERIFY2(!err.contains(QLatin1String("already exists")), qPrintable(err));
    }

    // Open on a selected folder navigates the active view (no OpenUrlJob, no second file manager).
    void openNavigatesIntoAFolder()
    {
        QTemporaryDir dir, configDir;
        QDir(dir.path()).mkdir(QStringLiteral("sub"));
        ViewSettingsStore store(KSharedConfig::openConfig(configDir.filePath(QStringLiteral("rc")), KConfig::SimpleConfig));
        FolderView view(&store);
        ExplorerActions actions(nullptr);
        QSignalSpy done(view.model()->folderModel()->lister(), &KCoreDirLister::listingDirCompleted);
        view.openUrl(QUrl::fromLocalFile(dir.path()));
        QVERIFY(done.count() > 0 || done.wait(5000));
        actions.setActiveView(&view);
        const QUrl sub = QUrl::fromLocalFile(dir.filePath(QStringLiteral("sub")));
        view.selectUrls({sub});
        QCOMPARE(view.selectedItems().size(), 1);
        actions.action(QStringLiteral("home_open"))->trigger();
        QCOMPARE(view.url(), sub);
    }

    void contextMenuHasWin12OnlyWhenFilled()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath(QStringLiteral("a.txt")), "a");
        const KFileItemList items = {itemFor(dir.filePath(QStringLiteral("a.txt")))};
        const QUrl folder = QUrl::fromLocalFile(dir.path());
        ExplorerActions actions(nullptr);

        std::unique_ptr<QMenu> plain(actions.contextMenu(items, folder));
        QVERIFY(win12Entries(plain.get()).isEmpty());

        connect(&actions, &ExplorerActions::fillWin12Menu, this, [](QMenu *m, const KFileItemList &) {
            m->addAction(QStringLiteral("Tag"));
        });
        std::unique_ptr<QMenu> filled(actions.contextMenu(items, folder));
        const QList<QAction *> win12 = win12Entries(filled.get());
        QCOMPARE(win12.size(), 1);
        QVERIFY(win12.first()->menu());
        QCOMPARE(win12.first()->menu()->actions().size(), 1);
    }

    void enabledFollowsSelection()
    {
        QTemporaryDir dir, configDir;
        writeFile(dir.filePath(QStringLiteral("a.txt")), "a");
        ViewSettingsStore store(KSharedConfig::openConfig(configDir.filePath(QStringLiteral("rc")), KConfig::SimpleConfig));
        FolderView view(&store);
        ExplorerActions actions(nullptr);
        QAction *copy = actions.action(QStringLiteral("home_copy"));
        QAction *newFolder = actions.action(QStringLiteral("home_new_folder"));
        QVERIFY(!newFolder->isEnabled()); // no active view: nothing to run against

        QSignalSpy done(view.model()->folderModel()->lister(), &KCoreDirLister::listingDirCompleted);
        view.openUrl(QUrl::fromLocalFile(dir.path()));
        QVERIFY(done.count() > 0 || done.wait(5000));
        actions.setActiveView(&view);

        view.selectNone();
        QVERIFY(!copy->isEnabled());
        QVERIFY(newFolder->isEnabled());
        view.selectAll();
        QVERIFY(copy->isEnabled());

        actions.setActiveView(nullptr);
        QVERIFY(!copy->isEnabled());
        QVERIFY(!newFolder->isEnabled());
    }
};

QTEST_MAIN(ExplorerActionsTest)
#include "exploreractions_test.moc"

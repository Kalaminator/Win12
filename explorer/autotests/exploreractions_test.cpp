#include <KFileMetaData/UserMetaData>
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
#include <QTimer>
#include <QApplication>
#include <QClipboard>
#include <KIO/FileUndoManager>
#include "renamemanydialog.h"
#include "newtagdialog.h"
#include <QDialogButtonBox>
#include <QPushButton>
#include <memory>
#include "tagstore.h"
#include <KFileMetaData/UserMetaData>
#include <QStandardPaths>

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

    QTemporaryDir tagDir{QDir::homePath() + QStringLiteral("/.cache/tagactions-XXXXXX")};
    QUrl tagFile(const QString &n)
    {
        const QString p = tagDir.filePath(n);
        QFile f(p);
        (void)f.open(QIODevice::WriteOnly);
        return QUrl::fromLocalFile(p);
    }
    static Tag T(const char *s) { return Tag::decode(QString::fromLatin1(s)); }
    bool tagsUnsupported() { return !tagDir.isValid() || !TagStore::supported(QUrl::fromLocalFile(tagDir.path())); }
    static void clearRegistry(ExplorerActions &a)
    {
        for (const Tag &t : a.registry().tags())
            a.registry().remove(t);
    }

private Q_SLOTS:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }
    void addTagWritesXattrAndRegistry()
    {
        if (tagsUnsupported())
            QSKIP("xattr tags not supported here");
        ExplorerActions a(nullptr);
        clearRegistry(a);
        const QUrl u = tagFile(QStringLiteral("a"));
        a.addTag({KFileItem(u)}, T("Red/x"));
        QCOMPARE(TagStore::read(u), QList<Tag>{T("Red/x")});
        QCOMPARE(a.registry().tags(), QList<Tag>{T("Red/x")});
    }
    void fourthTagShowsError()
    {
        if (tagsUnsupported())
            QSKIP("xattr tags not supported here");
        ExplorerActions a(nullptr);
        clearRegistry(a);
        QStringList errors;
        a.setShowError([&](const QString &t) { errors << t; });
        const QUrl u = tagFile(QStringLiteral("b"));
        for (const char *t : {"Red/1", "Gold/2", "Green/3"})
            a.addTag({KFileItem(u)}, T(t));
        QVERIFY(errors.isEmpty());
        a.addTag({KFileItem(u)}, T("Blue/4"));
        QCOMPARE(errors, QStringList{TagStore::errorText(TagStore::Error::TooMany)});
        QCOMPARE(TagStore::read(u).size(), 3);
        QVERIFY(!a.registry().contains(T("Blue/4")));
    }
    void tagsMenuListsRecentFirstAndChecks()
    {
        if (tagsUnsupported())
            QSKIP("xattr tags not supported here");
        ExplorerActions a(nullptr);
        clearRegistry(a);
        a.registry().touch(T("Red/old"));
        a.registry().touch(T("Gold/new"));
        const QUrl u = tagFile(QStringLiteral("c"));
        a.addTag({KFileItem(u)}, T("Red/old")); // touches Red/old again: now most recent
        std::unique_ptr<QMenu> m(a.tagsMenu({KFileItem(u)}));
        const QList<QAction *> acts = m->actions();
        QCOMPARE(acts[0]->text(), QStringLiteral("old"));
        QCOMPARE(acts[1]->text(), QStringLiteral("new"));
        QVERIFY(acts[0]->isCheckable() && acts[0]->isChecked());
        QVERIFY(!acts[1]->isChecked());
        QVERIFY(acts[2]->isSeparator());
        QCOMPARE(acts.size(), 5); // New tag…, Manage tags…
        acts[1]->trigger(); // toggles on
        QCOMPARE(TagStore::read(u).size(), 2);
        acts[0]->trigger(); // toggles off
        QCOMPARE(TagStore::read(u), QList<Tag>{T("Gold/new")});
    }
    void tagsMenuOnUnsupportedDriveIsOneDisabledEntry()
    {
        ExplorerActions a(nullptr);
        std::unique_ptr<QMenu> m(a.tagsMenu({KFileItem(QUrl(QStringLiteral("http://example.org/x")))}));
        QCOMPARE(m->actions().size(), 1);
        QVERIFY(!m->actions().first()->isEnabled());
        QCOMPARE(m->actions().first()->text(), TagStore::errorText(TagStore::Error::Unsupported));
    }
    void tagsMenuGreysOutAtThree()
    {
        if (tagsUnsupported())
            QSKIP("xattr tags not supported here");
        ExplorerActions a(nullptr);
        clearRegistry(a);
        a.registry().touch(T("Blue/other"));
        const QUrl u = tagFile(QStringLiteral("full"));
        for (const char *t : {"Red/1", "Gold/2", "Green/3"})
            a.addTag({KFileItem(u)}, T(t));
        std::unique_ptr<QMenu> m(a.tagsMenu({KFileItem(u)}));
        int enabledChecked = 0, disabledUnchecked = 0;
        QAction *newTag = nullptr;
        for (QAction *act : m->actions()) {
            if (act->isCheckable())
                (act->isChecked() ? enabledChecked : disabledUnchecked) += act->isEnabled() == act->isChecked();
            if (act->text().startsWith(QLatin1String("New tag")))
                newTag = act;
        }
        QCOMPARE(enabledChecked, 3);
        QCOMPARE(disabledUnchecked, 1);
        QVERIFY(newTag && !newTag->isEnabled());
        QVERIFY(newTag->text().contains(QLatin1String("max 3 tags")));
    }
    void removeMenuUnsupportedSaysSo()
    {
        ExplorerActions a(nullptr);
        QMenu m;
        a.fillRemoveMenu(&m, {KFileItem(QUrl(QStringLiteral("http://example.org/x")))});
        QCOMPARE(m.actions().size(), 1);
        QVERIFY(!m.actions().first()->isEnabled());
        QCOMPARE(m.actions().first()->text(), TagStore::errorText(TagStore::Error::Unsupported));
    }
    void removeAllKeepsForeignTag()
    {
        if (tagsUnsupported())
            QSKIP("xattr tags not supported here");
        ExplorerActions a(nullptr);
        const QUrl u = tagFile(QStringLiteral("foreign"));
        a.addTag({KFileItem(u)}, T("Red/x"));
        KFileMetaData::UserMetaData md(u.toLocalFile());
        QVERIFY(md.setTags(md.tags() << QStringLiteral("plainforeign")) == KFileMetaData::UserMetaData::NoError);
        QVERIFY(TagStore::read(u).last().colour < 0);
        QMenu m;
        a.fillRemoveMenu(&m, {KFileItem(u)});
        QCOMPARE(m.actions().size(), 3); // Red/x, separator, All tags
        m.actions().last()->trigger();
        QCOMPARE(TagStore::read(u), QList<Tag>{T("plainforeign")});
    }
    void contextMenuHasTagsSubmenu()
    {
        ExplorerActions a(nullptr);
        std::unique_ptr<QMenu> m(a.contextMenu({KFileItem(QUrl::fromLocalFile(tagDir.filePath(QStringLiteral("x"))))}, QUrl::fromLocalFile(tagDir.path())));
        int found = 0;
        for (QAction *act : m->actions())
            if (act->menu() && act->text() == QLatin1String("Tags"))
                ++found;
        QCOMPARE(found, 1);
    }
    void removeTagClearsIt()
    {
        if (tagsUnsupported())
            QSKIP("xattr tags not supported here");
        ExplorerActions a(nullptr);
        const QUrl u = tagFile(QStringLiteral("d"));
        a.addTag({KFileItem(u)}, T("Red/x"));
        a.addTag({KFileItem(u)}, T("Gold/y"));
        a.removeTag({KFileItem(u)}, T("Red/x"));
        QCOMPARE(TagStore::read(u), QList<Tag>{T("Gold/y")});
    }
    void newTagDialogSeamAddsTag()
    {
        if (tagsUnsupported())
            QSKIP("xattr tags not supported here");
        ExplorerActions a(nullptr);
        a.setExecNewTagDialog([](NewTagDialog &d) {
            d.setTag(Tag::decode(QStringLiteral("Green/seam")));
            return true;
        });
        const QUrl u = tagFile(QStringLiteral("e"));
        a.newTag({KFileItem(u)});
        QCOMPARE(TagStore::read(u), QList<Tag>{T("Green/seam")});
    }
    void selectByRuleAddTagOpensNewTagDialog()
    {
        // ponytail: the dialog's own accept path is covered by selectbyruledialog_test; here only the dispatch
        QVERIFY(Ribbon::actionIds().contains(QStringLiteral("adv_tag_matches")));
        ExplorerActions a(nullptr);
        QVERIFY(a.action(QStringLiteral("adv_tag_matches")));
    }
    void newTagDialogNeedsAColour()
    {
        NewTagDialog d(nullptr);
        auto *ok = d.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
        QVERIFY(!ok->isEnabled());
        d.setTag(T("Red/w"));
        QVERIFY(ok->isEnabled());
        QCOMPARE(d.tag(), T("Red/w"));
    }
    void everySpecIdHasAnAction()
    {
        ExplorerActions actions(nullptr);
        const QStringList menuIds = {QStringLiteral("home_history"), QStringLiteral("view_sort"), QStringLiteral("view_group"),
                                     QStringLiteral("view_columns"), QStringLiteral("home_new_item"), QStringLiteral("home_delete"),
                                     QStringLiteral("home_open"), QStringLiteral("share_share"), QStringLiteral("tags_add"),
                                     QStringLiteral("tags_remove")};
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
        QCOMPARE(win12Entries(plain.get()).size(), 1); // always has "Select by rule…"
        QCOMPARE(win12Entries(plain.get()).first()->menu()->actions().size(), 2); // Select by rule + Rename many

        connect(&actions, &ExplorerActions::fillWin12Menu, this, [](QMenu *m, const KFileItemList &) {
            m->addAction(QStringLiteral("Tag"));
        });
        std::unique_ptr<QMenu> filled(actions.contextMenu(items, folder));
        const QList<QAction *> win12 = win12Entries(filled.get());
        QCOMPARE(win12.size(), 1);
        QVERIFY(win12.first()->menu());
        QCOMPARE(win12.first()->menu()->actions().size(), 3);
    }

    void win12MenuHasSelectByRule()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath(QStringLiteral("a.txt")), "a");
        const KFileItemList items = {itemFor(dir.filePath(QStringLiteral("a.txt")))};
        const QUrl folder = QUrl::fromLocalFile(dir.path());
        ExplorerActions actions(nullptr);
        for (const KFileItemList &list : {items, KFileItemList()}) {
            std::unique_ptr<QMenu> menu(actions.contextMenu(list, folder));
            const QList<QAction *> win12 = win12Entries(menu.get());
            QCOMPARE(win12.size(), 1);
            QStringList texts;
            for (QAction *a : win12.first()->menu()->actions())
                texts << a->text();
            QVERIFY2(texts.contains(QStringLiteral("Select by rule…")), qPrintable(texts.join(QLatin1Char('|'))));
        }
    }

    void win12MenuHasRenameMany()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath(QStringLiteral("a.txt")), "a");
        const KFileItemList items = {itemFor(dir.filePath(QStringLiteral("a.txt")))};
        const QUrl folder = QUrl::fromLocalFile(dir.path());
        ExplorerActions actions(nullptr);
        const auto has = [&](const KFileItemList &list) {
            std::unique_ptr<QMenu> menu(actions.contextMenu(list, folder));
            const QList<QAction *> acts = win12Entries(menu.get()).first()->menu()->actions();
            for (QAction *a : acts)
                if (a->text() == QStringLiteral("Rename many…"))
                    return true;
            return false;
        };
        QVERIFY(has(items));
        QVERIFY(!has(KFileItemList()));
    }

    void renameManyRunsPlanAndUndoRestores()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath(QStringLiteral("a.txt")), "a");
        writeFile(dir.filePath(QStringLiteral("b.txt")), "b");
        ExplorerActions actions(nullptr);
        actions.setExecRenameDialog([](RenameManyDialog &d) {
            RenameBlock n, t, c;
            n.kind = RenameBlock::Number;
            t.kind = RenameBlock::Text;
            t.text = QStringLiteral(" - ");
            c.kind = RenameBlock::CurrentName;
            d.setBlocks({n, t, c});
            return true;
        });
        QVERIFY(!actions.action(QStringLiteral("adv_rename_undo"))->isEnabled());
        actions.renameMany({itemFor(dir.filePath(QStringLiteral("a.txt"))), itemFor(dir.filePath(QStringLiteral("b.txt")))});
        QVERIFY(QFile::exists(dir.filePath(QStringLiteral("01 - a.txt"))));
        QVERIFY(QFile::exists(dir.filePath(QStringLiteral("02 - b.txt"))));
        QVERIFY(actions.action(QStringLiteral("adv_rename_undo"))->isEnabled());
        actions.undoRenameBatch();
        QVERIFY(QFile::exists(dir.filePath(QStringLiteral("a.txt"))));
        QVERIFY(QFile::exists(dir.filePath(QStringLiteral("b.txt"))));
        QVERIFY(!actions.action(QStringLiteral("adv_rename_undo"))->isEnabled());
    }

    void undoRenameBatchRestoresNames()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath(QStringLiteral("a.txt")), "a");
        writeFile(dir.filePath(QStringLiteral("b.txt")), "b");
        writeFile(dir.filePath(QStringLiteral("02 - b.txt")), "other");
        ExplorerActions actions(nullptr);
        actions.setExecRenameDialog([](RenameManyDialog &d) {
            RenameBlock n, t, c;
            n.kind = RenameBlock::Number;
            t.kind = RenameBlock::Text;
            t.text = QStringLiteral(" - ");
            c.kind = RenameBlock::CurrentName;
            d.setBlocks({n, t, c});
            return true;
        });
        actions.renameMany({itemFor(dir.filePath(QStringLiteral("a.txt"))), itemFor(dir.filePath(QStringLiteral("b.txt")))});
        QVERIFY(QFile::exists(dir.filePath(QStringLiteral("01 - a.txt"))));
        QVERIFY(QFile::exists(dir.filePath(QStringLiteral("b.txt"))));
        actions.undoRenameBatch();
        QVERIFY(QFile::exists(dir.filePath(QStringLiteral("a.txt"))));
        QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("01 - a.txt"))));
        QCOMPARE(QFile(dir.filePath(QStringLiteral("02 - b.txt"))).size(), 5);
    }

    void undoRenameBatchKeepsRowsNotReversed()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath(QStringLiteral("a.txt")), "a");
        writeFile(dir.filePath(QStringLiteral("b.txt")), "b");
        ExplorerActions actions(nullptr);
        actions.setExecRenameDialog([](RenameManyDialog &d) {
            RenameBlock t, c;
            t.kind = RenameBlock::Text;
            t.text = QStringLiteral("x-");
            c.kind = RenameBlock::CurrentName;
            d.setBlocks({t, c});
            return true;
        });
        actions.renameMany({itemFor(dir.filePath(QStringLiteral("a.txt"))), itemFor(dir.filePath(QStringLiteral("b.txt")))});
        QVERIFY(QFile::exists(dir.filePath(QStringLiteral("x-a.txt"))) && QFile::exists(dir.filePath(QStringLiteral("x-b.txt"))));
        // undo walks from the back: b first. Make a.txt taken so the second reversal fails.
        writeFile(dir.filePath(QStringLiteral("a.txt")), "blocker");
        QTimer closer; // the error box is modal: close it as soon as it shows
        closer.setInterval(20);
        connect(&closer, &QTimer::timeout, &closer, [] {
            for (QWidget *w : QApplication::topLevelWidgets())
                if (w->isVisible() && w->inherits("QDialog"))
                    w->close();
        });
        closer.start();
        actions.undoRenameBatch();
        QVERIFY(QFile::exists(dir.filePath(QStringLiteral("b.txt")))); // reversed
        QVERIFY(QFile::exists(dir.filePath(QStringLiteral("x-a.txt")))); // not
        QVERIFY(actions.action(QStringLiteral("adv_rename_undo"))->isEnabled());
        QFile::remove(dir.filePath(QStringLiteral("a.txt")));
        actions.undoRenameBatch();
        QVERIFY(QFile::exists(dir.filePath(QStringLiteral("a.txt"))));
        QVERIFY(!actions.action(QStringLiteral("adv_rename_undo"))->isEnabled());
    }

    void withoutNestedDropsItemsInsideListedFolders()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath(QStringLiteral("a/x"));
        QDir(dir.path()).mkpath(QStringLiteral("ab"));
        writeFile(dir.filePath(QStringLiteral("a/x/f.txt")), "f");
        writeFile(dir.filePath(QStringLiteral("ab/g.txt")), "g");
        const KFileItemList in{itemFor(dir.filePath(QStringLiteral("a"))), itemFor(dir.filePath(QStringLiteral("a/x/f.txt"))),
                               itemFor(dir.filePath(QStringLiteral("a/x"))), itemFor(dir.filePath(QStringLiteral("ab/g.txt")))};
        const KFileItemList out = ExplorerActions::withoutNested(in);
        QCOMPARE(out.size(), 2);
        QCOMPARE(out.at(0).url(), in.at(0).url());
        QCOMPARE(out.at(1).url(), in.at(3).url()); // "ab" is not inside "a"
    }

    void copyPathsJoinsWithNewline()
    {
        QTemporaryDir dir;
        const QString p1 = dir.filePath(QStringLiteral("a.txt")), p2 = dir.filePath(QStringLiteral("b.txt"));
        writeFile(p1, "a");
        writeFile(p2, "b");
        ExplorerActions actions(nullptr);
        actions.copyPaths({itemFor(p1), itemFor(p2)});
        QCOMPARE(QApplication::clipboard()->text(), p1 + QLatin1Char('\n') + p2);
    }

    void transferKeepStructureCreatesSubfolders()
    {
        QTemporaryDir src, dest;
        QDir(src.path()).mkdir(QStringLiteral("sub"));
        writeFile(src.filePath(QStringLiteral("sub/b.txt")), "b");
        writeFile(src.filePath(QStringLiteral("a.txt")), "a");
        ExplorerActions actions(nullptr);
        const QUrl destUrl = QUrl::fromLocalFile(dest.path());
        actions.setPickFolder([destUrl](const QUrl &) { return destUrl; });
        QSignalSpy undo(KIO::FileUndoManager::self(), &KIO::FileUndoManager::undoAvailable);
        actions.transferTo({itemFor(src.filePath(QStringLiteral("sub/b.txt"))), itemFor(src.filePath(QStringLiteral("a.txt")))}, false,
                           QUrl::fromLocalFile(src.path()));
        QTRY_VERIFY(QFile::exists(dest.filePath(QStringLiteral("sub/b.txt"))));
        QTRY_VERIFY(QFile::exists(dest.filePath(QStringLiteral("a.txt"))));
        QTRY_VERIFY(KIO::FileUndoManager::self()->isUndoAvailable());
    }

    void transferFlatByDefault()
    {
        QTemporaryDir src, dest;
        QDir(src.path()).mkdir(QStringLiteral("sub"));
        writeFile(src.filePath(QStringLiteral("sub/b.txt")), "b");
        writeFile(src.filePath(QStringLiteral("a.txt")), "a");
        ExplorerActions actions(nullptr);
        const QUrl destUrl = QUrl::fromLocalFile(dest.path());
        actions.setPickFolder([destUrl](const QUrl &) { return destUrl; });
        actions.transferTo({itemFor(src.filePath(QStringLiteral("sub/b.txt"))), itemFor(src.filePath(QStringLiteral("a.txt")))}, false);
        QTRY_VERIFY(QFile::exists(dest.filePath(QStringLiteral("b.txt"))));
        QTRY_VERIFY(QFile::exists(dest.filePath(QStringLiteral("a.txt"))));
    }

    // The dialog is never answered by the test except by cancelling it: nothing may be deleted before then.
    void deleteForeverGoesThroughForceConfirmation()
    {
        QTemporaryDir dir;
        const QString p = dir.filePath(QStringLiteral("a.txt"));
        writeFile(p, "a");
        ExplorerActions actions(nullptr);
        QTimer::singleShot(50, [] {
            if (QWidget *w = QApplication::activeModalWidget())
                w->close();
        });
        actions.deletePermanently({itemFor(p)});
        QTest::qWait(150);
        QVERIFY(QFile::exists(p));
    }

    void advancedActionsEnabledWithFolder()
    {
        QTemporaryDir dir, configDir;
        ViewSettingsStore store(KSharedConfig::openConfig(configDir.filePath(QStringLiteral("rc")), KConfig::SimpleConfig));
        FolderView view(&store);
        ExplorerActions actions(nullptr);
        QAction *trash = actions.action(QStringLiteral("adv_trash_matches"));
        QVERIFY(!trash->isEnabled());
        QSignalSpy done(view.model()->folderModel()->lister(), &KCoreDirLister::listingDirCompleted);
        view.openUrl(QUrl::fromLocalFile(dir.path()));
        QVERIFY(done.count() > 0 || done.wait(5000));
        actions.setActiveView(&view);
        QVERIFY(trash->isEnabled());
        QVERIFY(actions.action(QStringLiteral("adv_select_rule"))->isEnabled());
        actions.setActiveView(nullptr);
        QVERIFY(!trash->isEnabled());
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

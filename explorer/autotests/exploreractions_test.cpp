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
#include <QPointer>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QApplication>
#include <QClipboard>
#include <KIO/FileUndoManager>
#include "renamemanydialog.h"
#include "newtagdialog.h"
#include "tagpicker.h"
#include <QWidgetAction>
#include <QLabel>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QLineEdit>
#include <memory>
#include "tagstore.h"
#include "scriptdialog.h"
#include "selectbyruledialog.h"
#include <KFileMetaData/UserMetaData>
#include <QStandardPaths>
#include <KPluginMetaData>
using namespace Qt::StringLiterals;

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

QStringList allTexts(QMenu *menu)
{
    QStringList t;
    for (QAction *a : menu->actions()) {
        t << a->text();
        if (a->menu())
            t << allTexts(a->menu());
    }
    return t;
}

QString servicesDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/kio/servicemenus");
}

void removeTestServiceMenus()
{
    QFile::remove(servicesDir() + QStringLiteral("/win12-servicemenu.desktop"));
    QFile::remove(servicesDir() + QStringLiteral("/other-app.desktop"));
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
    void cleanupTestCase() { removeTestServiceMenus(); }
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
        QVERIFY(acts[0]->isSeparator());
        QCOMPARE(acts[0]->text(), QStringLiteral("Your tags"));
        QCOMPARE(acts[1]->text(), QStringLiteral("old"));
        QCOMPARE(acts[2]->text(), QStringLiteral("new"));
        QVERIFY(acts[1]->isCheckable() && acts[1]->isChecked());
        QVERIFY(!acts[2]->isChecked());
        QVERIFY(acts[3]->isSeparator());
        QCOMPARE(acts[3]->text(), QStringLiteral("New tag"));
        QVERIFY(qobject_cast<QWidgetAction *>(acts[4]));
        QCOMPARE(acts.size(), 6); // + Manage tags…
        acts[2]->trigger(); // toggles on
        QCOMPARE(TagStore::read(u).size(), 2);
        acts[1]->trigger(); // toggles off
        QCOMPARE(TagStore::read(u), QList<Tag>{T("Gold/new")});
    }
    void tagsMenuPanelAddsTagOnColourClick()
    {
        if (tagsUnsupported())
            QSKIP("xattr tags not supported here");
        ExplorerActions a(nullptr);
        clearRegistry(a);
        const QUrl u = tagFile(QStringLiteral("panel"));
        std::unique_ptr<QMenu> m(a.tagsMenu({KFileItem(u)}));
        TagPicker *picker = nullptr;
        for (QAction *act : m->actions())
            if (auto *wa = qobject_cast<QWidgetAction *>(act))
                picker = wa->defaultWidget()->findChild<TagPicker *>();
        QVERIFY(picker);
        QTest::keyClicks(picker->wordEdit(), QStringLiteral("w"));
        picker->colourButton(3)->click();
        QCOMPARE(TagStore::read(u), QList<Tag>({Tag{3, QStringLiteral("w")}}));
        QVERIFY(a.registry().contains(Tag{3, QStringLiteral("w")}));
    }
    void tagsMenuHasNoNewTagDialogEntry()
    {
        if (tagsUnsupported())
            QSKIP("xattr tags not supported here");
        ExplorerActions a(nullptr);
        std::unique_ptr<QMenu> m(a.tagsMenu({KFileItem(tagFile(QStringLiteral("nodlg")))}));
        for (QAction *act : m->actions())
            QVERIFY(!act->text().startsWith(QStringLiteral("New tag…")));
    }
    void tagPickerRejectsCommaAndSlash()
    {
        TagPicker p;
        QTest::keyClicks(p.wordEdit(), QStringLiteral("a,b/c\"") );
        QCOMPARE(p.wordEdit()->text(), QStringLiteral("abc"));
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
        TagPicker *picker = nullptr;
        QLabel *label = nullptr;
        for (QAction *act : m->actions()) {
            if (act->isCheckable())
                (act->isChecked() ? enabledChecked : disabledUnchecked) += act->isEnabled() == act->isChecked();
            if (auto *wa = qobject_cast<QWidgetAction *>(act)) {
                picker = wa->defaultWidget()->findChild<TagPicker *>();
                label = wa->defaultWidget()->findChild<QLabel *>();
            }
        }
        QCOMPARE(enabledChecked, 3);
        QCOMPARE(disabledUnchecked, 1);
        QVERIFY(picker && !picker->isEnabled());
        QVERIFY(label && label->text() == QStringLiteral("max 3 tags"));
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
        const QStringList menuIds = {QStringLiteral("adv_my_rules"), QStringLiteral("adv_my_scripts"), QStringLiteral("home_history"), QStringLiteral("view_sort"), QStringLiteral("view_group"),
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

    void clearLibrary(ExplorerActions &a)
    {
        for (const SavedRule &r : a.library().all())
            a.library().remove(r.name, r.kind, nullptr);
    }
    void selectByFormulaOpensFormulaMode()
    {
        QTemporaryDir dir;
        ExplorerActions a(nullptr);
        QTemporaryDir cfg;
        ViewSettingsStore store(KSharedConfig::openConfig(cfg.filePath(QStringLiteral("rc")), KConfig::SimpleConfig));
        FolderView view(&store);
        QSignalSpy done(view.model()->folderModel()->lister(), &KCoreDirLister::listingDirCompleted);
        view.openUrl(QUrl::fromLocalFile(dir.path()));
        QVERIFY(done.count() > 0 || done.wait(5000));
        a.setActiveView(&view);
        int calls = 0;
        a.setExecSelectDialog([&](SelectByRuleDialog &d) {
            calls += d.mode() == SelectByRuleDialog::Mode::Formula && d.formula() == QStringLiteral("=ISFILE()");
            return false;
        });
        a.selectByFormula(QStringLiteral("=ISFILE()"));
        QCOMPARE(calls, 1);
    }
    void myRulesMenuListsByKind()
    {
        ExplorerActions a(nullptr);
        clearLibrary(a);
        QVERIFY(a.library().save({QStringLiteral("A"), QStringLiteral("=1"), SavedRule::Kind::Select}, nullptr));
        QVERIFY(a.library().save({QStringLiteral("B"), QStringLiteral("=2"), SavedRule::Kind::Rename}, nullptr));
        QVERIFY(a.library().save({QStringLiteral("C"), QStringLiteral("x"), SavedRule::Kind::Script}, nullptr));
        std::unique_ptr<QMenu> m(a.myRulesMenu({}));
        const auto acts = m->actions();
        QCOMPARE(acts.size(), 4);
        QCOMPARE(acts[0]->text(), QStringLiteral("A"));
        QCOMPARE(acts[1]->text(), QStringLiteral("B"));
        QVERIFY(acts[2]->isSeparator());
        QCOMPARE(acts[3]->text(), QStringLiteral("C"));
        QMenu *s = a.action(QStringLiteral("adv_my_scripts"))->menu(); // the ribbon button's menu
        QMetaObject::invokeMethod(s, "aboutToShow");
        QCOMPARE(s->actions().size(), 1);
        QCOMPARE(s->actions()[0]->text(), QStringLiteral("C"));
        clearLibrary(a);
    }
    void ruleNameAmpersandIsEscaped()
    {
        ExplorerActions a(nullptr);
        clearLibrary(a);
        QVERIFY(a.library().save({QStringLiteral("A&B"), QStringLiteral("=1"), SavedRule::Kind::Select}, nullptr));
        std::unique_ptr<QMenu> m(a.myRulesMenu({}));
        QCOMPARE(m->actions()[0]->text(), QStringLiteral("A&&B"));
        clearLibrary(a);
    }
    void myRulesMenuEmptyEntry()
    {
        ExplorerActions a(nullptr);
        clearLibrary(a);
        std::unique_ptr<QMenu> m(a.myRulesMenu({}));
        QCOMPARE(m->actions().size(), 1);
        QVERIFY(!m->actions()[0]->isEnabled());
    }
    void contextMenuHasNoWin12Submenu()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath(QStringLiteral("a.txt")), "a");
        const KFileItemList items = {itemFor(dir.filePath(QStringLiteral("a.txt")))};
        const QUrl folder = QUrl::fromLocalFile(dir.path());
        ExplorerActions a(nullptr);
        clearLibrary(a);
        QVERIFY(a.library().save({QStringLiteral("A"), QStringLiteral("=1"), SavedRule::Kind::Select}, nullptr));
        for (const KFileItemList &list : {items, KFileItemList()}) {
            std::unique_ptr<QMenu> m(a.contextMenu(list, folder));
            for (QAction *x : m->actions()) {
                QVERIFY(x->text() != QStringLiteral("Win12"));
                QVERIFY(x->text() != QStringLiteral("My rules"));
            }
        }
        { // a Rename rule with nothing selected says so
            QVERIFY(a.library().save({QStringLiteral("R"), QStringLiteral("=1"), SavedRule::Kind::Rename}, nullptr));
            QString msg;
            a.setShowError([&](const QString &t) { msg = t; });
            std::unique_ptr<QMenu> m(a.myRulesMenu({}));
            for (QAction *x : m->actions())
                if (x->text() == QStringLiteral("R"))
                    x->trigger();
            QVERIFY(!msg.isEmpty());
        }
        clearLibrary(a);
    }
    void contextMenuExcludesWin12ServiceMenus()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath(QStringLiteral("a.txt")), "a");
        const KFileItemList items = {itemFor(dir.filePath(QStringLiteral("a.txt")))};
        const QUrl folder = QUrl::fromLocalFile(dir.path());
        qInfo() << "servicemenus dir:" << servicesDir();
        QVERIFY(QDir().mkpath(servicesDir()));
        const QString win12 = servicesDir() + QStringLiteral("/win12-servicemenu.desktop");
        const QString other = servicesDir() + QStringLiteral("/other-app.desktop");
        removeTestServiceMenus();
        QVERIFY(QFile::copy(QFINDTESTDATA("../win12-servicemenu.desktop"), win12));
        writeFile(other, "[Desktop Entry]\nType=Service\nMimeType=all/allfiles;inode/directory;\nActions=zzz-other;\n\n"
                         "[Desktop Action zzz-other]\nName=Other app entry\nExec=true\n");
        const auto perms = QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner | QFileDevice::ReadGroup
            | QFileDevice::ExeGroup | QFileDevice::ReadOther | QFileDevice::ExeOther;
        QVERIFY(QFile::setPermissions(win12, perms));
        QVERIFY(QFile::setPermissions(other, perms));
        ExplorerActions a(nullptr);
        std::unique_ptr<QMenu> m(a.contextMenu(items, folder));
        const QStringList t = allTexts(m.get());
        removeTestServiceMenus();
        QVERIFY2(t.contains(QStringLiteral("Other app entry")), qPrintable(t.join(QLatin1Char('|'))));
        QVERIFY(!t.contains(QStringLiteral("Select by rule…")));
        QVERIFY(!t.contains(QStringLiteral("Rename many…")));
    }
    void contextMenuHidesForbiddenPlugins()
    {
        if (!KPluginMetaData::findPluginById(QStringLiteral("kf6/kfileitemaction"), QStringLiteral("compressfileitemaction")).isValid())
            QSKIP("Ark's compressfileitemaction is not installed");
        QTemporaryDir dir;
        writeFile(dir.filePath(QStringLiteral("a.txt")), "a");
        writeFile(dir.filePath(QStringLiteral("a.zip")), QByteArray("PK\x05\x06", 4) + QByteArray(18, '\0'));
        const QUrl folder = QUrl::fromLocalFile(dir.path());
        ExplorerActions a(nullptr);
        {
            std::unique_ptr<QMenu> m(a.contextMenu({itemFor(dir.filePath(QStringLiteral("a.txt")))}, folder));
            const QStringList t = allTexts(m.get());
            QVERIFY2(t.contains(QStringLiteral("Compress")), qPrintable(t.join(QLatin1Char('|'))));
            for (const QString &bad : {u"Assign Tags"_s, u"Activities"_s, u"Folder Colour"_s, u"Change Folder Icon"_s})
                QVERIFY2(!t.contains(bad), qPrintable(bad));
        }
        {
            std::unique_ptr<QMenu> m(a.contextMenu({itemFor(dir.filePath(QStringLiteral("a.zip")))}, folder));
            const QStringList t = allTexts(m.get());
            QVERIFY2(t.contains(QStringLiteral("Extract")), qPrintable(t.join(QLatin1Char('|'))));
        }
    }
    void openScriptLoadsText()
    {
        QTemporaryDir dir;
        ExplorerActions a(nullptr);
        QTemporaryDir cfg;
        ViewSettingsStore store(KSharedConfig::openConfig(cfg.filePath(QStringLiteral("rc")), KConfig::SimpleConfig));
        FolderView view(&store);
        QSignalSpy done(view.model()->folderModel()->lister(), &KCoreDirLister::listingDirCompleted);
        view.openUrl(QUrl::fromLocalFile(dir.path()));
        QVERIFY(done.count() > 0 || done.wait(5000));
        a.setActiveView(&view);
        QString got;
        a.setExecScriptDialog([&](ScriptDialog &d) { got = d.script(); });
        a.openScript(QStringLiteral("x = 1"));
        QCOMPARE(got, QStringLiteral("x = 1"));
    }

    void renameManyClashesWithHiddenFile()
    {
        QTemporaryDir dir, configDir;
        writeFile(dir.filePath(QStringLiteral("a.txt")), "a");
        writeFile(dir.filePath(QStringLiteral(".b.txt")), "b");
        ViewSettingsStore store(KSharedConfig::openConfig(configDir.filePath(QStringLiteral("rc")), KConfig::SimpleConfig));
        FolderView view(&store);
        ExplorerActions actions(nullptr);
        QSignalSpy done(view.model()->folderModel()->lister(), &KCoreDirLister::listingDirCompleted);
        view.openUrl(QUrl::fromLocalFile(dir.path()));
        QVERIFY(done.count() > 0 || done.wait(5000));
        actions.setActiveView(&view);
        PlannedRename::Status status = PlannedRename::Rename;
        actions.setExecRenameDialog([&](RenameManyDialog &d) {
            RenameBlock t;
            t.kind = RenameBlock::Text;
            t.text = QStringLiteral(".b");
            d.setBlocks({t});
            status = d.plan().first().status;
            return false;
        });
        actions.renameMany({itemFor(dir.filePath(QStringLiteral("a.txt")))});
        QCOMPARE(status, PlannedRename::ClashExisting);
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

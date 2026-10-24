#include "managetagsdialog.h"
#include "tagregistry.h"
#include "tagstore.h"
#include <QDialogButtonBox>
#include <QDir>
#include <QPushButton>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

class ManageTagsDialogTest : public QObject
{
    Q_OBJECT
    QTemporaryDir dir{QDir::homePath() + QStringLiteral("/.cache/managetags-XXXXXX")};
    QTemporaryDir cfg;
    static Tag T(const char *s) { return Tag::decode(QString::fromLatin1(s)); }
    QUrl file(const QString &n)
    {
        QFile f(dir.filePath(n));
        (void)f.open(QIODevice::WriteOnly);
        return QUrl::fromLocalFile(f.fileName());
    }
    QList<Tag> tagsOf(const QUrl &u) { return TagStore::read(u); }
private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QDir().mkpath(QDir::homePath() + QStringLiteral("/.cache"));
        if (!dir.isValid() || !TagStore::supported(QUrl::fromLocalFile(dir.path())))
            QSKIP("xattr tags not supported here");
    }
    void renameRewritesFilesAndRegistry()
    {
        TagRegistry reg(cfg.filePath(QStringLiteral("r1.json")));
        TagStore store;
        reg.touch(T("Red/x"));
        const QUrl a = file(QStringLiteral("a")), b = file(QStringLiteral("b"));
        store.add({a, b}, T("Red/x"));
        ManageTagsDialog d(&reg, &store, QUrl::fromLocalFile(dir.path()), nullptr);
        d.setEngineForTests(TagSearch::Engine::Walk);
        d.setExecNewTagDialog([](NewTagDialog &n) { // preset with the tag; the user retypes the name
            Tag t = n.tag();
            t.word = QStringLiteral("y");
            n.setTag(t);
            return true;
        });
        QSignalSpy spy(&d, &ManageTagsDialog::applied);
        d.findChild<QPushButton *>(QStringLiteral("rename"))->click();
        QVERIFY(spy.wait(5000));
        QCOMPARE(spy.value(0).value(0).toInt(), 2);
        QCOMPARE(tagsOf(a), QList<Tag>{T("Red/y")});
        QCOMPARE(tagsOf(b), QList<Tag>{T("Red/y")});
        QCOMPARE(reg.tags(), QList<Tag>{T("Red/y")});
    }
    void recolourKeepsWord()
    {
        TagRegistry reg(cfg.filePath(QStringLiteral("r2.json")));
        TagStore store;
        reg.touch(T("Red/k"));
        const QUrl a = file(QStringLiteral("c"));
        store.add({a}, T("Red/k"));
        ManageTagsDialog d(&reg, &store, QUrl::fromLocalFile(dir.path()), nullptr);
        d.setEngineForTests(TagSearch::Engine::Walk);
        QSignalSpy spy(&d, &ManageTagsDialog::applied);
        d.applyTo(T("Red/k"), T("Blue/k"));
        QVERIFY(spy.wait(5000));
        QCOMPARE(tagsOf(a), QList<Tag>{T("Blue/k")});
        QCOMPARE(reg.tags(), QList<Tag>{T("Blue/k")});
    }
    void deleteEverywhereRemovesTagKeepsOthers()
    {
        TagRegistry reg(cfg.filePath(QStringLiteral("r3.json")));
        TagStore store;
        reg.touch(T("Red/d"));
        reg.touch(T("Green/g"));
        const QUrl a = file(QStringLiteral("e"));
        store.add({a}, T("Red/d"));
        store.add({a}, T("Green/g"));
        ManageTagsDialog d(&reg, &store, QUrl::fromLocalFile(dir.path()), nullptr);
        d.setEngineForTests(TagSearch::Engine::Walk);
        QSignalSpy spy(&d, &ManageTagsDialog::applied);
        d.applyTo(T("Red/d"), std::nullopt);
        QVERIFY(spy.wait(5000));
        QCOMPARE(tagsOf(a), QList<Tag>{T("Green/g")});
        QCOMPARE(reg.tags(), QList<Tag>{T("Green/g")});
    }
    void closeDisabledWhileRunning()
    {
        TagRegistry reg(cfg.filePath(QStringLiteral("r5.json")));
        TagStore store;
        reg.touch(T("Red/q"));
        ManageTagsDialog d(&reg, &store, QUrl::fromLocalFile(dir.path()), nullptr);
        d.setEngineForTests(TagSearch::Engine::Walk);
        d.show();
        QSignalSpy spy(&d, &ManageTagsDialog::applied);
        d.applyTo(T("Red/q"), std::nullopt);
        QVERIFY(!d.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Close)->isEnabled());
        d.reject();
        QVERIFY(d.isVisible());
        QVERIFY(spy.wait(5000));
        QVERIFY(d.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Close)->isEnabled());
        d.reject();
        QVERIFY(!d.isVisible());
    }
    void deleteAsksConfirmation()
    {
        TagRegistry reg(cfg.filePath(QStringLiteral("r6.json")));
        TagStore store;
        reg.touch(T("Red/c"));
        ManageTagsDialog d(&reg, &store, QUrl::fromLocalFile(dir.path()), nullptr);
        d.setEngineForTests(TagSearch::Engine::Walk);
        bool ask = false, yes = false;
        d.setConfirmDelete([&](const Tag &) { ask = true; return yes; });
        auto *del = d.findChild<QPushButton *>(QStringLiteral("delete"));
        del->click();
        QVERIFY(ask);
        QVERIFY(reg.contains(T("Red/c"))); // declined
        yes = true;
        QSignalSpy spy(&d, &ManageTagsDialog::applied);
        del->click();
        QVERIFY(spy.wait(5000));
        QVERIFY(!reg.contains(T("Red/c")));
    }
    void renameOntoTagFileAlreadyHasMerges()
    {
        TagRegistry reg(cfg.filePath(QStringLiteral("r7.json")));
        TagStore store;
        reg.touch(T("Red/A"));
        const QUrl a = file(QStringLiteral("m"));
        store.add({a}, T("Red/A"));
        store.add({a}, T("Blue/B"));
        ManageTagsDialog d(&reg, &store, QUrl::fromLocalFile(dir.path()), nullptr);
        d.setEngineForTests(TagSearch::Engine::Walk);
        QSignalSpy spy(&d, &ManageTagsDialog::applied);
        d.applyTo(T("Red/A"), T("Blue/B"));
        QVERIFY(spy.wait(5000));
        QCOMPARE(tagsOf(a), QList<Tag>{T("Blue/B")});
    }
    void tagsChangedOncePerBatch()
    {
        TagRegistry reg(cfg.filePath(QStringLiteral("r8.json")));
        TagStore store;
        reg.touch(T("Red/o"));
        const QUrl a = file(QStringLiteral("o1")), b = file(QStringLiteral("o2"));
        store.add({a, b}, T("Red/o"));
        ManageTagsDialog d(&reg, &store, QUrl::fromLocalFile(dir.path()), nullptr);
        d.setEngineForTests(TagSearch::Engine::Walk);
        QSignalSpy changed(&store, &TagStore::tagsChanged);
        QSignalSpy spy(&d, &ManageTagsDialog::applied);
        d.applyTo(T("Red/o"), std::nullopt);
        QVERIFY(spy.wait(5000));
        QCOMPARE(changed.count(), 1);
        QCOMPARE(changed.value(0).value(0).value<QList<QUrl>>().size(), 2);
    }
    void buttonsDisabledWhileRunning()
    {
        TagRegistry reg(cfg.filePath(QStringLiteral("r4.json")));
        TagStore store;
        reg.touch(T("Red/z"));
        file(QStringLiteral("f"));
        ManageTagsDialog d(&reg, &store, QUrl::fromLocalFile(dir.path()), nullptr);
        d.setEngineForTests(TagSearch::Engine::Walk);
        auto *del = d.findChild<QPushButton *>(QStringLiteral("delete"));
        QVERIFY(del->isEnabled());
        QSignalSpy spy(&d, &ManageTagsDialog::applied);
        d.applyTo(T("Red/z"), std::nullopt);
        QVERIFY(!del->isEnabled());
        QVERIFY(!d.findChild<QPushButton *>(QStringLiteral("rename"))->isEnabled());
        QVERIFY(spy.wait(5000));
        QVERIFY(!reg.contains(T("Red/z"))); // list empty, nothing to pick
        QVERIFY(!del->isEnabled());
    }
};
QTEST_MAIN(ManageTagsDialogTest)
#include "managetagsdialog_test.moc"

#include "managetagsdialog.h"
#include "tagregistry.h"
#include "tagstore.h"
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

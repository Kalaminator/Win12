#include "rulelibrary.h"
#include <KDesktopFile>
#include <KDesktopFileAction>
#include <KConfigGroup>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

using namespace Qt::StringLiterals;
using K = SavedRule::Kind;

class RuleLibraryTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }
    void init()
    {
        QFile::remove(RuleLibrary::path() + QStringLiteral(".broken"));
        QDir(RuleLibrary::path()).removeRecursively();
        QFile::remove(RuleLibrary::path());
    }
    void serviceMenuWrittenOnSave()
    {
        QFile::remove(RuleLibrary::serviceMenuPath());
        QVERIFY(RuleLibrary::serviceMenuPath().contains(QStringLiteral("/kio/servicemenus/win12-my-rules.desktop")));
        RuleLibrary l;
        QString err;
        QVERIFY(l.save({QStringLiteral("Old CED files"), QStringLiteral("=AND(1)"), K::Select}, &err));
        const QFileInfo fi(RuleLibrary::serviceMenuPath());
        QVERIFY(fi.exists());
        QVERIFY(fi.isExecutable());
        QFile f(fi.filePath());
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QString t = QString::fromUtf8(f.readAll());
        QVERIFY(t.contains(QString::fromUtf8("X-KDE-Submenu=Win12")));
        QVERIFY(t.contains(QStringLiteral("Actions=rule000;")));
        QVERIFY(t.contains(QStringLiteral("Exec=win12-explorer --rule \"Old CED files\" %U")));
    }
    void serviceActionKeysMatchFiles()
    {
        RuleLibrary l;
        QVERIFY(l.save({QStringLiteral("A"), QStringLiteral("=1"), K::Select}, nullptr));
        QVERIFY(l.save({QStringLiteral("B"), QStringLiteral("=1"), K::Select}, nullptr));
        const QStringList keys = l.serviceActionKeys();
        QCOMPARE(keys, (QStringList{u"a-select-by-rule"_s, u"b-rename-many"_s, u"rule000"_s, u"rule001"_s}));
        const KDesktopFile df(QStringLiteral(SERVICEMENU_FILE));
        QVERIFY(!df.actions().isEmpty());
        for (const KDesktopFileAction &act : df.actions())
            QVERIFY2(keys.contains(act.actionsKey()), qPrintable(act.actionsKey()));
        QVERIFY(l.serviceMenuText().contains(QStringLiteral("[Desktop Action rule001]")));
    }
    void serviceMenuEscapesPercent()
    {
        RuleLibrary l;
        QString err;
        QVERIFY(l.save({QStringLiteral("100% done"), QStringLiteral("1"), K::Script}, &err));
        QVERIFY(l.serviceMenuText().contains(QStringLiteral("--rule \"100%% done\"")));
    }
    void serviceMenuWrittenOnLoad()
    {
        QFile::remove(RuleLibrary::serviceMenuPath());
        QDir().mkpath(QFileInfo(RuleLibrary::path()).absolutePath());
        QFile f(RuleLibrary::path());
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(R"j({"version":1,"rules":[{"name":"Old","kind":"select","text":"=AND(1)"}]})j");
        f.close();
        RuleLibrary l;
        QCOMPARE(l.all().size(), 1);
        QVERIFY(QFile::exists(RuleLibrary::serviceMenuPath()));
    }
    void staticMenuSharesSubmenu()
    {
        KDesktopFile f(QStringLiteral(SERVICEMENU_FILE));
        QCOMPARE(f.desktopGroup().readEntry("X-KDE-Submenu"), QString(RuleLibrary::kSubmenu));
        const QStringList a = f.readActions();
        QCOMPARE(a.size(), 2);
        QVERIFY(f.actionGroup(a[0]).readEntry("Exec").endsWith(QStringLiteral("--select-by-rule %U")));
        QVERIFY(f.actionGroup(a[1]).readEntry("Exec").endsWith(QStringLiteral("--rename-many %U")));
        const QStringList mt = f.desktopGroup().readXdgListEntry("MimeType");
        QVERIFY(mt.contains(QStringLiteral("inode/directory")));
        QVERIFY(mt.contains(QStringLiteral("all/allfiles")));
    }
    void serviceMenuRemovedWhenEmpty()
    {
        RuleLibrary l;
        QString err;
        QVERIFY(l.save({QStringLiteral("x"), QStringLiteral("1"), K::Script}, &err));
        QVERIFY(QFile::exists(RuleLibrary::serviceMenuPath()));
        QVERIFY(l.remove(QStringLiteral("x"), K::Script, &err));
        QVERIFY(!QFile::exists(RuleLibrary::serviceMenuPath()));
    }
    void serviceMenuEscapesQuotes()
    {
        RuleLibrary l;
        QString err;
        QVERIFY(l.save({QStringLiteral("a\"b"), QStringLiteral("1"), K::Script}, &err));
        // file text doubles the backslash of the quoting-level \" (desktop-entry unescape runs first)
        QVERIFY(l.serviceMenuText().contains(QStringLiteral("Exec=win12-explorer --rule \"a\\\\\"b\" %U")));
        QVERIFY(l.remove(QStringLiteral("a\"b"), K::Script, &err));
        QVERIFY(l.save({QStringLiteral("p$`\\%q\nr"), QStringLiteral("1"), K::Script}, &err));
        QVERIFY(l.serviceMenuText().contains(QStringLiteral("Exec=win12-explorer --rule \"p\\\\$\\\\`\\\\\\\\%%qr\" %U")));
        QVERIFY(l.serviceMenuText().contains(QStringLiteral("Name=p$`\\\\%qr\n")));
    }
    void saveThenAllRoundTrips()
    {
        QString err;
        {
            RuleLibrary l;
            QVERIFY(l.save({QStringLiteral("Zed"), QStringLiteral("print 1"), K::Script}, &err));
            QVERIFY(l.save({QStringLiteral("b rename"), QStringLiteral("=A1"), K::Rename}, &err));
            QVERIFY(l.save({QStringLiteral("Old CED files"), QStringLiteral("=AND(1)"), K::Select}, &err));
        }
        RuleLibrary r;
        const QList<SavedRule> all = r.all();
        QCOMPARE(all.size(), 3);
        QCOMPARE(all[0].name, QStringLiteral("Old CED files"));
        QCOMPARE(all[0].kind, K::Select);
        QCOMPARE(all[1].kind, K::Rename);
        QCOMPARE(all[2].kind, K::Script);
        QCOMPARE(all[2].text, QStringLiteral("print 1"));
        QCOMPARE(r.ofKind(K::Rename).size(), 1);
    }
    void saveReplacesSameNameAndKind()
    {
        RuleLibrary l;
        QString err;
        QVERIFY(l.save({QStringLiteral("x"), QStringLiteral("old"), K::Select}, &err));
        QVERIFY(l.save({QStringLiteral("x"), QStringLiteral("new"), K::Select}, &err));
        QCOMPARE(l.all().size(), 1);
        QCOMPARE(l.all()[0].text, QStringLiteral("new"));
        QVERIFY(l.save({QStringLiteral("y"), QStringLiteral("o"), K::Rename}, &err));
        QCOMPARE(l.all().size(), 2);
    }
    void removeByNameAndKind()
    {
        RuleLibrary l;
        QString err;
        l.save({QStringLiteral("x"), QStringLiteral("a"), K::Select}, &err);
        l.save({QStringLiteral("y"), QStringLiteral("b"), K::Rename}, &err);
        QVERIFY(l.remove(QStringLiteral("x"), K::Select, &err));
        QCOMPARE(l.all().size(), 1);
        QCOMPARE(l.all()[0].kind, K::Rename);
        RuleLibrary r;
        QCOMPARE(r.all().size(), 1);
    }
    void emptyNameRefused()
    {
        RuleLibrary l;
        QSignalSpy spy(&l, &RuleLibrary::changed);
        QString err;
        QVERIFY(!l.save({QString(), QStringLiteral("a"), K::Select}, &err));
        QCOMPARE(err, QStringLiteral("A name is needed"));
        QCOMPARE(spy.count(), 0);
    }
    void unreadableFileIsReported()
    {
        QVERIFY(QDir().mkpath(RuleLibrary::path()));
        RuleLibrary l;
        QString err;
        QVERIFY(!l.save({QStringLiteral("x"), QStringLiteral("a"), K::Select}, &err));
        QVERIFY(!err.isEmpty());
    }
    void garbageFileIsMovedAside()
    {
        QVERIFY(QDir().mkpath(QFileInfo(RuleLibrary::path()).absolutePath()));
        QFile f(RuleLibrary::path());
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("not json {");
        f.close();
        RuleLibrary l;
        QString err;
        QVERIFY(l.save({QStringLiteral("x"), QStringLiteral("a"), K::Select}, &err));
        QFile b(RuleLibrary::path() + QStringLiteral(".broken"));
        QVERIFY(b.open(QIODevice::ReadOnly));
        QCOMPARE(b.readAll(), QByteArray("not json {"));
        QCOMPARE(RuleLibrary().all().size(), 1);
    }
    void changedFiresOnlyAfterWrite()
    {
        RuleLibrary l;
        QSignalSpy spy(&l, &RuleLibrary::changed);
        QString err;
        QVERIFY(l.save({QStringLiteral("x"), QStringLiteral("a"), K::Select}, &err));
        QCOMPARE(spy.count(), 1);
        QVERIFY(l.remove(QStringLiteral("x"), K::Select, &err));
        QCOMPARE(spy.count(), 2);
    }
    void saveRefusesNameOfOtherKind()
    {
        RuleLibrary l;
        QString err;
        QVERIFY(l.save({QStringLiteral("x"), QStringLiteral("1"), K::Select}, &err));
        QVERIFY(!l.save({QStringLiteral("x"), QStringLiteral("2"), K::Script}, &err));
        QVERIFY(err.contains(QStringLiteral("x")));
        QCOMPARE(l.all().size(), 1);
    }
    void saveReplacesSameKind()
    {
        RuleLibrary l;
        QString err;
        QVERIFY(l.save({QStringLiteral("x"), QStringLiteral("1"), K::Select}, &err));
        QVERIFY(l.save({QStringLiteral("x"), QStringLiteral("2"), K::Select}, &err));
        QCOMPARE(l.all().size(), 1);
        QCOMPARE(l.all().first().text, QStringLiteral("2"));
    }
    void findByName()
    {
        RuleLibrary l;
        QString err;
        QVERIFY(l.save({QStringLiteral("x"), QStringLiteral("1"), K::Select}, &err));
        QVERIFY(l.find(QStringLiteral("x")).has_value());
        QCOMPARE(l.find(QStringLiteral("x"))->kind, K::Select);
        QVERIFY(!l.find(QStringLiteral("nope")).has_value());
    }
};

QTEST_MAIN(RuleLibraryTest)
#include "rulelibrary_test.moc"

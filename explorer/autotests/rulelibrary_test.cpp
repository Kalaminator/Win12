#include "rulelibrary.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

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
        QVERIFY(l.save({QStringLiteral("x"), QStringLiteral("o"), K::Rename}, &err));
        QCOMPARE(l.all().size(), 2);
    }
    void removeByNameAndKind()
    {
        RuleLibrary l;
        QString err;
        l.save({QStringLiteral("x"), QStringLiteral("a"), K::Select}, &err);
        l.save({QStringLiteral("x"), QStringLiteral("b"), K::Rename}, &err);
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
};

QTEST_MAIN(RuleLibraryTest)
#include "rulelibrary_test.moc"

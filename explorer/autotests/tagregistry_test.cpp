#include "tagregistry.h"
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

class TagRegistryTest : public QObject
{
    Q_OBJECT
    QTemporaryDir dir;
    QString path() const { return dir.path() + QStringLiteral("/sub/tags.json"); }
private Q_SLOTS:
    void init() { QFile::remove(path()); }
    void emptyWhenMissing()
    {
        TagRegistry r(path());
        QVERIFY(r.tags().isEmpty());
    }
    void touchSavesAndReloads()
    {
        const Tag t{0, QStringLiteral("Xiaomi")};
        {
            TagRegistry r(path());
            r.touch(t);
        }
        TagRegistry r2(path());
        QCOMPARE(r2.tags(), QList<Tag>{t});
        QVERIFY(r2.contains(t));
    }
    void touchMovesToFront()
    {
        TagRegistry r(path());
        const Tag a{1, QStringLiteral("a")}, b{2, QStringLiteral("b")};
        r.touch(a);
        r.touch(b);
        QCOMPARE(r.tags(), (QList<Tag>{b, a}));
        r.touch(a);
        QCOMPARE(r.tags(), (QList<Tag>{a, b}));
    }
    void foreignNeverStored()
    {
        TagRegistry r(path());
        QSignalSpy spy(&r, &TagRegistry::changed);
        r.touch(Tag{-1, QStringLiteral("work")});
        QVERIFY(r.tags().isEmpty());
        QCOMPARE(spy.count(), 0);
    }
    void renameRecolourRemove()
    {
        TagRegistry r(path());
        r.touch(Tag{3, QStringLiteral("a")});
        r.rename(Tag{3, QStringLiteral("a")}, QStringLiteral("b"));
        QCOMPARE(r.tags(), QList<Tag>{(Tag{3, QStringLiteral("b")})});
        r.recolour(Tag{3, QStringLiteral("b")}, 5);
        QCOMPARE(r.tags(), QList<Tag>{(Tag{5, QStringLiteral("b")})});
        r.remove(Tag{5, QStringLiteral("b")});
        QVERIFY(r.tags().isEmpty());
        TagRegistry r2(path());
        QVERIFY(r2.tags().isEmpty());
    }
    void renameOntoExistingMerges()
    {
        TagRegistry r(path());
        r.touch(Tag{3, QStringLiteral("a")});
        r.touch(Tag{3, QStringLiteral("b")});
        r.rename(Tag{3, QStringLiteral("a")}, QStringLiteral("b"));
        QCOMPARE(r.tags(), QList<Tag>{(Tag{3, QStringLiteral("b")})});
    }
    void recolourOntoExistingMerges()
    {
        TagRegistry r(path());
        r.touch(Tag{3, QStringLiteral("a")});
        r.touch(Tag{4, QStringLiteral("a")});
        r.recolour(Tag{3, QStringLiteral("a")}, 4);
        QCOMPARE(r.tags(), QList<Tag>{(Tag{4, QStringLiteral("a")})});
    }
    void unknownColourDropped()
    {
        QDir().mkpath(dir.path() + QStringLiteral("/sub"));
        QFile f(path());
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(R"({"version":1,"tags":[{"colour":"Mauve","word":"x","used":"2026-10-24T10:00:00Z"},{"colour":"Red","word":"y","used":"2026-10-24T10:00:00Z"}]})");
        f.close();
        TagRegistry r(path());
        QCOMPARE(r.tags().size(), 1);
        QCOMPARE(r.tags().first().word, QStringLiteral("y"));
    }
    void changedEmittedOnce()
    {
        TagRegistry r(path());
        QSignalSpy spy(&r, &TagRegistry::changed);
        const Tag t{4, QStringLiteral("a")};
        r.touch(t);
        QCOMPARE(spy.count(), 1);
        r.rename(t, QStringLiteral("b"));
        QCOMPARE(spy.count(), 2);
        r.recolour(Tag{4, QStringLiteral("b")}, 6);
        QCOMPARE(spy.count(), 3);
        r.remove(Tag{6, QStringLiteral("b")});
        QCOMPARE(spy.count(), 4);
    }
    void unwritableSaysSoAndStaysQuiet()
    {
        QFile blocker(dir.path() + QStringLiteral("/blocker"));
        QVERIFY(blocker.open(QIODevice::WriteOnly)); // a file where the registry's folder should be
        TagRegistry r(blocker.fileName() + QStringLiteral("/tags.json"));
        QSignalSpy spy(&r, &TagRegistry::changed);
        const Tag t{4, QStringLiteral("a")};
        QVERIFY(!r.touch(t));
        QVERIFY(!r.rename(t, QStringLiteral("b")));
        QCOMPARE(spy.count(), 0);
        QVERIFY(r.saveErrorText().contains(blocker.fileName()));
        TagRegistry ok(path());
        QVERIFY(ok.touch(t));
        QVERIFY(ok.rename(Tag{9, QStringLiteral("absent")}, QStringLiteral("x"))); // nothing to do is no failure
    }
};

QTEST_MAIN(TagRegistryTest)
#include "tagregistry_test.moc"

#include "tagsearch.h"
#include "tagstore.h"
#include <Baloo/IndexerConfig>
#include <KFileMetaData/UserMetaData>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using KFileMetaData::UserMetaData;

class TagSearchTest : public QObject
{
    Q_OBJECT
    QTemporaryDir dir{QDir::homePath() + QStringLiteral("/.cache/tagsearch-XXXXXX")};
    const Tag red = Tag::decode(QStringLiteral("Red/Xiaomi"));
    const Tag orange = Tag::decode(QStringLiteral("OrangeBright/Urgent"));

    QString file(const QString &rel, const QStringList &tags)
    {
        const QString p = dir.filePath(rel);
        QDir().mkpath(QFileInfo(p).path());
        QFile f(p);
        (void)f.open(QIODevice::WriteOnly);
        f.close();
        if (!tags.isEmpty())
            UserMetaData(p).setTags(tags);
        return p;
    }
    // Runs a walk to the end; returns the found paths, sorted.
    QStringList walk(const QString &folder, const QList<Tag> &tags, TagSearch::Mode mode, int *batches = nullptr)
    {
        TagSearch s;
        QStringList paths;
        int n = 0;
        connect(&s, &TagSearch::found, this, [&](const KFileItemList &items) {
            ++n;
            for (const KFileItem &i : items)
                paths << QDir(dir.path()).relativeFilePath(i.url().toLocalFile());
        });
        QSignalSpy done(&s, &TagSearch::finished);
        s.start(QUrl::fromLocalFile(folder), tags, mode, TagSearch::Engine::Walk);
        [&] { QVERIFY(done.wait(10000)); }();
        if (!done.isEmpty()) {
            [&] { QCOMPARE(done.first().at(0).toBool(), true); }();
            [&] { QCOMPARE(done.first().at(2).value<TagSearch::Engine>(), TagSearch::Engine::Walk); }();
        }
        if (batches)
            *batches = n;
        paths.sort();
        return paths;
    }

private Q_SLOTS:
    void initTestCase()
    {
        QDir().mkpath(QDir::homePath() + QStringLiteral("/.cache"));
        if (!dir.isValid() || !TagStore::supported(QUrl::fromLocalFile(dir.path())))
            QSKIP("xattr tags not supported here");
        file(QStringLiteral("tree/a.txt"), {red.encode()});
        file(QStringLiteral("tree/sub/b.txt"), {red.encode(), orange.encode()});
        file(QStringLiteral("tree/c.txt"), {});
    }
    void walkAnyFindsBoth()
    {
        QCOMPARE(walk(dir.filePath(QStringLiteral("tree")), {red, orange}, TagSearch::Mode::Any),
                 (QStringList{QStringLiteral("tree/a.txt"), QStringLiteral("tree/sub/b.txt")}));
    }
    void walkAllFindsNested()
    {
        QCOMPARE(walk(dir.filePath(QStringLiteral("tree")), {red, orange}, TagSearch::Mode::All),
                 QStringList{QStringLiteral("tree/sub/b.txt")});
    }
    void batchesNotPerFile()
    {
        for (int i = 0; i < 300; ++i)
            file(QStringLiteral("many/f%1").arg(i), {red.encode()});
        int batches = 0;
        QCOMPARE(walk(dir.filePath(QStringLiteral("many")), {red}, TagSearch::Mode::Any, &batches).size(), 300);
        QVERIFY2(batches >= 1 && batches <= 3, qPrintable(QString::number(batches)));
    }
    void stopEndsWithoutFinished()
    {
        TagSearch s;
        QSignalSpy done(&s, &TagSearch::finished);
        QSignalSpy found(&s, &TagSearch::found);
        s.start(QUrl::fromLocalFile(dir.path()), {red}, TagSearch::Mode::Any, TagSearch::Engine::Walk);
        QVERIFY(s.isRunning());
        s.stop();
        QVERIFY(!s.isRunning());
        QTest::qWait(300);
        QCOMPARE(done.count(), 0);
        QCOMPARE(found.count(), 0);
    }
    void stopThenDestroyIsSafe()
    {
        auto *s = new TagSearch;
        s->start(QUrl::fromLocalFile(dir.path()), {red}, TagSearch::Mode::Any, TagSearch::Engine::Walk);
        s->stop();
        delete s;
        auto *t = new TagSearch;
        t->start(QUrl::fromLocalFile(dir.path()), {red}, TagSearch::Mode::Any, TagSearch::Engine::Walk);
        delete t; // destroyed while running
        QTest::qWait(300);
    }
    void restartDropsOldRun()
    {
        TagSearch s;
        QSignalSpy done(&s, &TagSearch::finished);
        s.start(QUrl::fromLocalFile(dir.path()), {red}, TagSearch::Mode::Any, TagSearch::Engine::Walk);
        s.start(QUrl::fromLocalFile(dir.filePath(QStringLiteral("tree"))), {orange}, TagSearch::Mode::Any, TagSearch::Engine::Walk);
        QVERIFY(done.wait(10000));
        QTest::qWait(300);
        QCOMPARE(done.count(), 1);
    }
    void missingFolderFinishesWithError()
    {
        TagSearch s;
        QSignalSpy done(&s, &TagSearch::finished);
        s.start(QUrl::fromLocalFile(dir.filePath(QStringLiteral("nope"))), {red}, TagSearch::Mode::Any, TagSearch::Engine::Walk);
        QVERIFY(done.wait(5000));
        QCOMPARE(done.first().at(0).toBool(), false);
        QVERIFY(!done.first().at(1).toString().isEmpty());
        QVERIFY(!s.isRunning());
    }
    void matchesIsExact()
    {
        using M = TagSearch::Mode;
        const QStringList wantRed = encodeAll({Tag::decode(QStringLiteral("Red"))});
        const QStringList have = {QStringLiteral("Red/Xiaomi")};
        QVERIFY(!TagSearch::matches(have, wantRed, M::Any)); // colour-only Red is not Red/Xiaomi
        QVERIFY(!TagSearch::matches(have, wantRed, M::All));
        QVERIFY(!TagSearch::matches({QStringLiteral("red/xiaomi")}, encodeAll({red}), M::Any)); // foreign, lower-case
        QVERIFY(TagSearch::matches(have, encodeAll({red}), M::All));
        QVERIFY(!TagSearch::matches({}, encodeAll({red}), M::Any));
        const QStringList both = encodeAll({red, orange});
        QVERIFY(TagSearch::matches(have, both, M::Any));
        QVERIFY(!TagSearch::matches(have, both, M::All));
        QVERIFY(TagSearch::matches({both.first(), both.last()}, both, M::All));
    }
    void engineForTmpIsWalk()
    {
        Baloo::IndexerConfig c;
        if (c.fileIndexingEnabled() && c.shouldBeIndexed(QStringLiteral("/tmp")))
            QSKIP("this machine indexes /tmp");
        QCOMPARE(TagSearch::engineFor(QUrl::fromLocalFile(QStringLiteral("/tmp"))), TagSearch::Engine::Walk);
        QCOMPARE(TagSearch::engineFor(QUrl(QStringLiteral("smb://host/share"))), TagSearch::Engine::Walk);
    }
};

QTEST_GUILESS_MAIN(TagSearchTest)
#include "tagsearch_test.moc"

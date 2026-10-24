#include "cli.h"
#include <QTest>

class CliTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void flagsAndUrls()
    {
        QVERIFY(parseCli({QStringLiteral("--daemon")}, QStringLiteral("/x")).daemon);
        QVERIFY(parseCli({QStringLiteral("--daemon")}, QStringLiteral("/x")).urls.isEmpty());
        const CliOptions s = parseCli({QStringLiteral("--select"), QStringLiteral("/tmp/a b.txt")}, QStringLiteral("/x"));
        QVERIFY(s.select);
        QCOMPARE(s.urls, QList<QUrl>{QUrl(QStringLiteral("file:///tmp/a%20b.txt"))});
        const CliOptions r = parseCli({QStringLiteral("docs"), QStringLiteral("smb://nas/s")}, QStringLiteral("/home/u"));
        QCOMPARE(r.urls, (QList<QUrl>{QUrl(QStringLiteral("file:///home/u/docs")), QUrl(QStringLiteral("smb://nas/s"))}));
        QCOMPARE(parseCli({QStringLiteral("--bogus"), QStringLiteral("/tmp")}, QStringLiteral("/")).urls,
                 QList<QUrl>{QUrl(QStringLiteral("file:///tmp"))});
        QVERIFY(parseCli({QStringLiteral("--new-window")}, QStringLiteral("/")).newWindow);
    }
    void ruleOption()
    {
        const CliOptions o = parseCli({QStringLiteral("--rule"), QStringLiteral("Old"), QStringLiteral("/tmp/x")}, QStringLiteral("/"));
        QCOMPARE(o.rule, QStringLiteral("Old"));
        QCOMPARE(o.urls, QList<QUrl>{QUrl(QStringLiteral("file:///tmp/x"))});
    }
    void dialogOptions()
    {
        CliOptions a = parseCli({QStringLiteral("--select-by-rule"), QStringLiteral("smb://nas/x")}, QStringLiteral("/"));
        QVERIFY(a.selectByRule && !a.renameMany);
        QCOMPARE(a.urls, QList<QUrl>{QUrl(QStringLiteral("smb://nas/x"))});
        CliOptions b = parseCli({QStringLiteral("--rename-many"), QStringLiteral("/tmp/a"), QStringLiteral("/tmp/b")}, QStringLiteral("/"));
        QVERIFY(b.renameMany && !b.selectByRule);
        QCOMPARE(b.urls.size(), 2);
    }
};

QTEST_GUILESS_MAIN(CliTest)
#include "cli_test.moc"

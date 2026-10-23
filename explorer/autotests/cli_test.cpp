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
};

QTEST_GUILESS_MAIN(CliTest)
#include "cli_test.moc"

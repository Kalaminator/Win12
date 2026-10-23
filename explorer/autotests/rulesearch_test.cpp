#include "rulesearch.h"
#include "rulematcher.h"
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

Q_DECLARE_METATYPE(KFileItemList)

class RuleSearchTest : public QObject
{
    Q_OBJECT
    QTemporaryDir dir;

    static RuleMatcher txt()
    {
        QString err;
        return *RuleMatcher::compile({{Field::Extension, Test::Equals, QStringLiteral("txt")}}, &err);
    }
    QStringList run(bool recursive)
    {
        RuleSearch s;
        QStringList got;
        connect(&s, &RuleSearch::found, this, [&](const KFileItemList &l) {
            for (const KFileItem &i : l) {
                QVERIFY(i.url().isLocalFile() && QDir::isAbsolutePath(i.url().toLocalFile()));
                got << QDir(dir.path()).relativeFilePath(i.url().toLocalFile());
            }
        });
        QSignalSpy fin(&s, &RuleSearch::finished);
        s.start(QUrl::fromLocalFile(dir.path()), recursive, txt());
        [&] { QVERIFY(fin.wait(5000) || fin.count()); }();
        got.sort();
        return got;
    }
private Q_SLOTS:
    void initTestCase()
    {
        qRegisterMetaType<KFileItemList>();
        QDir d(dir.path());
        d.mkpath(QStringLiteral("sub/deep"));
        for (const char *f : {"a.txt", "sub/b.txt", "sub/deep/c.log"}) {
            QFile file(d.filePath(QString::fromLatin1(f)));
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
    }
    void nonRecursiveListsTopLevelOnly() { QCOMPARE(run(false), QStringList{QStringLiteral("a.txt")}); }
    void recursiveFindsNested() { QCOMPARE(run(true), (QStringList{QStringLiteral("a.txt"), QStringLiteral("sub/b.txt")})); }
    void closingStopsTheJob()
    {
        RuleSearch s;
        QSignalSpy fin(&s, &RuleSearch::finished);
        s.start(QUrl::fromLocalFile(dir.path()), true, txt());
        QVERIFY(s.isRunning());
        s.stop();
        QVERIFY(!s.isRunning());
        QVERIFY(!fin.wait(200));
    }
    void missingFolderFinishesWithError()
    {
        RuleSearch s;
        QSignalSpy fin(&s, &RuleSearch::finished);
        s.start(QUrl::fromLocalFile(dir.path() + QStringLiteral("/nope")), false, txt());
        QVERIFY(fin.wait(5000));
        QCOMPARE(fin.first().at(0).toBool(), false);
        QVERIFY(!fin.first().at(1).toString().isEmpty());
        QVERIFY(!s.isRunning());
    }
};
QTEST_MAIN(RuleSearchTest)
#include "rulesearch_test.moc"

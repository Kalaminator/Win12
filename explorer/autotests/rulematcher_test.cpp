#include "rulematcher.h"
#include "tagstore.h"
#include <KFileItem>
#include <QDateTime>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <fcntl.h>
#include <sys/stat.h>

static QStringList L(std::initializer_list<const char *> l)
{
    QStringList r;
    for (auto s : l)
        r << QString::fromLatin1(s);
    return r;
}

class RuleMatcherTest : public QObject
{
    Q_OBJECT
    QTemporaryDir dir;
    KFileItem a, b, big, old;

    static QStringList names(const RuleMatcher &m, const QList<KFileItem> &all)
    {
        QStringList r;
        for (const KFileItem &i : all)
            if (m.matches(i))
                r << i.name();
        r.sort();
        return r;
    }
    QList<KFileItem> all() const { return {a, b, big, old}; }
    QStringList run(const QList<Rule> &rules)
    {
        QString err;
        const auto m = RuleMatcher::compile(rules, &err);
        if (!m)
            return {QStringLiteral("ERR:") + err};
        return names(*m, all());
    }
    static void write(const QString &p, qint64 n)
    {
        QFile f(p);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QByteArray(n, 'x'));
    }

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(dir.isValid());
        const QString d = dir.path();
        write(d + QStringLiteral("/a.txt"), 10);
        write(d + QStringLiteral("/B.TXT"), 2000);
        write(d + QStringLiteral("/big.bin"), 3 * 1024 * 1024);
        QVERIFY(QDir(d).mkdir(QStringLiteral("old")));
        const time_t t = QDateTime::currentDateTime().addDays(-60).toSecsSinceEpoch();
        const timespec ts[2] = {{t, 0}, {t, 0}};
        QCOMPARE(utimensat(AT_FDCWD, QFile::encodeName(d + QStringLiteral("/old")).constData(), ts, 0), 0);
        auto item = [&](const char *n) { return KFileItem(QUrl::fromLocalFile(d + QLatin1Char('/') + QLatin1String(n))); };
        a = item("a.txt");
        b = item("B.TXT");
        big = item("big.bin");
        old = item("old");
    }
    void hasTagByWordOrEncoded()
    {
        QTemporaryDir td(QDir::homePath() + QStringLiteral("/.cache/rulematcher-XXXXXX"));
        if (!td.isValid() || !TagStore::supported(QUrl::fromLocalFile(td.path())))
            QSKIP("xattr tags not supported here");
        write(td.path() + QStringLiteral("/t.txt"), 1);
        const QUrl u = QUrl::fromLocalFile(td.path() + QStringLiteral("/t.txt"));
        TagStore s;
        QCOMPARE(s.add({u}, Tag::decode(QStringLiteral("Red/Xiaomi"))), TagStore::Error::None);
        const KFileItem it(u);
        auto hit = [&](const char *v) {
            QString err;
            const auto m = RuleMatcher::compile({{Field::Tag, Test::HasTag, QString::fromLatin1(v)}}, &err);
            return m && m->matches(it);
        };
        QVERIFY(hit("xiaomi"));
        QVERIFY(hit("Red/Xiaomi"));
        QVERIFY(!hit("Blue/Xiaomi"));
        QVERIFY(!hit("other"));
    }
    void hasTagEmptyValueIsError()
    {
        QVERIFY(run({{Field::Tag, Test::HasTag, QString()}}).first().startsWith(QLatin1String("ERR:")));
        QCOMPARE(testsFor(Field::Tag), QList<Test>{Test::HasTag});
    }
    void nameStartsWithIsCaseInsensitive()
    {
        QCOMPARE(run({{Field::Name, Test::StartsWith, QStringLiteral("b")}}), L({"B.TXT", "big.bin"}));
    }
    void extensionEqualsIgnoresDot()
    {
        QCOMPARE(run({{Field::Extension, Test::Equals, QStringLiteral("txt")}}), L({"B.TXT", "a.txt"}));
        QCOMPARE(run({{Field::Extension, Test::Equals, QStringLiteral("")}}).size(), 1); // empty value is an error for text tests
    }
    void regexMatchesWholeOrPart()
    {
        QCOMPARE(run({{Field::Name, Test::Regex, QStringLiteral("^a\\.")}}), L({"a.txt"}));
        QCOMPARE(run({{Field::Name, Test::Regex, QStringLiteral("IG")}}), L({"big.bin"}));
    }
    void invalidRegexReportsError()
    {
        QString err;
        QVERIFY(!RuleMatcher::compile({{Field::Name, Test::Regex, QStringLiteral("[")}}, &err));
        QVERIFY(!err.isEmpty());
    }
    void sizeBiggerThanParsesUnits()
    {
        QCOMPARE(run({{Field::Size, Test::BiggerThan, QStringLiteral("2 MB")}}), L({"big.bin"}));
    }
    void parseSizeRejectsOverflow()
    {
        bool ok = true;
        parseSize(QStringLiteral("99999999 TB"), &ok);
        QVERIFY(!ok);
        parseSize(QStringLiteral("8191 TB"), &ok);
        QVERIFY(ok);
    }
    void parseSizeRejectsGarbage()
    {
        bool ok = true;
        parseSize(QStringLiteral("lots"), &ok);
        QVERIFY(!ok);
        QCOMPARE(parseSize(QStringLiteral("10 MB"), &ok), qint64(10485760));
        QVERIFY(ok);
        QCOMPARE(parseSize(QStringLiteral("500 KB"), &ok), qint64(512000));
        QCOMPARE(parseSize(QStringLiteral("2 GB"), &ok), qint64(2) * 1024 * 1024 * 1024);
        QCOMPARE(parseSize(QStringLiteral("4096"), &ok), qint64(4096));
        QVERIFY(ok);
        parseSize(QStringLiteral("-5"), &ok);
        QVERIFY(!ok);
    }
    void olderThanRelativeAge()
    {
        QCOMPARE(run({{Field::Modified, Test::OlderThan, QStringLiteral("30 days")}}), L({"old"}));
    }
    void newerThanIsoDate()
    {
        const QString d = QDateTime::currentDateTime().addDays(-1).date().toString(Qt::ISODate);
        QCOMPARE(run({{Field::Modified, Test::NewerThan, d}}), L({"B.TXT", "a.txt", "big.bin"}));
        bool ok;
        const QDateTime now = QDateTime::currentDateTime();
        QCOMPARE(parseDateValue(QStringLiteral("2 weeks"), now, &ok), now.addDays(-14));
        QCOMPARE(parseDateValue(QStringLiteral("6 months"), now, &ok), now.addMonths(-6));
        QCOMPARE(parseDateValue(QStringLiteral("1 year"), now, &ok), now.addYears(-1));
        QCOMPARE(parseDateValue(QStringLiteral("2026-01-01"), now, &ok), QDateTime(QDate(2026, 1, 1), QTime(0, 0)));
        parseDateValue(QStringLiteral("soon"), now, &ok);
        QVERIFY(!ok);
    }
    void kindFolder()
    {
        QCOMPARE(run({{Field::Kind, Test::Equals, QStringLiteral("folder")}}), L({"old"}));
        QCOMPARE(run({{Field::Kind, Test::Equals, QStringLiteral("file")}}).size(), 3);
    }
    void rulesAreAnded()
    {
        QCOMPARE(run({{Field::Name, Test::Contains, QStringLiteral("t")}, {Field::Size, Test::SmallerThan, QStringLiteral("1 KB")}}),
                 L({"a.txt"}));
    }
    void emptyRulesMatchAll()
    {
        QCOMPARE(run({}).size(), 4);
    }
    void copyable()
    {
        QString err;
        auto m = RuleMatcher::compile({{Field::Kind, Test::Equals, QStringLiteral("folder")}}, &err);
        const RuleMatcher c = *m;
        QVERIFY(c.matches(old));
        QVERIFY(!testsFor(Field::Size).isEmpty());
    }
};

QTEST_GUILESS_MAIN(RuleMatcherTest)
#include "rulematcher_test.moc"

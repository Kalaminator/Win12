#include "tagstore.h"
#include <KFileMetaData/UserMetaData>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <unistd.h>

using KFileMetaData::UserMetaData;

class TagStoreTest : public QObject
{
    Q_OBJECT
    QTemporaryDir dir{QDir::homePath() + QStringLiteral("/.cache/tagstore-XXXXXX")};
    QUrl file(const QString &n)
    {
        const QString p = dir.filePath(n);
        QFile f(p);
        (void)f.open(QIODevice::WriteOnly);
        return QUrl::fromLocalFile(p);
    }
    static Tag T(const char *s) { return Tag::decode(QString::fromLatin1(s)); }
private Q_SLOTS:
    void initTestCase()
    {
        QDir().mkpath(QDir::homePath() + QStringLiteral("/.cache"));
        if (!dir.isValid() || !TagStore::supported(QUrl::fromLocalFile(dir.path())))
            QSKIP("xattr tags not supported here");
    }
    void addReadRoundTrip()
    {
        TagStore s;
        const QUrl u = file(QStringLiteral("a"));
        QCOMPARE(s.add({u}, T("Red/x")), TagStore::Error::None);
        QCOMPARE(TagStore::read(u), QList<Tag>{T("Red/x")});
    }
    void addTwiceIsNoop()
    {
        TagStore s;
        const QUrl u = file(QStringLiteral("b"));
        s.add({u}, T("Red/x"));
        QSignalSpy spy(&s, &TagStore::tagsChanged);
        QCOMPARE(s.add({u}, T("Red/x")), TagStore::Error::None);
        QCOMPARE(TagStore::read(u).size(), 1);
        QCOMPARE(spy.count(), 0);
    }
    void limitCountsWin12TagsOnly()
    {
        TagStore s;
        const QUrl u = file(QStringLiteral("c"));
        UserMetaData(u.toLocalFile()).setTags({QStringLiteral("holiday"), QStringLiteral("Red/a"), QStringLiteral("Blue/b"), QStringLiteral("Green/c")});
        QCOMPARE(s.add({u}, T("Gold/d")), TagStore::Error::TooMany);
        QCOMPARE(TagStore::read(u).size(), 4);
    }
    void removeKeepsOthersAndForeign()
    {
        TagStore s;
        const QUrl u = file(QStringLiteral("d"));
        UserMetaData(u.toLocalFile()).setTags({QStringLiteral("holiday"), QStringLiteral("Red/a"), QStringLiteral("Blue/b")});
        QCOMPARE(s.remove({u}, T("Red/a")), TagStore::Error::None);
        QCOMPARE(UserMetaData(u.toLocalFile()).tags(), (QStringList{QStringLiteral("holiday"), QStringLiteral("Blue/b")}));
    }
    void replaceRenames()
    {
        TagStore s;
        const QUrl u = file(QStringLiteral("e"));
        UserMetaData(u.toLocalFile()).setTags({QStringLiteral("holiday"), QStringLiteral("Red/a")});
        QCOMPARE(s.replace(u, T("Red/a"), T("Blue/z")), TagStore::Error::None);
        QCOMPARE(UserMetaData(u.toLocalFile()).tags(), (QStringList{QStringLiteral("holiday"), QStringLiteral("Blue/z")}));
    }
    void nonLocalIsNotLocal()
    {
        TagStore s;
        const QUrl u(QStringLiteral("sftp://h/x"));
        QVERIFY(TagStore::read(u).isEmpty());
        QVERIFY(!TagStore::supported(u));
        QCOMPARE(s.add({u}, T("Red/x")), TagStore::Error::NotLocal);
    }
    void tagsChangedListsWrittenUrlsOnly()
    {
        if (geteuid() == 0)
            QSKIP("root ignores permissions");
        TagStore s;
        const QUrl a = file(QStringLiteral("f1")), b = file(QStringLiteral("f2"));
        QVERIFY(QFile::setPermissions(b.toLocalFile(), QFileDevice::ReadOwner));
        QSignalSpy spy(&s, &TagStore::tagsChanged);
        QCOMPARE(s.add({a, b}, T("Red/x")), TagStore::Error::WriteFailed);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).value<QList<QUrl>>(), QList<QUrl>{a});
    }
};

QTEST_MAIN(TagStoreTest)
#include "tagstore_test.moc"

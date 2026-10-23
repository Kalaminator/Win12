#include "filemanager1.h"
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTest>

class FileManager1Test : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void foldersForGroupsByParent()
    {
        const auto m = FileManager1::foldersFor({QStringLiteral("file:///home/u/a%20b/x.txt"), QStringLiteral("file:///home/u/a%20b/y%23.txt"),
                                                 QStringLiteral("file:///tmp/z"), QStringLiteral("smb://nas/share/doc.odt"),
                                                 QStringLiteral("file:///home/u/dir/")});
        QCOMPARE(m.size(), 4);
        const QList<QUrl> ab = m.value(QUrl(QStringLiteral("file:///home/u/a%20b")));
        QCOMPARE(ab.size(), 2);
        QCOMPARE(ab[0].fileName(), QStringLiteral("x.txt"));
        QCOMPARE(ab[1].fileName(), QStringLiteral("y#.txt"));
        QCOMPARE(m.value(QUrl(QStringLiteral("file:///tmp"))).size(), 1);
        QCOMPARE(m.value(QUrl(QStringLiteral("smb://nas/share"))).size(), 1);
        QVERIFY(m.contains(QUrl(QStringLiteral("file:///home/u/dir/"))));
        QCOMPARE(m.value(QUrl(QStringLiteral("file:///home/u/dir/"))).size(), 0);
    }

    void registrationFollowsStateFile()
    {
        QStandardPaths::setTestModeEnabled(true);
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + QStringLiteral("/win12-explorer");
        QFile::remove(dir + QStringLiteral("/saved-default"));
        QVERIFY(!FileManager1::wantsRegistration());
        QDir().mkpath(dir);
        QFile f(dir + QStringLiteral("/saved-default"));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.close();
        QVERIFY(FileManager1::wantsRegistration());
        f.remove();
    }
};

QTEST_GUILESS_MAIN(FileManager1Test)
#include "filemanager1_test.moc"

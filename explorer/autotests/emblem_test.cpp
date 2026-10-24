#include "emblem.h"
#include "tag.h"
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTest>

class EmblemTest : public QObject
{
    Q_OBJECT
    static QString hex(int i) { return TagColours::colour(i).name().mid(1); }
private Q_SLOTS:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }
    void nameIsSortedAndCapped()
    {
        QCOMPARE(Emblem::iconName({9, 1}), Emblem::iconName({1, 9}));
        QCOMPARE(Emblem::iconName({9, 1}), QStringLiteral("win12-tags-%1-%2").arg(hex(1), hex(9)));
        QCOMPARE(Emblem::iconName({20, 9, 1, 3}), QStringLiteral("win12-tags-%1-%2-%3").arg(hex(1), hex(3), hex(9)));
        QCOMPARE(Emblem::iconName({}), QString());
    }
    void svgHasOneRectPerColour()
    {
        const QString s = QString::fromUtf8(Emblem::svg({20, 9, 1}));
        QVERIFY(s.contains(QLatin1String("viewBox=\"0 0 16 16\"")));
        QCOMPARE(s.count(QLatin1String("<polygon")), 3);
        for (int i : {1, 9, 20})
            QCOMPARE(s.count(QLatin1Char('#') + hex(i)), 1);
        QCOMPARE(QString::fromUtf8(Emblem::svg({9})).count(QLatin1String("<polygon")), 1);
    }
    void ensureWritesOnce()
    {
        const QString name = Emblem::iconName({9, 1});
        const QString path = Emblem::iconDir() + QLatin1Char('/') + name + QStringLiteral(".svg");
        QVERIFY(path.contains(QLatin1String("/icons/hicolor/scalable/emblems/")));
        QFile::remove(path);
        QCOMPARE(Emblem::ensure({1, 9}), name);
        QVERIFY(QFile::exists(path));
        const QDateTime old = QDateTime::currentDateTime().addDays(-1);
        {
            QFile f(path);
            QVERIFY(f.open(QIODevice::ReadWrite));
            QVERIFY(f.setFileTime(old, QFileDevice::FileModificationTime));
        }
        QCOMPARE(Emblem::ensure({9, 1}), name);
        QCOMPARE(QFileInfo(path).lastModified().toSecsSinceEpoch(), old.toSecsSinceEpoch());
        QCOMPARE(Emblem::ensure({}), QString());
    }
};

QTEST_GUILESS_MAIN(EmblemTest)
#include "emblem_test.moc"

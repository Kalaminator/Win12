#include "sortgroup.h"
#include "tag.h"
#include <KDirModel>
#include <KIO/UDSEntry>
#include <QMimeDatabase>
#include <QTest>

using namespace SortGroup;
using namespace Qt::StringLiterals;

static KFileItem mk(const QString &name, quint64 size = 1, const QDateTime &mtime = {}, bool dir = false, const QString &mime = {})
{
    KIO::UDSEntry e;
    e.fastInsert(KIO::UDSEntry::UDS_NAME, name);
    e.fastInsert(KIO::UDSEntry::UDS_SIZE, qlonglong(size));
    e.fastInsert(KIO::UDSEntry::UDS_MODIFICATION_TIME, mtime.toSecsSinceEpoch());
    e.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, dir ? S_IFDIR : S_IFREG);
    if (!mime.isEmpty())
        e.fastInsert(KIO::UDSEntry::UDS_MIME_TYPE, mime);
    return KFileItem(e, QUrl(QStringLiteral("file:///t/") + name));
}

class SortGroupTest : public QObject
{
    Q_OBJECT
    const QDateTime now{QDate(2026, 10, 23), QTime(12, 0)};
private Q_SLOTS:
    void modifiedBuckets_data()
    {
        QTest::addColumn<int>("days");
        QTest::addColumn<QString>("label");
        const QList<std::pair<int, const char *>> rows = {{0, "Today"}, {-1, "Yesterday"}, {-3, "Earlier this week"}, {-5, "Last week"},
            {-9, "Last week"}, {-13, "Earlier this month"}, {-30, "Last month"}, {-70, "Earlier this year"}, {-400, "A long time ago"}};
        for (const auto &r : rows)
            QTest::newRow(QByteArray(QByteArray(r.second).append(QByteArray::number(r.first))).constData()) << r.first << QString::fromLatin1(r.second);
    }
    void modifiedBuckets()
    {
        QFETCH(int, days);
        QFETCH(QString, label);
        QCOMPARE(groupInfo(mk(QStringLiteral("f"), 1, now.addDays(days)), GroupRole::Modified, now).label, label);
    }
    void sizeBuckets_data()
    {
        QTest::addColumn<quint64>("size");
        QTest::addColumn<int>("order");
        const QList<std::pair<quint64, int>> rows = {{0, 0}, {16384, 1}, {16385, 2}, {1ull << 20, 2}, {(1ull << 20) + 1, 3},
            {128ull << 20, 3}, {1ull << 30, 4}, {4ull << 30, 5}, {(4ull << 30) + 1, 6}};
        for (const auto &r : rows)
            QTest::newRow(QByteArray::number(r.first).constData()) << r.first << r.second;
    }
    void sizeBuckets()
    {
        QFETCH(quint64, size);
        QFETCH(int, order);
        QCOMPARE(groupInfo(mk(QStringLiteral("f"), size), GroupRole::Size, now).order, order);
    }
    void sizeFolders()
    {
        QCOMPARE(groupInfo(mk(QStringLiteral("d"), 0, now, true), GroupRole::Size, now), (GroupInfo{-1, QStringLiteral("Folders")}));
        QCOMPARE(groupInfo(mk(QStringLiteral("f"), 0), GroupRole::Size, now).label, QStringLiteral("Empty (0 KB)"));
    }
    void nameBuckets_data()
    {
        QTest::addColumn<QString>("name");
        QTest::addColumn<QString>("label");
        const QList<std::pair<QString, QString>> rows = {{u"7up"_s, u"0-9"_s}, {u"apple"_s, u"A-H"_s}, {u"Hat"_s, u"A-H"_s},
            {u"iris"_s, u"I-P"_s}, {u"Zoo"_s, u"Q-Z"_s}, {u"_x"_s, u"Other"_s}, {u"éclair"_s, u"Other"_s}};
        for (const auto &r : rows)
            QTest::newRow(r.first.toUtf8().constData()) << r.first << r.second;
    }
    void nameBuckets()
    {
        QFETCH(QString, name);
        QFETCH(QString, label);
        QCOMPARE(groupInfo(mk(name), GroupRole::Name, now).label, label);
    }
    void typeGroups()
    {
        QCOMPARE(groupInfo(mk(QStringLiteral("d"), 0, now, true), GroupRole::Type, now), (GroupInfo{0, QStringLiteral("File folder")}));
        const GroupInfo g = groupInfo(mk(QStringLiteral("a.txt"), 1, now, false, QStringLiteral("text/plain")), GroupRole::Type, now);
        QCOMPARE(g.order, 1);
        QCOMPARE(g.label, QMimeDatabase().mimeTypeForName(QStringLiteral("text/plain")).comment());
    }
    void noneGroup()
    {
        QCOMPARE(groupInfo(mk(QStringLiteral("f")), GroupRole::None, now), (GroupInfo{0, QString()}));
    }
    void groupLessOrdersThenLabel()
    {
        QVERIFY(groupLess({0, QStringLiteral("b")}, {1, QStringLiteral("a")}));
        QVERIFY(groupLess({1, QStringLiteral("a")}, {1, QStringLiteral("B")}));
        QVERIFY(!groupLess({1, QStringLiteral("b")}, {1, QStringLiteral("a")}));
    }
    void tagNameGroupsOnePerWord()
    {
        const auto g = tagGroups({Tag::decode(u"Red/Xiaomi"_s), Tag::decode(u"Blue/Xiaomi"_s)}, GroupRole::TagName);
        QCOMPARE(g, (QList<GroupInfo>{{0, u"Xiaomi"_s}}));
    }
    void tagColourGroupsOnePerColour()
    {
        const auto g = tagGroups({Tag::decode(u"Red/Xiaomi"_s), Tag::decode(u"Red/Urgent"_s)}, GroupRole::TagColour);
        QCOMPARE(g, (QList<GroupInfo>{{TagColours::indexOfName(u"Red"_s), u"Red"_s}}));
        QCOMPARE(g.first().order, 9);
    }
    void noTagGroupLast()
    {
        for (GroupRole r : {GroupRole::TagName, GroupRole::TagColour}) {
            const auto g = tagGroups({}, r);
            QCOMPARE(g.size(), 1);
            QCOMPARE(g.first().order, 1000);
            QVERIFY(g.first().order > TagColours::Count);
        }
    }
    void foreignTagGroup()
    {
        QCOMPARE(tagGroups({Tag::decode(u"work"_s)}, GroupRole::TagName), (QList<GroupInfo>{{999, u"work"_s}}));
        QCOMPARE(tagGroups({Tag::decode(u"work"_s)}, GroupRole::TagColour),
                 (QList<GroupInfo>{{TagColours::indexOfName(u"Overcast"_s), u"Overcast"_s}}));
    }
    void sortColumns()
    {
        QCOMPARE(sortColumnFor(SortRole::Name), int(KDirModel::Name));
        QCOMPARE(sortColumnFor(SortRole::Modified), int(KDirModel::ModifiedTime));
        QCOMPARE(sortColumnFor(SortRole::Size), int(KDirModel::Size));
        QCOMPARE(sortColumnFor(SortRole::Type), int(KDirModel::Type));
    }
};
QTEST_GUILESS_MAIN(SortGroupTest)
#include "sortgroup_test.moc"

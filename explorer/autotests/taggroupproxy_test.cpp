#include "foldermodel.h"
#include "taggroupproxy.h"
#include "tagstore.h"
#include <KDirModel>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QStandardItemModel>
#include <QTemporaryDir>
#include <QTest>

class TagGroupProxyTest : public QObject
{
    Q_OBJECT
    QTemporaryDir dir{QDir::homePath() + QStringLiteral("/.cache/taggroupproxy-XXXXXX")};
    static Tag T(const char *s) { return Tag::decode(QString::fromLatin1(s)); }
    // A source row whose FileItemRole is a fresh temp file carrying `tags`.
    QStandardItem *fileRow(const QString &name, const QList<Tag> &tags)
    {
        const QString p = dir.filePath(name);
        QFile f(p);
        (void)f.open(QIODevice::WriteOnly);
        f.close();
        for (const Tag &t : tags)
            TagStore().add({QUrl::fromLocalFile(p)}, t);
        auto *item = new QStandardItem(name);
        item->setData(QVariant::fromValue(KFileItem(QUrl::fromLocalFile(p))), KDirModel::FileItemRole);
        return item;
    }
    static QStringList labels(const TagGroupProxy &p)
    {
        QStringList out;
        for (int r = 0; r < p.rowCount(); ++r)
            out << p.index(r, 0).data(FolderModel::GroupLabelRole).toString() + QLatin1Char(':') + p.index(r, 0).data().toString();
        return out;
    }
    static QString noTag() { return SortGroup::tagGroups({}, GroupRole::TagName).first().label; }

private Q_SLOTS:
    void initTestCase()
    {
        QDir().mkpath(QDir::homePath() + QStringLiteral("/.cache"));
        if (!dir.isValid() || !TagStore::supported(QUrl::fromLocalFile(dir.path())))
            QSKIP("xattr tags not supported here");
    }
    void identityWhenNotTagMode()
    {
        QStandardItemModel src;
        src.appendRow(fileRow(QStringLiteral("i1"), {T("Red/Xiaomi"), T("OrangeBright/Urgent")}));
        src.appendRow(fileRow(QStringLiteral("i2"), {}));
        TagGroupProxy p;
        p.setSourceModel(&src);
        p.setGroupRole(GroupRole::Name);
        QCOMPARE(p.rowCount(), 2);
        QCOMPARE(p.mapToSource(p.index(1, 0)), src.index(1, 0));
        QCOMPARE(p.mapFromSource(src.index(0, 0)), p.index(0, 0));
        QCOMPARE(p.index(1, 0).data().toString(), QStringLiteral("i2"));
        QSignalSpy inserted(&p, &QAbstractItemModel::rowsInserted);
        src.appendRow(fileRow(QStringLiteral("i3"), {}));
        QCOMPARE(inserted.count(), 1); // forwarded 1:1, no reset
    }
    void twoTagsTwoRows()
    {
        QStandardItemModel src;
        src.appendRow(fileRow(QStringLiteral("phone"), {T("Red/Xiaomi"), T("OrangeBright/Urgent")}));
        TagGroupProxy p;
        p.setSourceModel(&src);
        p.setGroupRole(GroupRole::TagName);
        QCOMPARE(labels(p), (QStringList{QStringLiteral("Urgent:phone"), QStringLiteral("Xiaomi:phone")}));
        QVERIFY(p.index(0, 0).data(FolderModel::GroupFirstRole).toBool());
        QVERIFY(p.index(1, 0).data(FolderModel::GroupFirstRole).toBool());
        QCOMPARE(p.mapToSource(p.index(1, 0)), src.index(0, 0));
        QCOMPARE(p.mapFromSource(src.index(0, 0)), p.index(0, 0)); // the first duplicate
    }
    void untaggedInNoTagGroupLast()
    {
        QStandardItemModel src;
        src.appendRow(fileRow(QStringLiteral("u1"), {}));
        src.appendRow(fileRow(QStringLiteral("u2"), {T("Red/x")}));
        TagGroupProxy p;
        p.setSourceModel(&src);
        p.setGroupRole(GroupRole::TagName);
        QCOMPARE(labels(p), (QStringList{QStringLiteral("x:u2"), noTag() + QStringLiteral(":u1")}));
    }
    void tagColourMergesSameColour()
    {
        QStandardItemModel src;
        src.appendRow(fileRow(QStringLiteral("c1"), {T("Red/x"), T("Red/y")}));
        src.appendRow(fileRow(QStringLiteral("c2"), {T("Red/z")}));
        TagGroupProxy p;
        p.setSourceModel(&src);
        p.setGroupRole(GroupRole::TagColour);
        QCOMPARE(p.rowCount(), 2);
        QCOMPARE(p.index(0, 0).data().toString(), QStringLiteral("c1"));
        QCOMPARE(p.index(1, 0).data().toString(), QStringLiteral("c2"));
        QCOMPARE(p.index(0, 0).data(FolderModel::GroupLabelRole), p.index(1, 0).data(FolderModel::GroupLabelRole));
        QVERIFY(p.index(0, 0).data(FolderModel::GroupFirstRole).toBool());
        QVERIFY(!p.index(1, 0).data(FolderModel::GroupFirstRole).toBool());
    }
    void sourceInsertRebuilds()
    {
        QStandardItemModel src;
        src.appendRow(fileRow(QStringLiteral("s1"), {T("Red/x")}));
        TagGroupProxy p;
        p.setSourceModel(&src);
        p.setGroupRole(GroupRole::TagName);
        QSignalSpy reset(&p, &QAbstractItemModel::modelReset);
        src.appendRow(fileRow(QStringLiteral("s2"), {T("Red/x"), T("Blue/a")}));
        QCOMPARE(reset.count(), 1);
        QCOMPARE(labels(p), (QStringList{QStringLiteral("a:s2"), QStringLiteral("x:s1"), QStringLiteral("x:s2")}));
        src.removeRow(0);
        QCOMPARE(labels(p), (QStringList{QStringLiteral("a:s2"), QStringLiteral("x:s2")}));
    }
    void invalidateTagsRegroups()
    {
        QStandardItemModel src;
        src.appendRow(fileRow(QStringLiteral("v1"), {T("Red/x")}));
        TagGroupProxy p;
        p.setSourceModel(&src);
        p.setGroupRole(GroupRole::TagName);
        const QUrl u = QUrl::fromLocalFile(dir.filePath(QStringLiteral("v1")));
        TagStore().add({u}, T("Blue/a"));
        QCOMPARE(p.rowCount(), 1); // cached until told
        p.invalidateTags({u});
        QCOMPARE(labels(p), (QStringList{QStringLiteral("a:v1"), QStringLiteral("x:v1")}));
    }
};

QTEST_MAIN(TagGroupProxyTest)
#include "taggroupproxy_test.moc"

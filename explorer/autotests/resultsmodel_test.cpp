#include "resultsmodel.h"
#include <KDirModel>
#include <QSignalSpy>
#include <QTest>

class ResultsModelTest : public QObject
{
    Q_OBJECT

    static KFileItem item(const QString &path)
    {
        return KFileItem(QUrl::fromLocalFile(path), QString(), KFileItem::Unknown);
    }

private Q_SLOTS:
    void columnsMatchKDirModel()
    {
        ResultsModel m;
        QCOMPARE(m.columnCount(), int(KDirModel::ColumnCount));
        QCOMPARE(m.headerData(0, Qt::Horizontal).toString(), QStringLiteral("Name"));
    }
    void appendEmitsRowsInserted()
    {
        ResultsModel m;
        QSignalSpy spy(&m, &QAbstractItemModel::rowsInserted);
        m.append({item(QStringLiteral("/tmp/a")), item(QStringLiteral("/tmp/b"))});
        m.append({item(QStringLiteral("/tmp/c"))});
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(1).at(1).toInt(), 2);
        QCOMPARE(spy.at(1).at(2).toInt(), 2);
        QCOMPARE(m.rowCount(), 3);
        QCOMPARE(m.count(), 3);
        QCOMPARE(m.itemAt(2).url(), QUrl::fromLocalFile(QStringLiteral("/tmp/c")));
        QCOMPARE(m.indexForUrl(QUrl::fromLocalFile(QStringLiteral("/tmp/b"))).row(), 1);
        m.clear();
        QCOMPARE(m.rowCount(), 0);
        QVERIFY(!m.indexForUrl(QUrl::fromLocalFile(QStringLiteral("/tmp/b"))).isValid());
    }
    void fileItemRoleRoundTrip()
    {
        ResultsModel m;
        m.append({item(QStringLiteral("/tmp/a.txt"))});
        const QModelIndex idx = m.index(0, KDirModel::Size);
        QCOMPARE(idx.data(KDirModel::FileItemRole).value<KFileItem>().url(), QUrl::fromLocalFile(QStringLiteral("/tmp/a.txt")));
        QCOMPARE(m.index(0, 0).data().toString(), QStringLiteral("a.txt"));
        QCOMPARE(m.index(0, 0).internalPointer(), m.index(0, 3).internalPointer()); // one stable node per row
        m.append({item(QStringLiteral("/tmp/b.txt"))});
        QCOMPARE(m.index(0, 0).internalPointer(), idx.internalPointer());
    }
};

QTEST_MAIN(ResultsModelTest)
#include "resultsmodel_test.moc"

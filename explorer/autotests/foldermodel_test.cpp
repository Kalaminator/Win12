#include "foldermodel.h"
#include <KDirLister>
#include <KIO/PreviewJob>
#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QListView>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

class FolderModelTest : public QObject
{
    Q_OBJECT

    static void touch(const QString &path, qint64 size = 0)
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        if (size)
            f.write(QByteArray(size, 'x'));
    }
    static bool list(FolderModel &model, const QTemporaryDir &dir)
    {
        QSignalSpy done(model.lister(), &KCoreDirLister::listingDirCompleted);
        model.openUrl(QUrl::fromLocalFile(dir.path()));
        return done.wait(5000);
    }
    static QStringList names(const QAbstractItemModel &model)
    {
        QStringList out;
        for (int r = 0; r < model.rowCount(); ++r)
            out << model.index(r, 0).data().toString();
        return out;
    }

private Q_SLOTS:
    void listsAndSortsFoldersFirst()
    {
        QTemporaryDir dir;
        touch(dir.filePath(QStringLiteral("b.txt")));
        touch(dir.filePath(QStringLiteral("a.txt")));
        QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("zdir")));
        FolderModel model;
        QVERIFY(list(model, dir));
        QCOMPARE(names(model), (QStringList{QStringLiteral("zdir"), QStringLiteral("a.txt"), QStringLiteral("b.txt")}));
        model.setSorting(SortRole::Name, Qt::DescendingOrder);
        QCOMPARE(names(model), (QStringList{QStringLiteral("zdir"), QStringLiteral("b.txt"), QStringLiteral("a.txt")}));
    }

    void groupsBySizeThenName()
    {
        QTemporaryDir dir;
        touch(dir.filePath(QStringLiteral("c")), 0);
        touch(dir.filePath(QStringLiteral("a")), 20 * 1024);
        touch(dir.filePath(QStringLiteral("b")), 2 * 1024 * 1024);
        FolderModel model;
        QVERIFY(list(model, dir));
        model.setGroupRole(GroupRole::Size);
        QCOMPARE(model.groupRole(), GroupRole::Size);
        QCOMPARE(names(model), (QStringList{QStringLiteral("c"), QStringLiteral("a"), QStringLiteral("b")}));
        QCOMPARE(model.index(0, 0).data(FolderModel::GroupLabelRole).toString(), QStringLiteral("Empty (0 KB)"));
        for (int r = 0; r < 3; ++r)
            QVERIFY(model.index(r, 0).data(FolderModel::GroupFirstRole).toBool());
        model.setGroupRole(GroupRole::None);
        QCOMPARE(names(model), (QStringList{QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")}));
    }

    void tagsColumnIsLastAndEmpty()
    {
        QTemporaryDir dir;
        touch(dir.filePath(QStringLiteral("a.txt")));
        FolderModel model;
        QVERIFY(list(model, dir));
        TagsColumns t(&model);
        QCOMPARE(t.columnCount(), 8);
        QCOMPARE(t.headerData(7, Qt::Horizontal).toString(), QStringLiteral("Tags"));
        QVERIFY(t.index(0, 7).data(TagsColumns::TagsRole).toStringList().isEmpty());
        QCOMPARE(t.itemFor(t.index(0, 7)).name(), model.itemFor(model.index(0, 0)).name());
    }

    void largeFolderKeepsEventLoopFree()
    {
        QTemporaryDir dir;
        for (int i = 0; i < 5000; ++i)
            touch(dir.filePath(QStringLiteral("file%1.txt").arg(i)));
        FolderModel model;
        QElapsedTimer clock;
        qint64 last = 0, maxGap = 0;
        QTimer tick;
        tick.setInterval(10);
        connect(&tick, &QTimer::timeout, this, [&] {
            const qint64 now = clock.elapsed();
            maxGap = qMax(maxGap, now - last);
            last = now;
        });
        clock.start();
        tick.start();
        QVERIFY(list(model, dir));
        tick.stop();
        QCOMPARE(model.rowCount(), 5000);
        qInfo() << "max event loop gap" << maxGap << "ms";
        QVERIFY2(maxGap < 250, qPrintable(QStringLiteral("max gap %1 ms").arg(maxGap)));
    }

    void previewLoaderCachesByUrl()
    {
        if (KIO::PreviewJob::availablePlugins().isEmpty())
            QSKIP("no thumbnail plugins installed");
        QTemporaryDir dir;
        QImage img(64, 64, QImage::Format_RGB32);
        img.fill(Qt::red);
        QVERIFY(img.save(dir.filePath(QStringLiteral("pic.png"))));
        FolderModel model;
        QListView listView;
        listView.setModel(&model);
        listView.show();
        PreviewLoader p(&model);
        p.setView(&listView);
        p.setSize(48);
        p.setEnabled(true);
        model.setPreviewLoader(&p);
        QVERIFY(list(model, dir));
        QSignalSpy ready(&p, &PreviewLoader::previewReady);
        p.requestVisible();
        QVERIFY(ready.wait(5000));
        const QModelIndex idx = model.indexFor(ready.first().first().toUrl());
        QVERIFY(idx.isValid());
        const QPixmap pix = p.previewFor(model.itemFor(idx));
        QVERIFY(!pix.isNull());
        QVERIFY(pix.width() <= 48);
        QVERIFY(!model.index(idx.row(), 0).data(Qt::DecorationRole).value<QPixmap>().isNull());
    }
};

QTEST_MAIN(FolderModelTest)
#include "foldermodel_test.moc"

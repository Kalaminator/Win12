#include "foldermodel.h"
#include "tagstore.h"
#include <KFileMetaData/UserMetaData>
#include <QDir>
#include <KDirLister>
#include <KDirModel>
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
    // Windows details text: folders show no size, file sizes are right-aligned, dates use the short locale format.
    void detailsTextIsWindowsLike()
    {
        QTemporaryDir dir;
        touch(dir.filePath(QStringLiteral("a.txt")), 2048);
        QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("zdir")));
        FolderModel model;
        QVERIFY(list(model, dir));
        QCOMPARE(names(model), (QStringList{QStringLiteral("zdir"), QStringLiteral("a.txt")}));
        QVERIFY(model.index(0, KDirModel::Size).data().toString().isEmpty());
        QVERIFY(!model.index(1, KDirModel::Size).data().toString().isEmpty());
        QCOMPARE(model.index(1, KDirModel::Size).data(Qt::TextAlignmentRole).value<Qt::Alignment>() & Qt::AlignHorizontal_Mask, Qt::AlignRight);
        const QDateTime mtime = model.itemFor(model.index(1, 0)).time(KFileItem::ModificationTime);
        QCOMPARE(model.index(1, KDirModel::ModifiedTime).data().toString(), QLocale().toString(mtime, QLocale::ShortFormat));
    }

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

    void typeSortUsesCheapLabel()
    {
        // "noext" holds PNG bytes: mimeComment() would sniff it ("PNG image", sorted first); the cheap
        // extension-only label is application/octet-stream ("unknown"), sorted after "shell script".
        QTemporaryDir dir;
        {
            QFile f(dir.filePath(QStringLiteral("noext")));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(QByteArray::fromHex("89504e470d0a1a0a0000000d49484452"));
        }
        touch(dir.filePath(QStringLiteral("m.sh")));
        QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("zdir")));
        FolderModel model;
        QVERIFY(list(model, dir));
        model.setSorting(SortRole::Type, Qt::AscendingOrder);
        QCOMPARE(names(model), (QStringList{QStringLiteral("zdir"), QStringLiteral("m.sh"), QStringLiteral("noext")}));
        model.setSorting(SortRole::Type, Qt::DescendingOrder);
        QCOMPARE(names(model), (QStringList{QStringLiteral("zdir"), QStringLiteral("noext"), QStringLiteral("m.sh")}));
    }

    void tagsColumnReadsXattr()
    {
        QTemporaryDir dir{QDir::homePath() + QStringLiteral("/.cache/foldermodel-XXXXXX")};
        if (!dir.isValid() || !TagStore::supported(QUrl::fromLocalFile(dir.path())))
            QSKIP("xattr tags not supported here");
        touch(dir.filePath(QStringLiteral("a.txt")));
        KFileMetaData::UserMetaData(dir.filePath(QStringLiteral("a.txt"))).setTags({QStringLiteral("Red/Xiaomi")});
        FolderModel model;
        QVERIFY(list(model, dir));
        TagsColumns t(&model);
        QCOMPARE(t.columnCount(), 8);
        QCOMPARE(t.headerData(7, Qt::Horizontal).toString(), QStringLiteral("Tags"));
        for (const QString &id : knownColumnIds()) // the Windows titles, not KDirModel's ("Date" for modified)
            QCOMPARE(t.headerData(modelColumnFor(id), Qt::Horizontal).toString(), columnTitle(id));
        QCOMPARE(t.headerData(modelColumnFor(QStringLiteral("modified")), Qt::Horizontal).toString(), QStringLiteral("Date modified"));
        QCOMPARE(t.index(0, 7).data(TagsColumns::TagsRole).value<QList<Tag>>().first().word, QStringLiteral("Xiaomi"));
        QCOMPARE(t.index(0, 7).data(Qt::DisplayRole).toString(), QStringLiteral("Red/Xiaomi"));
        QCOMPARE(t.itemFor(t.index(0, 7)).name(), model.itemFor(model.index(0, 0)).name());
    }

    void sourceDataChangedDropsOnlyItsRows()
    {
        QTemporaryDir dir{QDir::homePath() + QStringLiteral("/.cache/foldermodel-XXXXXX")};
        if (!dir.isValid() || !TagStore::supported(QUrl::fromLocalFile(dir.path())))
            QSKIP("xattr tags not supported here");
        const QString a = dir.filePath(QStringLiteral("a.txt")), b = dir.filePath(QStringLiteral("b.txt"));
        touch(a);
        touch(b);
        KFileMetaData::UserMetaData(a).setTags({QStringLiteral("Red/One")});
        KFileMetaData::UserMetaData(b).setTags({QStringLiteral("Red/One")});
        FolderModel model;
        QVERIFY(list(model, dir));
        TagsColumns t(&model);
        auto word = [&](const QString &name) {
            for (int r = 0; r < t.rowCount(); ++r)
                if (t.itemFor(t.index(r, 0)).name() == name)
                    return t.index(r, 7).data(TagsColumns::TagsRole).value<QList<Tag>>().value(0).word;
            return QString();
        };
        QCOMPARE(word(QStringLiteral("a.txt")), QStringLiteral("One")); // fills the cache
        QCOMPARE(word(QStringLiteral("b.txt")), QStringLiteral("One"));
        KFileMetaData::UserMetaData(a).setTags({QStringLiteral("Red/Two")});
        KFileMetaData::UserMetaData(b).setTags({QStringLiteral("Red/Two")});
        int rowA = -1;
        for (int r = 0; r < model.rowCount(); ++r)
            if (model.itemFor(model.index(r, 0)).name() == QStringLiteral("a.txt"))
                rowA = r;
        Q_EMIT model.dataChanged(model.index(rowA, 0), model.index(rowA, 0));
        QCOMPARE(word(QStringLiteral("b.txt")), QStringLiteral("One")); // untouched row keeps its entry
        QCOMPARE(word(QStringLiteral("a.txt")), QStringLiteral("Two")); // changed row is re-read
        t.invalidateTags({QUrl::fromLocalFile(b)});
        QCOMPARE(word(QStringLiteral("b.txt")), QStringLiteral("Two"));
    }

    void invalidateTagsEmitsDataChanged()
    {
        QTemporaryDir dir;
        touch(dir.filePath(QStringLiteral("a.txt")));
        FolderModel model;
        QVERIFY(list(model, dir));
        TagsColumns t(&model);
        QVERIFY(t.index(0, 7).data(TagsColumns::TagsRole).value<QList<Tag>>().isEmpty());
        QSignalSpy spy(&t, &QAbstractItemModel::dataChanged);
        t.invalidateTags({QUrl::fromLocalFile(dir.filePath(QStringLiteral("a.txt")))});
        QVERIFY(spy.count() >= 1);
        bool col7 = false, col0 = false;
        for (const auto &a : spy) {
            col7 |= a.at(0).toModelIndex().column() <= 7 && a.at(1).toModelIndex().column() >= 7;
            col0 |= a.at(0).toModelIndex().column() == 0;
        }
        QVERIFY(col7 && col0);
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

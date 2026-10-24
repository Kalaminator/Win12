#include "emblem.h"
#include "tag.h"
#include "win12tagsoverlay.h"
#include <KConfigGroup>
#include <KFileMetaData/UserMetaData>
#include <KSharedConfig>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using KFileMetaData::UserMetaData;

class OverlayPluginTest : public QObject
{
    Q_OBJECT
    QTemporaryDir dir{QDir::homePath() + QStringLiteral("/.cache/overlayplugin-XXXXXX")};
    Win12TagsOverlay plugin;
    QUrl file(const QString &n, const QStringList &tags)
    {
        const QString p = dir.filePath(n);
        QFile f(p);
        (void)f.open(QIODevice::WriteOnly);
        f.close();
        if (!tags.isEmpty())
            UserMetaData(p).setTags(tags);
        return QUrl::fromLocalFile(p);
    }
    static void setShowFlags(bool on)
    {
        KSharedConfig::Ptr c = KSharedConfig::openConfig(QStringLiteral("win12-explorerrc"));
        c->group(QStringLiteral("Tags")).writeEntry("showFlags", on);
        c->sync();
    }
private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QDir().mkpath(QDir::homePath() + QStringLiteral("/.cache"));
        if (!dir.isValid() || !UserMetaData(dir.path()).isSupported())
            QSKIP("xattr tags not supported here");
    }
    void init() { setShowFlags(true); }
    void emblemExistsBeforeNameIsReturned()
    {
        const QString expected = Emblem::iconName({TagColours::indexOfName(QStringLiteral("Red")), TagColours::indexOfName(QStringLiteral("Gold"))});
        QFile::remove(Emblem::iconDir() + QLatin1Char('/') + expected + QStringLiteral(".svg"));
        const QUrl u = file(QStringLiteral("a"), {QStringLiteral("holiday"), QStringLiteral("Red/Xiaomi"), QStringLiteral("Gold")});
        const QStringList o = plugin.getOverlays(u);
        QCOMPARE(o, QStringList{expected});
        QVERIFY(QFile::exists(Emblem::iconDir() + QLatin1Char('/') + o.first() + QStringLiteral(".svg")));
    }
    void untaggedReturnsNothing() { QVERIFY(plugin.getOverlays(file(QStringLiteral("b"), {})).isEmpty()); }
    void foreignOnlyReturnsNothing() { QVERIFY(plugin.getOverlays(file(QStringLiteral("c"), {QStringLiteral("holiday")})).isEmpty()); }
    void flagsOffReturnsNothing()
    {
        const QUrl u = file(QStringLiteral("d"), {QStringLiteral("Red/Xiaomi")});
        QCOMPARE(plugin.getOverlays(u).size(), 1);
        setShowFlags(false);
        QVERIFY(plugin.getOverlays(u).isEmpty());
    }
    void remoteUrlReturnsNothing() { QVERIFY(plugin.getOverlays(QUrl(QStringLiteral("sftp://h/x"))).isEmpty()); }
    void cleanupTestCase() { setShowFlags(true); }
};

QTEST_GUILESS_MAIN(OverlayPluginTest)
#include "overlayplugin_test.moc"

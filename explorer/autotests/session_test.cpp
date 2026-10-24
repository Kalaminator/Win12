#include "explorerwindow.h"
#include <KConfigGroup>
#include <KSharedConfig>
#include <QDir>
#include <QTemporaryDir>
#include <QTest>

class SessionTest : public QObject
{
    Q_OBJECT
    QTemporaryDir m_dir;
    KSharedConfig::Ptr config(const QString &name) const
    {
        return KSharedConfig::openConfig(m_dir.filePath(name), KConfig::SimpleConfig);
    }

private Q_SLOTS:
    void missingFoldersAreDropped()
    {
        const QUrl existing = QUrl::fromLocalFile(m_dir.path());
        const QUrl remote(QStringLiteral("smb://nas/share"));
        QCOMPARE(ExplorerWindow::sessionUrlsToOpen({existing, QUrl(QStringLiteral("file:///nonexistent/x")), remote}),
                 (QList<QUrl>{existing, remote}));
        const QList<QUrl> home{QUrl::fromLocalFile(QDir::homePath())};
        QCOMPARE(ExplorerWindow::sessionUrlsToOpen({QUrl(QStringLiteral("file:///nonexistent"))}), home);
        QCOMPARE(ExplorerWindow::sessionUrlsToOpen({}), home);
    }

    void savedSessionRoundTrip()
    {
        const KSharedConfig::Ptr cfg = config(QStringLiteral("sessionrc"));
        KConfigGroup g(cfg, QStringLiteral("Session"));
        g.writeEntry("Url", QStringLiteral("file:///a"));
        g.writeEntry("SecondUrl", QStringLiteral("file:///b"));
        QCOMPARE(ExplorerWindow::savedSession(cfg), (QList<QUrl>{QUrl(QStringLiteral("file:///a")), QUrl(QStringLiteral("file:///b"))}));
    }

    void savedSessionUrlOnly()
    {
        const KSharedConfig::Ptr cfg = config(QStringLiteral("urlonlyrc"));
        KConfigGroup(cfg, QStringLiteral("Session")).writeEntry("Url", QStringLiteral("file:///a"));
        QCOMPARE(ExplorerWindow::savedSession(cfg).size(), 1);
    }

    void savedSessionIgnoresOldTabs()
    {
        const KSharedConfig::Ptr cfg = config(QStringLiteral("oldtabsrc"));
        KConfigGroup g(cfg, QStringLiteral("Session"));
        g.writeEntry("Tabs", QStringList{QStringLiteral("file:///a")});
        g.writeEntry("Active", 0);
        QVERIFY(ExplorerWindow::savedSession(cfg).isEmpty());
        QCOMPARE(ExplorerWindow::sessionUrlsToOpen(ExplorerWindow::savedSession(cfg)),
                 (QList<QUrl>{QUrl::fromLocalFile(QDir::homePath())}));
    }

    void recentFoldersDedupAndTrim()
    {
        const KSharedConfig::Ptr cfg = config(QStringLiteral("recentrc"));
        ExplorerWindow::addRecentFolder(cfg, QUrl(QStringLiteral("file:///a")));
        ExplorerWindow::addRecentFolder(cfg, QUrl(QStringLiteral("file:///b")));
        ExplorerWindow::addRecentFolder(cfg, QUrl(QStringLiteral("file:///a")));
        QCOMPARE(ExplorerWindow::recentFolders(cfg), (QStringList{QStringLiteral("file:///a"), QStringLiteral("file:///b")}));
        for (int i = 0; i < 25; ++i)
            ExplorerWindow::addRecentFolder(cfg, QUrl(QStringLiteral("file:///d%1").arg(i)));
        const QStringList recent = ExplorerWindow::recentFolders(cfg);
        QCOMPARE(recent.size(), 20);
        QCOMPARE(recent.first(), QStringLiteral("file:///d24"));
        QCOMPARE(recent.last(), QStringLiteral("file:///d5"));
    }
};

QTEST_GUILESS_MAIN(SessionTest)
#include "session_test.moc"

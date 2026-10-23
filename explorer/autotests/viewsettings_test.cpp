#include "viewsettings.h"
#include <QTemporaryFile>
#include <QTest>
using namespace Qt::StringLiterals;

class ViewSettingsTest : public QObject
{
    Q_OBJECT
    QTemporaryFile m_file;
    KSharedConfig::Ptr cfg() { return KSharedConfig::openConfig(m_file.fileName(), KConfig::SimpleConfig); }
private Q_SLOTS:
    void initTestCase() { QVERIFY(m_file.open()); }
    void init() // KSharedConfig instances are cached per path: wipe state, not just the file
    {
        cfg()->deleteGroup(u"View"_s);
        cfg()->deleteGroup(u"Folders"_s);
    }
    void defaultsMatchSpec()
    {
        ViewSettings s;
        QCOMPARE(s.mode, ViewMode::Details);
        QCOMPARE(s.columns, (QStringList{u"name"_s, u"modified"_s, u"type"_s, u"size"_s}));
        QVERIFY(s.previews);
        QVERIFY(!s.hiddenFiles);
    }
    void iconSizes()
    {
        QCOMPARE(iconSizeFor(ViewMode::ExtraLargeIcons), 256);
        QCOMPARE(iconSizeFor(ViewMode::LargeIcons), 96);
        QCOMPARE(iconSizeFor(ViewMode::MediumIcons), 48);
        QCOMPARE(iconSizeFor(ViewMode::SmallIcons), 16);
        QCOMPARE(iconSizeFor(ViewMode::Content), 32);
    }
    void zoomSteps()
    {
        QCOMPARE(zoomStep(ViewMode::Details, 1), ViewMode::List);
        QCOMPARE(zoomStep(ViewMode::Details, 3), ViewMode::MediumIcons);
        QCOMPARE(zoomStep(ViewMode::ExtraLargeIcons, 1), ViewMode::ExtraLargeIcons);
        QCOMPARE(zoomStep(ViewMode::Details, -1), ViewMode::Details);
        QCOMPARE(zoomStep(ViewMode::Tiles, 1), ViewMode::LargeIcons);
        QCOMPARE(zoomStep(ViewMode::Content, -1), ViewMode::SmallIcons);
    }
    void columnsMap()
    {
        QCOMPARE(modelColumnFor(u"tags"_s), 7);
        QCOMPARE(modelColumnFor(u"modified"_s), 2);
        QCOMPARE(knownColumnIds().size(), 8);
        QCOMPARE(columnTitle(u"modified"_s), u"Date modified"_s);
    }
    void globalRoundTrip()
    {
        ViewSettingsStore st(cfg());
        ViewSettings s;
        s.mode = ViewMode::LargeIcons;
        s.groupRole = GroupRole::Modified;
        s.columns = {u"name"_s, u"tags"_s};
        st.save(QUrl(u"file:///tmp/a"_s), s);
        QCOMPARE(st.load(QUrl(u"file:///tmp/b"_s)), s);
    }
    void perFolderRoundTrip()
    {
        ViewSettingsStore st(cfg());
        st.setPerFolder(true);
        const QUrl a(u"file:///tmp/a"_s), b(u"file:///tmp/b"_s);
        ViewSettings s;
        s.mode = ViewMode::Tiles;
        st.save(a, s);
        QCOMPARE(st.load(a).mode, ViewMode::Tiles);
        QCOMPARE(st.load(b).mode, ViewMode::Details);
        QVERIFY(st.hasFolderSettings(a));
        QVERIFY(!st.hasFolderSettings(b));
        st.clearFolder(a);
        QVERIFY(!st.hasFolderSettings(a));
    }
    void reopenPersists()
    {
        ViewSettings s;
        s.mode = ViewMode::SmallIcons;
        s.sortRole = SortRole::Size;
        s.sortOrder = Qt::DescendingOrder;
        s.hiddenFiles = true;
        {
            ViewSettingsStore st(cfg());
            st.setPerFolder(true);
            st.save(QUrl(u"file:///x"_s), s);
        }
        ViewSettingsStore st2(KSharedConfig::openConfig(m_file.fileName(), KConfig::SimpleConfig));
        QVERIFY(st2.perFolder());
        QCOMPARE(st2.load(QUrl(u"file:///x"_s)), s);
    }
};
QTEST_GUILESS_MAIN(ViewSettingsTest)
#include "viewsettings_test.moc"

#include "exploreractions.h"
#include "ribbonbar.h"
#include "ribbonspec.h"
#include "titlebar.h"
#include <KColorScheme>
#include <KConfigGroup>
#include <KSharedConfig>
#include <QTemporaryDir>
#include <QAction>
#include <QLabel>
#include <QMenu>
#include <QSignalSpy>
#include <QStackedWidget>
#include <QTest>
#include <QToolButton>

using namespace Qt::StringLiterals;

class RibbonBarTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void tabsAndFileMenu()
    {
        ExplorerActions a(nullptr);
        RibbonBar r(&a);
        QStringList t;
        for (QAction *x : r.fileMenu()->actions())
            t << x->text();
        QCOMPARE(t, (QStringList{u"New window"_s, u"Open terminal here"_s, u"Options"_s, u"Help"_s, u"Close"_s}));
        QCOMPARE(r.currentTab(), 0);
    }
    void homeGroupsVisible()
    {
        ExplorerActions a(nullptr);
        RibbonBar r(&a);
        QCOMPARE(r.visibleGroupTitles(0), (QStringList{u"Clipboard"_s, u"Organize"_s, u"New"_s, u"Open"_s, u"Select"_s}));
    }
    void tagsGroupShowsBuiltButtons()
    {
        ExplorerActions a(nullptr);
        RibbonBar r(&a);
        QCOMPARE(r.visibleGroupTitles(1),
                 (QStringList{u"Panes"_s, u"Layout"_s, u"Current view"_s, u"Tags"_s, u"Show/hide"_s, u"Split view"_s}));
    }
    void advancedShowsBuiltGroups()
    {
        ExplorerActions a(nullptr);
        RibbonBar r(&a);
        QCOMPARE(r.visibleGroupTitles(2),
                 (QStringList{u"Select"_s, u"Rename"_s, u"Bulk actions"_s, u"Automate"_s, u"Share"_s}));
    }
    void everyVisibleButtonHasItsAction()
    {
        ExplorerActions a(nullptr);
        RibbonBar r(&a);
        for (const Ribbon::Tab &t : Ribbon::spec())
            for (const Ribbon::Group &g : t.groups) {
                if (!Ribbon::phaseBuilt(g.phase) || !Ribbon::phaseBuilt(t.phase) || t.isFileMenu)
                    continue;
                for (const Ribbon::Button &b : g.buttons) {
                    QCOMPARE(r.buttonsFor(b.action).size(), 1);
                    QCOMPARE(r.buttonsFor(b.action).first()->defaultAction(), a.action(b.action));
                }
            }
        QCOMPARE(r.buttonsFor(u"home_delete"_s).first()->popupMode(), QToolButton::MenuButtonPopup);
    }
    void collapseHidesBody()
    {
        ExplorerActions a(nullptr);
        RibbonBar r(&a);
        r.show();
        QVERIFY(r.height() > 100);
        QSignalSpy spy(&r, &RibbonBar::collapsedChanged);
        r.collapseAction()->trigger();
        QVERIFY(r.isCollapsed());
        QVERIFY(!r.findChild<QStackedWidget *>()->isVisible());
        QVERIFY(r.childAt(5, 5)->isVisible()); // the tab strip stays
        QCOMPARE(spy.size(), 1);
        QCOMPARE(spy.first().first().toBool(), true);
        QCOMPARE(r.collapseAction()->shortcut(), QKeySequence(Qt::CTRL | Qt::Key_F1));
    }
    void clickingTabWhileCollapsedExpands()
    {
        ExplorerActions a(nullptr);
        RibbonBar r(&a);
        r.show();
        r.setCollapsed(true);
        QWidget *strip = r.tabStrip();
        const int x = strip->fontMetrics().horizontalAdvance(u"File"_s) + 28 + 10; // inside the Home tab
        QTest::mouseClick(strip, Qt::LeftButton, {}, QPoint(x, 20));
        QVERIFY(!r.isCollapsed());
        QVERIFY(r.findChild<QStackedWidget *>()->isVisible());
    }
    void wrappedLabelSurvivesActionChanges()
    {
        ExplorerActions a(nullptr);
        RibbonBar r(&a);
        r.show();
        QToolButton *b = r.buttonsFor(u"home_pin"_s).first();
        QVERIFY(b->text().contains(u'\n'));
        const int w = b->width();
        a.action(u"home_pin"_s)->setEnabled(false);
        a.action(u"home_pin"_s)->setEnabled(true);
        QCoreApplication::processEvents();
        QVERIFY(b->text().contains(u'\n'));
        QCOMPARE(b->width(), w);
    }
    void fileTabUsesAccent()
    {
        ExplorerActions a(nullptr);
        RibbonBar r(&a);
        r.show();
        const QImage img = r.grab().toImage();
        // a few px inside the File tab, clear of the label glyphs
        QCOMPARE(img.pixelColor(4, 14), r.palette().highlight().color());
    }
    void unselectedTabShowsTitleColour()
    {
        ExplorerActions a(nullptr);
        RibbonBar r(&a);
        r.setTabStripColours(Qt::red, Qt::white);
        r.show();
        QWidget *strip = r.tabStrip();
        const QFontMetrics fm = strip->fontMetrics();
        const int fileW = fm.horizontalAdvance(u"File"_s) + 28;
        const int homeX = fileW + 4; // inside the tab, clear of the label glyphs
        const int viewX = fileW + fm.horizontalAdvance(u"Home"_s) + 28 + 4;
        const QImage img = strip->grab().toImage();
        QCOMPARE(img.pixelColor(homeX, 3), QColor(Qt::red));
        QCOMPARE(img.pixelColor(homeX, 20), r.palette().window().color());
        QCOMPARE(img.pixelColor(viewX, 20), QColor(Qt::red));
    }
    static KSharedConfig::Ptr tempConfig(QTemporaryDir &dir)
    {
        return KSharedConfig::openConfig(dir.filePath(u"kdeglobals"_s), KConfig::SimpleConfig);
    }
    void readColoursFromWm()
    {
        QTemporaryDir d;
        auto cfg = tempConfig(d);
        KConfigGroup g(cfg, u"WM"_s);
        g.writeEntry("activeBackground", QColor(232, 17, 35));
        g.writeEntry("activeForeground", QColor(255, 255, 255));
        g.writeEntry("inactiveBackground", QColor(221, 126, 141));
        g.writeEntry("inactiveForeground", QColor(245, 216, 221));
        cfg->sync();
        const WmColours c = TitleBar::readColours(cfg);
        QCOMPARE(c.activeBg, QColor(232, 17, 35));
        QCOMPARE(c.activeFg, QColor(255, 255, 255));
        QCOMPARE(c.inactiveBg, QColor(221, 126, 141));
        QCOMPARE(c.inactiveFg, QColor(245, 216, 221));
    }
    void readColoursFallsBackToHeader()
    {
        QTemporaryDir d;
        const WmColours c = TitleBar::readColours(tempConfig(d));
        const KColorScheme act(QPalette::Active, KColorScheme::Header);
        const KColorScheme ina(QPalette::Inactive, KColorScheme::Header);
        QCOMPARE(c.activeBg, act.background().color());
        QCOMPARE(c.activeFg, act.foreground().color());
        QCOMPARE(c.inactiveBg, ina.background().color());
        QCOMPARE(c.inactiveFg, ina.foreground().color());
    }
    void readColoursMixesPerKey()
    {
        QTemporaryDir d;
        auto cfg = tempConfig(d);
        KConfigGroup(cfg, u"WM"_s).writeEntry("activeBackground", QColor(1, 2, 3));
        cfg->sync();
        const WmColours c = TitleBar::readColours(cfg);
        QCOMPARE(c.activeBg, QColor(1, 2, 3));
        QCOMPARE(c.activeFg, KColorScheme(QPalette::Active, KColorScheme::Header).foreground().color());
        QCOMPARE(c.inactiveBg, KColorScheme(QPalette::Inactive, KColorScheme::Header).background().color());
        QCOMPARE(c.inactiveFg, KColorScheme(QPalette::Inactive, KColorScheme::Header).foreground().color());
    }
    void inactiveColours()
    {
        QTemporaryDir d;
        auto cfg = tempConfig(d);
        KConfigGroup g(cfg, u"WM"_s);
        g.writeEntry("activeBackground", QColor(1, 2, 3));
        g.writeEntry("inactiveBackground", QColor(4, 5, 6));
        cfg->sync();
        ExplorerActions a(nullptr);
        RibbonBar r(&a);
        TitleBar tb(&r, cfg);
        QVERIFY(tb.isActive());
        QCOMPARE(tb.background(), QColor(1, 2, 3));
        tb.setActive(false);
        QCOMPARE(tb.background(), QColor(4, 5, 6));
    }
    void reloadColoursPicksUpChange()
    {
        QTemporaryDir d;
        auto cfg = tempConfig(d);
        KConfigGroup(cfg, u"WM"_s).writeEntry("activeBackground", QColor(1, 2, 3));
        cfg->sync();
        ExplorerActions a(nullptr);
        RibbonBar r(&a);
        TitleBar tb(&r, cfg);
        QCOMPARE(tb.background(), QColor(1, 2, 3));
        KConfigGroup(cfg, u"WM"_s).writeEntry("activeBackground", QColor(9, 8, 7));
        cfg->sync();
        tb.reloadColours();
        QCOMPARE(tb.background(), QColor(9, 8, 7));
    }
    void titleBarHostsTheStrip()
    {
        QTemporaryDir d;
        ExplorerActions a(nullptr);
        RibbonBar r(&a);
        TitleBar tb(&r, tempConfig(d));
        QVERIFY(tb.isAncestorOf(r.tabStrip()));
        QVERIFY(r.findChild<QStackedWidget *>());
        tb.show();
        QCOMPARE(tb.height(), 32);
    }
    void closeButtonClosesHost()
    {
        QTemporaryDir d;
        ExplorerActions a(nullptr);
        RibbonBar r(&a);
        QWidget host;
        auto *tb = new TitleBar(&r, tempConfig(d), &host);
        host.show();
        QTest::mouseClick(tb->closeButton(), Qt::LeftButton);
        QTRY_VERIFY(!host.isVisible());
    }
};

QTEST_MAIN(RibbonBarTest)
#include "ribbonbar_test.moc"

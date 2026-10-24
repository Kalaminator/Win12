#include "exploreractions.h"
#include "ribbonbar.h"
#include "ribbonspec.h"
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
        QWidget *strip = r.childAt(5, 5);
        const int x = strip->fontMetrics().horizontalAdvance(u"File"_s) + 28 + 10; // inside the Home tab
        QTest::mouseClick(strip, Qt::LeftButton, {}, QPoint(x, 14));
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
};

QTEST_MAIN(RibbonBarTest)
#include "ribbonbar_test.moc"

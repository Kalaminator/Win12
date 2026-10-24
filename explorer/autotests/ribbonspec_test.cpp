#include "ribbonspec.h"
#include <QRegularExpression>
#include <QSet>
#include <QtTest>

using namespace Qt::StringLiterals;

class RibbonSpecTest : public QObject
{
    Q_OBJECT
    static const Ribbon::Tab *tab(const QString &id)
    {
        for (const auto &t : Ribbon::spec())
            if (t.id == id)
                return &t;
        return nullptr;
    }
    static QStringList groupTexts(const QString &id)
    {
        QStringList r;
        for (const auto &g : tab(id)->groups)
            r << g.text;
        return r;
    }
private Q_SLOTS:
    void tabOrder()
    {
        QStringList ids;
        for (const auto &t : Ribbon::spec())
            ids << t.id;
        QCOMPARE(ids, (QStringList{u"file"_s, u"home"_s, u"share"_s, u"view"_s, u"advanced"_s}));
        QVERIFY(Ribbon::spec()[0].isFileMenu);
        QCOMPARE(Ribbon::spec()[4].phase, 3);
    }
    void homeGroups()
    {
        QCOMPARE(groupTexts(u"home"_s), (QStringList{u"Clipboard"_s, u"Organize"_s, u"New"_s, u"Open"_s, u"Select"_s}));
        QCOMPARE(groupTexts(u"view"_s),
                 (QStringList{u"Panes"_s, u"Layout"_s, u"Current view"_s, u"Tags"_s, u"Show/hide"_s, u"Split view"_s}));
        QCOMPARE(tab(u"view"_s)->groups[3].phase, 2);
    }
    void tagsGroupHasPhase2Buttons()
    {
        const auto &g = tab(u"view"_s)->groups[3];
        QStringList ids;
        for (const auto &b : g.buttons)
            ids << b.action;
        QCOMPARE(ids, (QStringList{u"tags_add"_s, u"tags_remove"_s, u"tags_manage"_s, u"tags_group_name"_s, u"tags_group_colour"_s,
                                   u"tags_flags"_s}));
        QCOMPARE(Ribbon::button(u"tags_add"_s)->size, Ribbon::Size::Big);
        QVERIFY(Ribbon::button(u"tags_add"_s)->menu);
        QVERIFY(Ribbon::button(u"tags_remove"_s)->menu);
        QVERIFY(!Ribbon::button(u"tags_manage"_s)->menu);
    }
    void actionIdsUniqueAndNamed()
    {
        const QStringList ids = Ribbon::actionIds();
        QCOMPARE(ids.size(), 69);
        QCOMPARE(QSet<QString>(ids.begin(), ids.end()).size(), 69);
        const QRegularExpression re(u"^(file|home|share|view|adv|tags)_[a-z_]+$"_s);
        for (const auto &id : ids)
            QVERIFY2(re.match(id).hasMatch(), qPrintable(id));
    }
    void automateGroupHasFourButtons()
    {
        const auto &g = tab(u"advanced"_s)->groups[3];
        QStringList ids;
        for (const auto &b : g.buttons)
            ids << b.action;
        QCOMPARE(ids, (QStringList{u"adv_formula"_s, u"adv_script"_s, u"adv_my_rules"_s, u"adv_my_scripts"_s}));
        QVERIFY(Ribbon::button(u"adv_my_rules"_s)->menu);
        QVERIFY(Ribbon::button(u"adv_my_scripts"_s)->menu);
        QVERIFY(!Ribbon::button(u"adv_formula"_s)->menu);
        QVERIFY(Ribbon::phaseBuilt(4));
    }
    void advancedTabHasPhase3Groups()
    {
        QCOMPARE(groupTexts(u"advanced"_s), (QStringList{u"Select"_s, u"Rename"_s, u"Bulk actions"_s, u"Automate"_s}));
        QCOMPARE(tab(u"advanced"_s)->groups[3].phase, 4);
        const QStringList ids = Ribbon::actionIds();
        for (const char *id : {"adv_select_rule", "adv_rename_many", "adv_rename_undo", "adv_select_matches",
                               "adv_copy_matches", "adv_move_matches", "adv_rename_matches", "adv_trash_matches", "adv_tag_matches"})
            QVERIFY2(ids.contains(QString::fromLatin1(id)), id);
        QCOMPARE(Ribbon::button(u"adv_trash_matches"_s)->size, Ribbon::Size::Big);
    }
    void menuButtons()
    {
        QVERIFY(Ribbon::button(u"home_delete"_s)->menu);
        QVERIFY(Ribbon::button(u"view_sort"_s)->menu);
        QVERIFY(!Ribbon::button(u"home_copy"_s)->menu);
        QCOMPARE(Ribbon::button(u"nope"_s), nullptr);
    }
    void everyButtonHasTextAndIcon()
    {
        for (const auto &id : Ribbon::actionIds()) {
            QVERIFY2(!Ribbon::button(id)->text.isEmpty(), qPrintable(id));
            QVERIFY2(!Ribbon::button(id)->icon.isEmpty(), qPrintable(id));
        }
    }
};
QTEST_GUILESS_MAIN(RibbonSpecTest)
#include "ribbonspec_test.moc"

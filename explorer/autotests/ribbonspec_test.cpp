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
        QVERIFY(Ribbon::spec()[4].groups.isEmpty());
    }
    void homeGroups()
    {
        QCOMPARE(groupTexts(u"home"_s), (QStringList{u"Clipboard"_s, u"Organize"_s, u"New"_s, u"Open"_s, u"Select"_s}));
        QCOMPARE(groupTexts(u"view"_s),
                 (QStringList{u"Panes"_s, u"Layout"_s, u"Current view"_s, u"Tags"_s, u"Show/hide"_s, u"Split view"_s}));
        QCOMPARE(tab(u"view"_s)->groups[3].phase, 2);
    }
    void actionIdsUniqueAndNamed()
    {
        const QStringList ids = Ribbon::actionIds();
        QCOMPARE(ids.size(), 50);
        QCOMPARE(QSet<QString>(ids.begin(), ids.end()).size(), 50);
        const QRegularExpression re(u"^(file|home|share|view)_[a-z_]+$"_s);
        for (const auto &id : ids)
            QVERIFY2(re.match(id).hasMatch(), qPrintable(id));
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

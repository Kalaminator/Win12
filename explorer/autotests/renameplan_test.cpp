#include "renameplan.h"
#include <QTest>

static QString S(const char *c)
{
    return QString::fromUtf8(c);
}
static RenameBlock B(RenameBlock::Kind k, const QString &text = {})
{
    RenameBlock b;
    b.kind = k;
    b.text = text;
    return b;
}
static RenameItem I(const char *name, const QDateTime &m = {})
{
    return {QUrl::fromLocalFile(S("/d/") + S(name)), S(name), m};
}
using Blocks = QList<RenameBlock>;
using Items = QList<RenameItem>;
using Names = QSet<QString>;
static Names N(std::initializer_list<const char *> l)
{
    Names r;
    for (auto s : l)
        r << S(s);
    return r;
}

class RenamePlanTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void numberDigitsStartStep()
    {
        RenameBlock n = B(RenameBlock::Number);
        n.digits = 3;
        n.start = 5;
        n.step = 2;
        QCOMPARE(RenamePlan::buildName({n}, I("a.txt"), 0), S("005"));
        QCOMPARE(RenamePlan::buildName({n}, I("a.txt"), 1), S("007"));
        n.digits = 1;
        QCOMPARE(RenamePlan::buildName({n}, I("a.txt"), 20), S("45"));
    }
    void textAndCurrentName()
    {
        QCOMPARE(RenamePlan::buildName({B(RenameBlock::Text, S(" - ")), B(RenameBlock::CurrentName)}, I("holiday.jpg"), 0), S(" - holiday"));
    }
    void extensionAlwaysKept()
    {
        Blocks b{B(RenameBlock::Number), B(RenameBlock::Text, S(" - ")), B(RenameBlock::CurrentName)};
        auto p = RenamePlan::plan(b, {I("holiday.jpg")}, N({"holiday.jpg"}));
        QCOMPARE(p[0].newName, S("01 - holiday.jpg"));
        p = RenamePlan::plan({B(RenameBlock::Text, S("x"))}, {I("archive.tar.gz")}, {});
        QCOMPARE(p[0].newName, S("x.gz"));
        p = RenamePlan::plan({B(RenameBlock::CurrentName)}, {I(".bashrc")}, N({".bashrc"}));
        QCOMPARE(p[0].newName, S(".bashrc"));
        QCOMPARE(p[0].status, PlannedRename::Unchanged);
        p = RenamePlan::plan({B(RenameBlock::Text, S("x"))}, {I(".bashrc")}, {});
        QCOMPARE(p[0].newName, S("x"));
    }
    void dateUsesModified()
    {
        QDateTime m(QDate(2026, 3, 9), QTime(1, 2));
        RenameBlock d = B(RenameBlock::Date, S("yyyy-MM-dd"));
        QCOMPARE(RenamePlan::buildName({d}, I("a.txt", m), 0), S("2026-03-09"));
    }
    void findReplaceAppliesToBuiltName()
    {
        RenameBlock f = B(RenameBlock::FindReplace, S("o"));
        f.replace = S("0");
        QCOMPARE(RenamePlan::buildName({B(RenameBlock::CurrentName), B(RenameBlock::Text, S("-foo")), f}, I("book.txt"), 0), S("b00k-f00"));
    }
    void caseModes()
    {
        Blocks b{B(RenameBlock::CurrentName), B(RenameBlock::Case)};
        RenameItem it = I("hELLO wORLD.txt");
        b[1].caseMode = RenameBlock::Upper;
        QCOMPARE(RenamePlan::buildName(b, it, 0), S("HELLO WORLD"));
        b[1].caseMode = RenameBlock::Lower;
        QCOMPARE(RenamePlan::buildName(b, it, 0), S("hello world"));
        b[1].caseMode = RenameBlock::Title;
        QCOMPARE(RenamePlan::buildName(b, it, 0), S("Hello World"));
    }
    void clashWithExistingNameIsSkipped()
    {
        Names ex = N({"notes.txt", "Pictures", "a.txt", "b"});
        auto p = RenamePlan::plan({B(RenameBlock::Text, S("notes"))}, {I("a.txt")}, ex);
        QCOMPARE(p[0].status, PlannedRename::ClashExisting);
        p = RenamePlan::plan({B(RenameBlock::Text, S("Pictures"))}, {I("b")}, ex);
        QCOMPARE(p[0].status, PlannedRename::ClashExisting);
    }
    void clashWithOtherCurrentNameIsSkipped()
    {
        // b.txt is another item's current name: a.txt -> b.txt is skipped
        auto p = RenamePlan::plan({B(RenameBlock::Text, S("b"))}, {I("a.txt"), I("b.txt")}, N({"a.txt", "b.txt"}));
        QCOMPARE(p[0].status, PlannedRename::ClashExisting);
        QCOMPARE(p[1].status, PlannedRename::Unchanged);
    }
    void clashWithPlannedNameSkipsBoth()
    {
        auto p = RenamePlan::plan({B(RenameBlock::Text, S("same"))}, {I("a.txt"), I("b.txt"), I("c.md")}, N({"a.txt", "b.txt", "c.md"}));
        QCOMPARE(p[0].status, PlannedRename::ClashPlanned);
        QCOMPARE(p[1].status, PlannedRename::ClashPlanned);
        QCOMPARE(p[2].status, PlannedRename::Rename);
    }
    void unchangedIsNotARename()
    {
        auto p = RenamePlan::plan({B(RenameBlock::CurrentName)}, {I("a.txt")}, N({"a.txt"}));
        QCOMPARE(p[0].status, PlannedRename::Unchanged);
        p = RenamePlan::plan({}, {I("a.txt")}, N({"a.txt"}));
        QCOMPARE(p[0].status, PlannedRename::Unchanged);
    }
    void numberFollowsListPosition()
    {
        auto p = RenamePlan::plan({B(RenameBlock::Number)}, {I("01.txt"), I("b.txt")}, N({"01.txt", "b.txt"}));
        QCOMPARE(p[0].status, PlannedRename::Unchanged);
        QCOMPARE(p[1].newName, S("02.txt"));
    }
    void undoPlanReversesOnlyExecuted()
    {
        auto p = RenamePlan::plan({B(RenameBlock::Text, S("n"))}, {I("a.txt"), I("b.txt"), I("c.txt")}, N({"a.txt", "b.txt", "c.txt"}));
        p[0].newName = S("x.txt");
        p[2].newName = S("z.txt");
        p[0].status = p[2].status = PlannedRename::Rename;
        p[1].status = PlannedRename::ClashExisting;
        auto u = RenamePlan::undoPlan(p);
        QCOMPARE(u.size(), 2);
        QCOMPARE(u[0].second, S("c.txt"));
        QCOMPARE(u[0].first, QUrl::fromLocalFile(S("/d/z.txt")));
        QCOMPARE(u[1].second, S("a.txt"));
    }
};

QTEST_GUILESS_MAIN(RenamePlanTest)
#include "renameplan_test.moc"

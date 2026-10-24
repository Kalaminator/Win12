#include "script.h"
#include <QTest>

using namespace Qt::Literals::StringLiterals;

// Structural equality of two trees (kind, payload, name, op, args).
static bool same(const FxExpr &a, const FxExpr &b)
{
    if (a->kind != b->kind || !(a->literal == b->literal) || a->name != b->name || a->op != b->op || a->args.size() != b->args.size())
        return false;
    for (size_t i = 0; i < a->args.size(); ++i)
        if (!same(a->args[i], b->args[i]))
            return false;
    return true;
}

class ScriptTest : public QObject
{
    Q_OBJECT

    static Script parse(const QString &text)
    {
        FxParseError err;
        const auto s = parseScript(text, &err);
        if (!s)
            qWarning() << "parse error" << err.message << err.line << err.col;
        return s ? *s : Script{};
    }
    static FxParseError error(const QString &text)
    {
        FxParseError err;
        if (parseScript(text, &err))
            err.message = u"<parsed>"_s;
        return err;
    }

private Q_SLOTS:
    void mockupScriptParses()
    {
        const Script s = parse(
            u"For Each f In Files(\"~/Downloads\", Subfolders:=True)\n"
            "  If f.Ext = \"pdf\" And f.Contains(\"invoice\") Then\n"
            "    f.MoveTo \"~/Documents/Invoices/\" & YEAR(f.Modified)\n"
            "    f.Tag \"Invoice\", \"Orange\"\n"
            "  ElseIf f.Size = 0 Then\n"
            "    f.Trash\n"
            "  End If\n"
            "Next\n"_s);
        QCOMPARE(s.body.size(), size_t(1));
        const ScriptStmt &loop = s.body[0];
        QCOMPARE(loop.kind, ScriptStmt::ForEach);
        QCOMPARE(loop.var, u"f"_s);
        QCOMPARE(loop.expr->kind, FxNode::Call);
        QCOMPARE(loop.expr->name, u"FILES"_s);
        QCOMPARE(loop.expr->args.size(), size_t(2));
        QCOMPARE(loop.body.size(), size_t(1));
        const ScriptStmt &ifs = loop.body[0];
        QCOMPARE(ifs.kind, ScriptStmt::If);
        QCOMPARE(ifs.line, 2);
        QCOMPARE(ifs.branches.size(), size_t(2));
        QCOMPARE(ifs.branches[0].first->kind, FxNode::Binary);
        QCOMPARE(ifs.branches[0].first->op, u"AND"_s);
        const auto &b0 = ifs.branches[0].second;
        QCOMPARE(b0.size(), size_t(2));
        QCOMPARE(b0[0].kind, ScriptStmt::Action);
        QCOMPARE(b0[0].action, u"MoveTo"_s);
        QCOMPARE(b0[0].line, 3);
        QCOMPARE(b0[0].target->kind, FxNode::Variable);
        QCOMPARE(b0[0].target->name, u"f"_s);
        QCOMPARE(b0[0].args.size(), size_t(1));
        QCOMPARE(b0[0].args[0]->kind, FxNode::Binary);
        QCOMPARE(b0[0].args[0]->op, u"&"_s);
        QCOMPARE(b0[1].action, u"Tag"_s);
        QCOMPARE(b0[1].args.size(), size_t(2));
        const auto &b1 = ifs.branches[1].second;
        QCOMPARE(b1.size(), size_t(1));
        QCOMPARE(b1[0].action, u"Trash"_s);
        QCOMPARE(b1[0].args.size(), size_t(0));
        QVERIFY(ifs.elseBody.empty());
    }
    void assignAndDim()
    {
        const Script s = parse(u"Dim n As Integer\nn = 1\nx = n + 1"_s);
        QCOMPARE(s.body.size(), size_t(3));
        for (const ScriptStmt &st : s.body)
            QCOMPARE(st.kind, ScriptStmt::Assign);
        QCOMPARE(s.body[0].var, u"n"_s);
        QVERIFY(!s.body[0].expr);
        QCOMPARE(s.body[1].var, u"n"_s);
        QVERIFY(s.body[1].expr);
        QCOMPARE(s.body[2].var, u"x"_s);
        QCOMPARE(s.body[2].expr->kind, FxNode::Binary);
        QCOMPARE(s.body[2].line, 3);
        const Script d = parse(u"dim k = 2"_s);
        QCOMPARE(d.body.size(), size_t(1));
        QVERIFY(d.body[0].expr);
    }
    void elseBranch()
    {
        const Script s = parse(u"If x > 1 Then\n  y = 1\nElse\n  y = 2\n  y = 3\nEnd If"_s);
        QCOMPARE(s.body.size(), size_t(1));
        QCOMPARE(s.body[0].branches.size(), size_t(1));
        QCOMPARE(s.body[0].branches[0].second.size(), size_t(1));
        QCOMPARE(s.body[0].elseBody.size(), size_t(2));
    }
    void exitFor()
    {
        const Script s = parse(u"for each f in files(\"~\")\n  if f.Size = 0 then\n    exit for\n  end if\nnext f"_s);
        QCOMPARE(s.body.size(), size_t(1));
        QCOMPARE(s.body[0].body.at(0).branches.at(0).second.at(0).kind, ScriptStmt::ExitFor);
    }
    void missingNext()
    {
        const FxParseError e = error(u"For Each f In Files(\"~\")\n  f.Trash\n"_s);
        QCOMPARE(e.message, u"Expected Next"_s);
        QCOMPARE(e.line, 1);
    }
    void missingEndIf()
    {
        const FxParseError e = error(u"x = 1\nIf x = 1 Then\n  x = 2\n"_s);
        QCOMPARE(e.message, u"Expected End If"_s);
        QCOMPARE(e.line, 2);
        // a Next closing the loop before the If is closed
        const FxParseError e2 = error(u"For Each f In Files(\"~\")\n  If x Then\nNext"_s);
        QCOMPARE(e2.message, u"Expected End If"_s);
        QCOMPARE(e2.line, 2);
    }
    void nextWithoutFor()
    {
        const FxParseError e = error(u"x = 1\nNext"_s);
        QCOMPARE(e.message, u"Next without For"_s);
        QCOMPARE(e.line, 2);
    }
    void exitForOutsideLoop()
    {
        const FxParseError e = error(u"If x Then\n  Exit For\nEnd If"_s);
        QCOMPARE(e.message, u"Exit For outside a loop"_s);
        QCOMPARE(e.line, 2);
    }
    void unknownAction()
    {
        const FxParseError e = error(u"For Each f In Files(\"~\")\n  f.Explode\nNext"_s);
        QCOMPARE(e.message, u"Unknown action Explode"_s);
        QCOMPARE(e.line, 2);
    }
    void forEachNeedsFiles()
    {
        QCOMPARE(error(u"For Each f In x\nNext"_s).message, u"For Each needs Files(…)"_s);
        QCOMPARE(error(u"For Each f In Files(Subfolders:=True)\nNext"_s).message, u"Files needs a folder"_s);
        QCOMPARE(parse(u"For Each f In Files(\"~\", True)\nNext"_s).body.size(), size_t(1));
    }
    void actionArity()
    {
        QCOMPARE(error(u"For Each f In Files(\"~\")\n  f.MoveTo\nNext"_s).message, u"MoveTo takes 1 argument"_s);
        QCOMPARE(error(u"For Each f In Files(\"~\")\n  f.Tag \"a\", \"b\", \"c\"\nNext"_s).message, u"Tag takes 1 to 2 arguments"_s);
        QCOMPARE(error(u"For Each f In Files(\"~\")\n  f.Trash \"x\"\nNext"_s).message, u"Trash takes no arguments"_s);
    }
    void parenthesisedActionArgs()
    {
        const Script a = parse(u"For Each f In Files(\"~\")\n  f.Tag(\"a\", \"Red\")\nNext"_s);
        const Script b = parse(u"For Each f In Files(\"~\")\n  f.Tag \"a\", \"Red\"\nNext"_s);
        const ScriptStmt &x = a.body.at(0).body.at(0), &y = b.body.at(0).body.at(0);
        QCOMPARE(x.action, y.action);
        QVERIFY(same(x.target, y.target));
        QCOMPARE(x.args.size(), size_t(2));
        QCOMPARE(x.args.size(), y.args.size());
        for (size_t i = 0; i < x.args.size(); ++i)
            QVERIFY(same(x.args[i], y.args[i]));
    }
    void blankLinesAndComments()
    {
        const Script s = parse(
            u"' tidy downloads\n\n"
            "For Each f In Files(\"~\") ' every file\n"
            "\n"
            "  f.MoveTo _\n"
            "    \"~/x\"\n"
            "  ' nothing here\n"
            "Next\n\n"_s);
        QCOMPARE(s.body.size(), size_t(1));
        QCOMPARE(s.body[0].body.size(), size_t(1));
        QCOMPARE(s.body[0].body[0].args.size(), size_t(1));
        QVERIFY(parse(u""_s).body.empty());
    }
    void notAnAction()
    {
        QVERIFY(error(u"x + 1"_s).message != u"<parsed>"_s);
        QVERIFY(error(u"If x Then f.Trash\nEnd If"_s).message != u"<parsed>"_s); // block form only
    }
    void nestedTooDeeply()
    {
        QString text;
        for (int i = 0; i < 300; ++i)
            text += u"If x Then\n"_s;
        QCOMPARE(error(text).message, u"Script is nested too deeply"_s);
    }
};

QTEST_GUILESS_MAIN(ScriptTest)
#include "script_test.moc"

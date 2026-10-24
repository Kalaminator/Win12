#include "fxparser.h"
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

class FxParserTest : public QObject
{
    Q_OBJECT

    static FxExpr formula(const QString &text)
    {
        FxParseError err;
        const auto e = FxParser::parseFormula(text, &err);
        if (!e)
            qWarning() << "parse error" << err.message << err.line << err.col;
        return e ? *e : nullptr;
    }
    static FxParseError formulaError(const QString &text)
    {
        FxParseError err;
        const auto e = FxParser::parseFormula(text, &err);
        if (e)
            err.message = u"<parsed>"_s;
        return err;
    }
    static FxExpr script(const QString &text, FxParseError *err = nullptr)
    {
        QString lexErr;
        const auto toks = fxLex(text, &lexErr);
        FxParseError local;
        int pos = 0;
        const auto e = FxParser::parseExpression(toks, &pos, FxParser::Mode::Script, err ? err : &local);
        if (!e && !err)
            qWarning() << "parse error" << local.message;
        return e ? *e : nullptr;
    }
    static bool isNum(const FxExpr &e, double n)
    {
        return e->kind == FxNode::Literal && e->literal == FxValue::number(n);
    }

private Q_SLOTS:
    void mockupSelectFormula()
    {
        const auto e = formula(u"=AND(LEFT(NAME,3)=\"CED\", DAYS(MODIFIED)>30, NOT(HASTAG(\"Xiaomi\",\"Red\")))"_s);
        QVERIFY(e);
        QCOMPARE(e->kind, FxNode::Call);
        QCOMPARE(e->name, u"AND"_s);
        QCOMPARE(e->args.size(), size_t(3));
        const auto eq = e->args[0];
        QCOMPARE(eq->kind, FxNode::Binary);
        QCOMPARE(eq->op, u"="_s);
        const auto left = eq->args[0];
        QCOMPARE(left->kind, FxNode::Call);
        QCOMPARE(left->name, u"LEFT"_s);
        QCOMPARE(left->args[0]->kind, FxNode::Column);
        QCOMPARE(left->args[0]->name, u"NAME"_s);
        QVERIFY(isNum(left->args[1], 3));
        QVERIFY(eq->args[1]->literal == FxValue::text(u"CED"_s));
        QCOMPARE(e->args[2]->kind, FxNode::Call);
        QCOMPARE(e->args[2]->name, u"NOT"_s);
    }
    void precedence()
    {
        const auto e = formula(u"1+2*3^2"_s);
        QVERIFY(e);
        QCOMPARE(e->op, u"+"_s);
        QVERIFY(isNum(e->args[0], 1));
        const auto mul = e->args[1];
        QCOMPARE(mul->op, u"*"_s);
        QVERIFY(isNum(mul->args[0], 2));
        QCOMPARE(mul->args[1]->op, u"^"_s);
        QVERIFY(isNum(mul->args[1]->args[0], 3));
        QVERIFY(isNum(mul->args[1]->args[1], 2));
    }
    void concatBelowPlusAboveCompare()
    {
        const auto e = formula(u"\"a\"&1+1=\"a2\""_s);
        QVERIFY(e);
        QCOMPARE(e->op, u"="_s);
        QCOMPARE(e->args[0]->op, u"&"_s);
        QVERIFY(e->args[0]->args[0]->literal == FxValue::text(u"a"_s));
        QCOMPARE(e->args[0]->args[1]->op, u"+"_s);
        QVERIFY(e->args[1]->literal == FxValue::text(u"a2"_s));
    }
    void keywordsAndOrNot()
    {
        const auto e = script(u"Not a = 1 And b Or c"_s);
        QVERIFY(e);
        QCOMPARE(e->kind, FxNode::Binary);
        QCOMPARE(e->op, u"OR"_s);
        const auto andNode = e->args[0];
        QCOMPARE(andNode->op, u"AND"_s);
        const auto eq = andNode->args[0];
        QCOMPARE(eq->op, u"="_s);
        QCOMPARE(eq->args[0]->kind, FxNode::Unary);
        QCOMPARE(eq->args[0]->op, u"NOT"_s);
        QCOMPARE(eq->args[0]->args[0]->kind, FxNode::Variable);
        QCOMPARE(eq->args[0]->args[0]->name, u"a"_s);
    }
    void keywordsInFormulaMode()
    {
        const auto e = formula(u"LEN(NAME)>1 and not ISFOLDER"_s);
        QVERIFY(e);
        QCOMPARE(e->op, u"AND"_s);
        QCOMPARE(e->args[1]->op, u"NOT"_s);
    }
    void leadingEqualsOptional()
    {
        const auto a = formula(u"LEN(NAME)"_s), b = formula(u"=LEN(NAME)"_s);
        QVERIFY(a && b);
        QVERIFY(same(a, b));
    }
    void caseInsensitiveNames()
    {
        const auto e = formula(u"left(name,1)"_s);
        QVERIFY(e);
        QCOMPARE(e->name, u"LEFT"_s);
        QCOMPARE(e->args[0]->kind, FxNode::Column);
        QCOMPARE(e->args[0]->name, u"NAME"_s);
    }
    void trueFalseLiterals()
    {
        const auto e = formula(u"TRUE"_s);
        QVERIFY(e);
        QCOMPARE(e->kind, FxNode::Literal);
        QVERIFY(e->literal == FxValue::boolean(true));
        QVERIFY(formula(u"false"_s)->literal == FxValue::boolean(false));
    }
    void unknownNameInFormulaMode()
    {
        const auto err = formulaError(u"FOO+1"_s);
        QCOMPARE(err.message, u"Unknown name FOO"_s);
        QCOMPARE(err.col, 1);
    }
    void variableInScriptMode()
    {
        const auto e = script(u"foo+1"_s);
        QVERIFY(e);
        QCOMPARE(e->args[0]->kind, FxNode::Variable);
        QCOMPARE(e->args[0]->name, u"foo"_s);
    }
    void arity()
    {
        QCOMPARE(formulaError(u"LEFT()"_s).message, u"LEFT takes 1 to 2 arguments"_s);
        QCOMPARE(formulaError(u"IFS(TRUE)"_s).message, u"IFS takes an even number of arguments"_s);
        FxParseError err;
        QVERIFY(!script(u"IFS(a)"_s, &err));
        QCOMPARE(err.message, u"IFS takes an even number of arguments"_s);
        QCOMPARE(formulaError(u"MID(NAME)"_s).message, u"MID takes 3 arguments"_s);
    }
    void missingParen()
    {
        const auto err = formulaError(u"LEFT(NAME"_s);
        QCOMPARE(err.message, u"Expected )"_s);
        QCOMPARE(err.col, 10);
    }
    void trailingJunk()
    {
        QCOMPARE(formulaError(u"1 2"_s).message, u"Unexpected \"2\""_s);
    }
    void scriptStopsAtNewline()
    {
        QString lexErr;
        const auto toks = fxLex(u"a + 1\nb"_s, &lexErr);
        int pos = 0;
        FxParseError err;
        QVERIFY(FxParser::parseExpression(toks, &pos, FxParser::Mode::Script, &err));
        QCOMPARE(toks[pos].kind, FxToken::Newline);
    }
    void memberAndMethod()
    {
        const auto e = script(u"f.Name & f.Contains(\"x\")"_s);
        QVERIFY(e);
        QCOMPARE(e->op, u"&"_s);
        const auto m = e->args[0];
        QCOMPARE(m->kind, FxNode::Member);
        QCOMPARE(m->name, u"Name"_s);
        QCOMPARE(m->args[0]->kind, FxNode::Variable);
        QCOMPARE(m->args[0]->name, u"f"_s);
        const auto c = e->args[1];
        QCOMPARE(c->kind, FxNode::Method);
        QCOMPARE(c->name, u"Contains"_s);
        QCOMPARE(c->args.size(), size_t(2)); // object + 1 argument
    }
    void columnBeforeDotIsErrorInFormula()
    {
        QVERIFY(formulaError(u"NAME.Length"_s).message != u"<parsed>"_s);
    }
    void filesNamedArg()
    {
        const auto e = script(u"Files(\"~/D\", Subfolders:=True)"_s);
        QVERIFY(e);
        QCOMPARE(e->kind, FxNode::Call);
        QCOMPARE(e->name, u"FILES"_s);
        QCOMPARE(e->args.size(), size_t(2));
        QCOMPARE(e->args[1]->kind, FxNode::Literal);
        QVERIFY(e->args[1]->literal == FxValue::boolean(true));
    }
    void namedArgElsewhereIsError()
    {
        FxParseError err;
        QVERIFY(!script(u"LEFT(a, x:=1)"_s, &err));
        QVERIFY2(err.message.contains(u":="_s), qPrintable(err.message));
    }
    void functionTableHasAllSpecNames()
    {
        const QStringList spec = u"LEFT RIGHT MID LEN FIND SUBSTITUTE UPPER LOWER PROPER TRIM TEXT CONTAINS STARTSWITH ENDSWITH REGEX "
                                 u"IF IFS AND OR NOT TODAY DAYS YEAR MONTH DAY DATE DATEVALUE HASTAG COUNTTAGS ROW ROWINFOLDER COUNTIN FILES"_s.split(u' ');
        QCOMPARE(spec.size(), 33);
        QCOMPARE(FxFunctions::all().size(), 33);
        for (const QString &n : spec)
            QVERIFY2(FxFunctions::find(n), qPrintable(n));
        QVERIFY(FxColumns::isColumn(u"TAKEN"_s));
        QCOMPARE(FxColumns::names().size(), 14);
    }
    void valueText()
    {
        QCOMPARE(FxValue::number(12).toText(), u"12"_s);
        QCOMPARE(FxValue::number(1.5).toText(), u"1.5"_s);
        QCOMPARE(FxValue::number(1.256).toText(), u"1.26"_s);
        QCOMPARE(FxValue::boolean(true).toText(), u"TRUE"_s);
        QCOMPARE(FxValue::error(FxError::Div0).toText(), u"#DIV/0!"_s);
        QCOMPARE(FxValue::date(QDateTime(QDate(2026, 1, 2), QTime(3, 4))).toText(), u"2026-01-02 03:04"_s);
        bool ok;
        QVERIFY(FxValue::number(2).truthy(&ok) && ok);
        FxValue::text(u"x"_s).truthy(&ok);
        QVERIFY(!ok);
    }
};

QTEST_GUILESS_MAIN(FxParserTest)
#include "fxparser_test.moc"

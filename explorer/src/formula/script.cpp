#include "script.h"
#include <KLocalizedString>

using namespace Qt::Literals::StringLiterals;

namespace
{
using Mode = FxParser::Mode;

struct Action {
    QString name;
    int minArgs, maxArgs;
};
// Action names are API words like Excel's function names: never translated.
const Action kActions[] = {
    {u"MoveTo"_s, 1, 1}, {u"CopyTo"_s, 1, 1}, {u"Rename"_s, 1, 1}, {u"Tag"_s, 1, 2}, {u"Untag"_s, 1, 1}, {u"Trash"_s, 0, 0},
};

// What ended a block: the first token of that line (pos is left on it).
enum class Stop { Eof, Next, ElseIf, Else, EndIf };

struct ScriptParser {
    const QList<FxToken> &t;
    FxParseError *err;
    int pos = 0;
    int depth = 0;

    const FxToken &at(int k) const
    {
        return t[qMin(k, int(t.size()) - 1)];
    }
    const FxToken &cur() const
    {
        return at(pos);
    }
    bool isWord(int k, const QString &upper) const
    {
        return at(k).kind == FxToken::Ident && at(k).text.toUpper() == upper;
    }
    bool atLineEnd() const
    {
        return cur().kind == FxToken::Newline || cur().kind == FxToken::End;
    }
    bool fail(const QString &message, const FxToken &where)
    {
        *err = {message, where.line, where.col};
        return false;
    }
    bool expectWord(const QString &upper, const QString &shown)
    {
        if (!isWord(pos, upper))
            return fail(i18n("Expected %1", shown), cur());
        ++pos;
        return true;
    }
    bool endOfLine()
    {
        if (!atLineEnd())
            return fail(i18n("Expected end of line"), cur());
        return true;
    }
    FxExpr expression()
    {
        auto e = FxParser::parseExpression(t, &pos, Mode::Script, err);
        return e ? *e : nullptr;
    }

    // Statements up to a line starting with Next/ElseIf/Else/End If, or the end of the text.
    bool block(std::vector<ScriptStmt> &out, int loops, Stop *stop)
    {
        for (;;) {
            while (cur().kind == FxToken::Newline)
                ++pos;
            if (cur().kind == FxToken::End)
                return *stop = Stop::Eof, true;
            if (isWord(pos, u"NEXT"_s))
                return *stop = Stop::Next, true;
            if (isWord(pos, u"ELSEIF"_s))
                return *stop = Stop::ElseIf, true;
            if (isWord(pos, u"ELSE"_s))
                return *stop = Stop::Else, true;
            if (isWord(pos, u"END"_s) && isWord(pos + 1, u"IF"_s))
                return *stop = Stop::EndIf, true;
            ScriptStmt st;
            st.line = cur().line;
            if (!statement(st, loops) || !endOfLine())
                return false;
            out.push_back(std::move(st));
        }
    }

    bool statement(ScriptStmt &st, int loops)
    {
        const FxToken &first = cur();
        if (isWord(pos, u"FOR"_s))
            return forEach(st, loops);
        if (isWord(pos, u"IF"_s))
            return ifBlock(st, loops);
        if (isWord(pos, u"EXIT"_s)) {
            ++pos;
            if (!expectWord(u"FOR"_s, u"For"_s))
                return false;
            if (!loops)
                return fail(i18n("Exit For outside a loop"), first);
            st.kind = ScriptStmt::ExitFor;
            return true;
        }
        const bool dim = isWord(pos, u"DIM"_s);
        if (dim || (first.kind == FxToken::Ident && at(pos + 1).kind == FxToken::Op && at(pos + 1).text == u"="_s))
            return assign(st, dim);
        return action(st);
    }

    bool assign(ScriptStmt &st, bool dim)
    {
        st.kind = ScriptStmt::Assign;
        if (dim)
            ++pos;
        if (cur().kind != FxToken::Ident)
            return fail(i18n("Expected a variable name"), cur());
        st.var = cur().text;
        ++pos;
        if (dim && isWord(pos, u"AS"_s)) { // "As Integer": types are ignored
            ++pos;
            if (cur().kind != FxToken::Ident)
                return fail(i18n("Expected a type name after As"), cur());
            ++pos;
        }
        if (dim && atLineEnd())
            return true;
        if (cur().kind != FxToken::Op || cur().text != u"="_s)
            return fail(i18n("Expected ="), cur());
        ++pos;
        st.expr = expression();
        return bool(st.expr);
    }

    // <expr>.<Action> [args] — VBA call syntax, or <expr>.<Action>(args).
    bool action(ScriptStmt &st)
    {
        const FxToken &first = cur();
        FxExpr e = expression();
        if (!e)
            return false;
        if (e->kind != FxNode::Member && e->kind != FxNode::Method)
            return fail(i18n("Expected a statement: For Each, If, Dim, a variable = value, or an action like f.MoveTo"), first);
        const Action *spec = nullptr;
        for (const Action &a : kActions)
            if (a.name.compare(e->name, Qt::CaseInsensitive) == 0)
                spec = &a;
        if (!spec)
            return fail(i18n("Unknown action %1", e->name), first);
        st.kind = ScriptStmt::Action;
        st.action = spec->name;
        st.target = e->args[0];
        st.args.assign(e->args.begin() + 1, e->args.end());
        if (e->kind == FxNode::Member) { // no parentheses: arguments follow, comma-separated
            while (!atLineEnd()) {
                FxExpr a = expression();
                if (!a)
                    return false;
                st.args.push_back(a);
                if (cur().kind != FxToken::Comma)
                    break;
                ++pos;
            }
        }
        const int count = int(st.args.size());
        if (count < spec->minArgs || count > spec->maxArgs) {
            if (spec->minArgs == spec->maxArgs)
                return fail(spec->minArgs == 0 ? i18n("%1 takes no arguments", spec->name)
                                               : i18np("%2 takes %1 argument", "%2 takes %1 arguments", spec->minArgs, spec->name),
                            first);
            return fail(i18n("%1 takes %2 to %3 arguments", spec->name, spec->minArgs, spec->maxArgs), first);
        }
        return true;
    }

    bool enter(const FxToken &where)
    {
        if (++depth > 256) // the interpreter recurses the same way
            return fail(i18n("Script is nested too deeply"), where);
        return true;
    }

    bool forEach(ScriptStmt &st, int loops)
    {
        const FxToken &forTok = cur();
        if (!enter(forTok))
            return false;
        ++pos;
        if (!expectWord(u"EACH"_s, u"Each"_s))
            return false;
        if (cur().kind != FxToken::Ident)
            return fail(i18n("Expected a variable name"), cur());
        st.kind = ScriptStmt::ForEach;
        st.var = cur().text;
        ++pos;
        if (!expectWord(u"IN"_s, u"In"_s))
            return false;
        const FxToken &exprTok = cur();
        st.expr = expression();
        if (!st.expr)
            return false;
        if (st.expr->kind != FxNode::Call || st.expr->name != u"FILES"_s)
            return fail(i18n("For Each needs Files(…)"), exprTok);
        Stop stop;
        if (!endOfLine() || !block(st.body, loops + 1, &stop))
            return false;
        if (stop == Stop::Eof)
            return fail(i18n("Expected Next"), forTok);
        if (stop != Stop::Next)
            return fail(stop == Stop::EndIf ? i18n("End If without If") : i18n("%1 without If", cur().text), cur());
        ++pos;
        if (cur().kind == FxToken::Ident) { // Next f
            if (cur().text.compare(st.var, Qt::CaseInsensitive) != 0)
                return fail(i18n("Next %1 does not match For Each %2", cur().text, st.var), cur());
            ++pos;
        }
        --depth;
        return true;
    }

    bool ifBlock(ScriptStmt &st, int loops)
    {
        const FxToken &ifTok = cur();
        if (!enter(ifTok))
            return false;
        st.kind = ScriptStmt::If;
        Stop stop = Stop::ElseIf;
        while (stop == Stop::ElseIf) { // the If itself, then each ElseIf
            ++pos;
            FxExpr cond = expression();
            if (!cond || !expectWord(u"THEN"_s, u"Then"_s))
                return false;
            if (!atLineEnd()) // block form only (Call 10)
                return fail(i18n("Start a new line after Then and close the block with End If"), cur());
            st.branches.push_back({cond, {}});
            if (!block(st.branches.back().second, loops, &stop))
                return false;
        }
        if (stop == Stop::Else) {
            ++pos;
            if (!endOfLine() || !block(st.elseBody, loops, &stop))
                return false;
            if (stop == Stop::Else || stop == Stop::ElseIf)
                return fail(i18n("%1 after Else", cur().text), cur());
        }
        if (stop != Stop::EndIf)
            return fail(i18n("Expected End If"), ifTok);
        pos += 2;
        --depth;
        return true;
    }
};
}

std::optional<Script> parseScript(const QString &text, FxParseError *err)
{
    QString lexError;
    int line = 1, col = 1;
    const QList<FxToken> tokens = fxLex(text, &lexError, &line, &col);
    if (!lexError.isEmpty()) {
        *err = {lexError, line, col};
        return std::nullopt;
    }
    if (tokens.isEmpty()) // ponytail: fxLex always ends with End; this only guards at()
        return Script{};
    ScriptParser p{tokens, err};
    Script s;
    Stop stop;
    if (!p.block(s.body, 0, &stop))
        return std::nullopt;
    if (stop != Stop::Eof) {
        const FxToken &tok = p.cur();
        if (stop == Stop::Next)
            p.fail(i18n("Next without For"), tok);
        else
            p.fail(stop == Stop::EndIf ? i18n("End If without If") : i18n("%1 without If", tok.text), tok);
        return std::nullopt;
    }
    return s;
}

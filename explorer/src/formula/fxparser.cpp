#include "fxparser.h"
#include <KLocalizedString>

using namespace Qt::Literals::StringLiterals;

namespace FxFunctions
{
const QList<FxFunctionSpec> &all()
{
    // Excel's English names and argument lists (never translated); the help line is UI text.
    static const QList<FxFunctionSpec> list = {
        {u"LEFT"_s, 1, 2, u"LEFT(text, [count])"_s, i18n("The first characters of a text (1 if no count).")},
        {u"RIGHT"_s, 1, 2, u"RIGHT(text, [count])"_s, i18n("The last characters of a text (1 if no count).")},
        {u"MID"_s, 3, 3, u"MID(text, start, count)"_s, i18n("Characters from the middle of a text; start counts from 1.")},
        {u"LEN"_s, 1, 1, u"LEN(text)"_s, i18n("The number of characters in a text.")},
        {u"FIND"_s, 2, 3, u"FIND(find_text, within_text, [start])"_s, i18n("Where a text starts inside another, from 1; case-sensitive.")},
        {u"SUBSTITUTE"_s, 3, 3, u"SUBSTITUTE(text, old_text, new_text)"_s, i18n("Replaces every occurrence of a text with another.")},
        {u"UPPER"_s, 1, 1, u"UPPER(text)"_s, i18n("A text in upper case.")},
        {u"LOWER"_s, 1, 1, u"LOWER(text)"_s, i18n("A text in lower case.")},
        {u"PROPER"_s, 1, 1, u"PROPER(text)"_s, i18n("Each word with its first letter in upper case.")},
        {u"TRIM"_s, 1, 1, u"TRIM(text)"_s, i18n("A text without spaces at the start and end.")},
        {u"TEXT"_s, 2, 2, u"TEXT(value, format)"_s, i18n("A number or date as text: TEXT(ROW(), \"00\"), TEXT(MODIFIED, \"yyyy-MM-dd\").")},
        {u"CONTAINS"_s, 2, 2, u"CONTAINS(text, part)"_s, i18n("TRUE when the text contains the part (any case).")},
        {u"STARTSWITH"_s, 2, 2, u"STARTSWITH(text, part)"_s, i18n("TRUE when the text starts with the part (any case).")},
        {u"ENDSWITH"_s, 2, 2, u"ENDSWITH(text, part)"_s, i18n("TRUE when the text ends with the part (any case).")},
        {u"REGEX"_s, 2, 2, u"REGEX(text, pattern)"_s, i18n("TRUE when the text matches the regular expression.")},
        {u"IF"_s, 2, 3, u"IF(test, value_if_true, [value_if_false])"_s, i18n("One value when the test is TRUE, another when it is FALSE.")},
        {u"IFS"_s, 2, -1, u"IFS(test1, value1, [test2, value2], ...)"_s, i18n("The value of the first test that is TRUE.")},
        {u"AND"_s, 1, -1, u"AND(test1, [test2], ...)"_s, i18n("TRUE when every test is TRUE.")},
        {u"OR"_s, 1, -1, u"OR(test1, [test2], ...)"_s, i18n("TRUE when any test is TRUE.")},
        {u"NOT"_s, 1, 1, u"NOT(test)"_s, i18n("TRUE when the test is FALSE.")},
        {u"TODAY"_s, 0, 0, u"TODAY()"_s, i18n("Today's date.")},
        {u"DAYS"_s, 1, 2, u"DAYS(end_date, [start_date])"_s, i18n("Days from a date until today, or between two dates.")},
        {u"YEAR"_s, 1, 1, u"YEAR(date)"_s, i18n("The year of a date.")},
        {u"MONTH"_s, 1, 1, u"MONTH(date)"_s, i18n("The month of a date, 1 to 12.")},
        {u"DAY"_s, 1, 1, u"DAY(date)"_s, i18n("The day of the month of a date.")},
        {u"DATE"_s, 3, 3, u"DATE(year, month, day)"_s, i18n("A date from its year, month and day.")},
        {u"DATEVALUE"_s, 1, 1, u"DATEVALUE(date_text)"_s, i18n("A date from a text such as \"2026-01-31\".")},
        {u"HASTAG"_s, 1, 2, u"HASTAG(tag, [colour])"_s, i18n("TRUE when the file has the tag (of that colour, if given).")},
        {u"COUNTTAGS"_s, 0, 0, u"COUNTTAGS()"_s, i18n("How many tags the file has.")},
        {u"ROW"_s, 0, 0, u"ROW()"_s, i18n("The file's row number in the results, from 1.")},
        {u"ROWINFOLDER"_s, 0, 0, u"ROWINFOLDER()"_s, i18n("The file's row number inside its folder, from 1.")},
        {u"COUNTIN"_s, 1, 1, u"COUNTIN(folder)"_s, i18n("How many items a folder holds.")},
        {u"FILES"_s, 1, 2, u"Files(folder, [Subfolders:=True])"_s, i18n("Scripts: the files of a folder, for For Each.")},
    };
    return list;
}

const FxFunctionSpec *find(const QString &upperName)
{
    for (const FxFunctionSpec &f : all())
        if (f.name == upperName)
            return &f;
    return nullptr;
}
}

namespace FxColumns
{
const QStringList &names()
{
    static const QStringList list = u"NAME EXT FULLNAME FOLDER PATH SIZE MODIFIED CREATED ISFOLDER DEPTH TAGS WIDTH HEIGHT TAKEN"_s.split(u' ');
    return list;
}

bool isColumn(const QString &upperName)
{
    return names().contains(upperName);
}
}

namespace
{
using Mode = FxParser::Mode;

// Binary levels, low -> high; unary '-'/NOT and postfix '.' bind tighter than all of them.
const QStringList kLevels[] = {{u"OR"_s}, {u"AND"_s}, {u"="_s, u"<>"_s, u"<"_s, u">"_s, u"<="_s, u">="_s}, {u"&"_s}, {u"+"_s, u"-"_s}, {u"*"_s, u"/"_s}, {u"^"_s}};
constexpr int kLevelCount = sizeof(kLevels) / sizeof(kLevels[0]);

bool isWord(const FxToken &t, const QString &upper)
{
    return t.kind == FxToken::Ident && t.text.toUpper() == upper;
}

struct Parser {
    const QList<FxToken> &t;
    int &pos;
    Mode mode;
    FxParseError *err;

    const FxToken &at(int k) const
    {
        return t[qMin(k, int(t.size()) - 1)];
    }
    const FxToken &cur() const
    {
        return at(pos);
    }

    FxExpr fail(const QString &message, const FxToken &where)
    {
        *err = {message, where.line, where.col};
        return nullptr;
    }

    FxExpr unexpected(const FxToken &tok)
    {
        if (tok.kind == FxToken::End && mode == Mode::Formula)
            return fail(i18n("Unexpected end of formula"), tok);
        if (tok.kind == FxToken::End || tok.kind == FxToken::Newline)
            return fail(i18n("Unexpected end of line"), tok);
        return fail(i18n("Unexpected \"%1\"", tok.kind == FxToken::String ? u'"' + tok.text + u'"' : tok.text), tok);
    }

    static FxExpr node(FxNode::Kind kind, const FxToken &where)
    {
        auto n = std::make_shared<FxNode>();
        n->kind = kind;
        n->line = where.line;
        n->col = where.col;
        return n;
    }

    QString binaryOp(int level) const
    {
        const FxToken &c = cur();
        for (const QString &op : kLevels[level]) {
            if (level <= 1 ? isWord(c, op) : (c.kind == FxToken::Op && c.text == op))
                return op;
        }
        return {};
    }

    FxExpr binary(int level)
    {
        if (level == kLevelCount)
            return unary();
        FxExpr left = binary(level + 1);
        while (left) {
            const QString op = binaryOp(level);
            if (op.isEmpty())
                break;
            const FxToken &opTok = cur();
            ++pos;
            FxExpr right = binary(level + 1);
            if (!right)
                return nullptr;
            FxExpr n = node(FxNode::Binary, opTok);
            n->op = op;
            n->args = {left, right};
            left = n;
        }
        return left;
    }

    FxExpr unary()
    {
        const FxToken &tok = cur();
        const bool minus = tok.kind == FxToken::Op && tok.text == u"-"_s;
        const bool notWord = isWord(tok, u"NOT"_s) && at(pos + 1).kind != FxToken::LParen; // NOT(x) stays a call
        if (!minus && !notWord)
            return postfix();
        ++pos;
        FxExpr operand = unary();
        if (!operand)
            return nullptr;
        FxExpr n = node(FxNode::Unary, tok);
        n->op = minus ? u"-"_s : u"NOT"_s;
        n->args = {operand};
        return n;
    }

    FxExpr postfix()
    {
        FxExpr e = primary();
        while (e && cur().kind == FxToken::Dot) {
            if (mode == Mode::Formula && e->kind == FxNode::Column)
                return unexpected(cur());
            ++pos;
            const FxToken &nameTok = cur();
            if (nameTok.kind != FxToken::Ident)
                return fail(i18n("Expected a name after \".\""), nameTok);
            ++pos;
            const bool method = cur().kind == FxToken::LParen;
            FxExpr n = node(method ? FxNode::Method : FxNode::Member, nameTok);
            n->name = nameTok.text;
            n->args = {e};
            if (method) {
                ++pos;
                if (!arguments(n, false))
                    return nullptr;
            }
            e = n;
        }
        return e;
    }

    // After '(' : reads arguments up to and including ')' into n->args.
    bool arguments(const FxExpr &n, bool allowSubfolders)
    {
        if (cur().kind == FxToken::RParen) {
            ++pos;
            return true;
        }
        for (;;) {
            if (cur().kind == FxToken::Ident && at(pos + 1).kind == FxToken::Op && at(pos + 1).text == u":="_s) {
                const FxToken &nameTok = cur();
                if (!allowSubfolders || nameTok.text.toUpper() != u"SUBFOLDERS"_s)
                    return fail(i18n("Named arguments (name:=value) only work as Subfolders:= in Files"), nameTok), false;
                pos += 2;
                const FxToken &valueTok = cur();
                FxExpr v = binary(0);
                if (!v)
                    return false;
                if (v->kind != FxNode::Literal || v->literal.type != FxValue::Type::Bool)
                    return fail(i18n("Subfolders:= takes True or False"), valueTok), false;
                n->args.push_back(v);
                if (cur().kind != FxToken::RParen) // the named argument comes last
                    return fail(i18n("Expected )"), cur()), false;
            } else {
                FxExpr a = binary(0);
                if (!a)
                    return false;
                n->args.push_back(a);
            }
            if (cur().kind == FxToken::Comma) {
                ++pos;
            } else if (cur().kind == FxToken::RParen) {
                ++pos;
                return true;
            } else {
                return fail(i18n("Expected )"), cur()), false;
            }
        }
    }

    FxExpr call(const FxToken &nameTok)
    {
        const QString upper = nameTok.text.toUpper();
        const FxFunctionSpec *spec = FxFunctions::find(upper);
        if (!spec)
            return fail(i18n("Unknown function %1", nameTok.text), nameTok);
        pos += 2; // name and '('
        FxExpr n = node(FxNode::Call, nameTok);
        n->name = upper;
        if (!arguments(n, upper == u"FILES"_s))
            return nullptr;
        const int count = int(n->args.size());
        if (upper == u"IFS"_s) {
            if (count < 2 || count % 2)
                return fail(i18n("IFS takes an even number of arguments"), nameTok);
        } else if (spec->maxArgs < 0) {
            if (count < spec->minArgs)
                return fail(i18np("%2 takes at least %1 argument", "%2 takes at least %1 arguments", spec->minArgs, upper), nameTok);
        } else if (count < spec->minArgs || count > spec->maxArgs) {
            if (spec->minArgs == spec->maxArgs)
                return fail(spec->minArgs == 0 ? i18n("%1 takes no arguments", upper)
                                               : i18np("%2 takes %1 argument", "%2 takes %1 arguments", spec->minArgs, upper),
                            nameTok);
            return fail(i18n("%1 takes %2 to %3 arguments", upper, spec->minArgs, spec->maxArgs), nameTok);
        }
        return n;
    }

    FxExpr primary()
    {
        const FxToken &tok = cur();
        switch (tok.kind) {
        case FxToken::Number:
        case FxToken::String: {
            ++pos;
            FxExpr n = node(FxNode::Literal, tok);
            n->literal = tok.kind == FxToken::Number ? FxValue::number(tok.number) : FxValue::text(tok.text);
            return n;
        }
        case FxToken::LParen: {
            ++pos;
            FxExpr e = binary(0);
            if (!e)
                return nullptr;
            if (cur().kind != FxToken::RParen)
                return fail(i18n("Expected )"), cur());
            ++pos;
            return e;
        }
        case FxToken::Ident:
            break;
        default:
            return unexpected(tok);
        }
        if (at(pos + 1).kind == FxToken::LParen)
            return call(tok);
        const QString upper = tok.text.toUpper();
        if (upper == u"TRUE"_s || upper == u"FALSE"_s) {
            ++pos;
            FxExpr n = node(FxNode::Literal, tok);
            n->literal = FxValue::boolean(upper == u"TRUE"_s);
            return n;
        }
        if (upper == u"AND"_s || upper == u"OR"_s)
            return unexpected(tok);
        FxExpr n;
        if (mode == Mode::Script) {
            n = node(FxNode::Variable, tok);
            n->name = tok.text;
        } else if (FxColumns::isColumn(upper)) {
            n = node(FxNode::Column, tok);
            n->name = upper;
        } else {
            return fail(i18n("Unknown name %1", tok.text), tok);
        }
        ++pos;
        return n;
    }
};
}

std::optional<FxExpr> FxParser::parseExpression(const QList<FxToken> &tokens, int *pos, Mode mode, FxParseError *err)
{
    if (tokens.isEmpty()) {
        *err = {i18n("Unexpected end of line")};
        return std::nullopt;
    }
    Parser p{tokens, *pos, mode, err};
    FxExpr e = p.binary(0);
    if (!e)
        return std::nullopt;
    return e;
}

std::optional<FxExpr> FxParser::parseFormula(const QString &text, FxParseError *err)
{
    QString lexError;
    int line = 1, col = 1;
    QList<FxToken> tokens = fxLex(text, &lexError, &line, &col);
    if (!lexError.isEmpty()) {
        *err = {lexError, line, col};
        return std::nullopt;
    }
    tokens.removeIf([](const FxToken &t) { return t.kind == FxToken::Newline; });
    int pos = tokens.first().kind == FxToken::Op && tokens.first().text == u"="_s ? 1 : 0;
    auto e = parseExpression(tokens, &pos, Mode::Formula, err);
    if (e && tokens[pos].kind != FxToken::End) {
        Parser{tokens, pos, Mode::Formula, err}.unexpected(tokens[pos]);
        return std::nullopt;
    }
    return e;
}

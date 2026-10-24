#include "fxeval.h"
#include <QDir>
#include <QRegularExpression>
#include <algorithm>
#include <climits>
#include <cmath>

using namespace Qt::Literals::StringLiterals;

int fxColourIndex(const QString &word)
{
    const int exact = TagColours::indexOfName(word);
    if (exact >= 0 || word.isEmpty())
        return exact;
    for (int i = 0; i < TagColours::Count; ++i)
        if (TagColours::name(i).startsWith(word, Qt::CaseInsensitive))
            return i;
    return -1;
}
namespace
{
static int toInt(double x) { return std::isnan(x) ? 0 : int(std::clamp(x, double(INT_MIN), double(INT_MAX))); }
using V = FxValue;
using Type = FxValue::Type;

V fail(FxError e = FxError::Value)
{
    return V::error(e);
}

V dateOrEmpty(const QDateTime &d)
{
    return d.isValid() ? V::date(d) : V::empty();
}

// Function arguments: Number, Bool, Empty (0) and numeric text are numbers.
bool toNumber(const V &v, double *out)
{
    bool ok = true;
    switch (v.type) {
    case Type::Number:
        *out = v.n;
        return true;
    case Type::Bool:
        *out = v.b ? 1 : 0;
        return true;
    case Type::Empty:
        *out = 0;
        return true;
    case Type::Text:
        *out = v.s.trimmed().toDouble(&ok);
        return ok;
    default:
        return false;
    }
}

bool toDate(const V &v, QDate *out)
{
    if (v.type != Type::Date || !v.d.isValid())
        return false;
    *out = v.d.date();
    return true;
}

// TEXT(number, "00" / "0.0"): zeros before '.' pad the integer part, zeros after it are decimals.
V textOfNumber(double x, const QString &fmt)
{
    static const QRegularExpression re(u"^(0*)(?:\\.(0+))?$"_s);
    const auto m = re.match(fmt);
    if (!m.hasMatch())
        return fail();
    const int width = m.capturedLength(1), decimals = m.capturedLength(2);
    const double scale = std::pow(10.0, decimals);
    // Excel rounds half away from zero on 15 significant digits (R3): 1.25 -> 1.3, 1.005 -> 1.01.
    const double r = std::round(QString::number(std::abs(x) * scale, 'g', 15).toDouble()) / scale;
    QString t = QString::number(r, 'f', decimals);
    const int intLen = decimals ? t.indexOf(u'.') : t.size();
    if (intLen < width)
        t.prepend(QString(width - intLen, u'0'));
    if (x < 0 && r != 0)
        t.prepend(u'-');
    return V::text(t);
}

int defaultCountIn(const QString &abs)
{
    const QDir d(abs);
    return d.exists() ? int(d.entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot).size()) : -1;
}

V compare(const QString &op, V a, V b)
{
    // Excel: an empty value compares as "" against text, 0 against numbers, FALSE against booleans.
    auto blankAs = [](V &v, Type other) {
        if (v.type != Type::Empty)
            return;
        if (other == Type::Text)
            v = V::text(QString());
        else if (other == Type::Number)
            v = V::number(0);
        else if (other == Type::Bool)
            v = V::boolean(false);
    };
    blankAs(a, b.type);
    blankAs(b, a.type);
    if (a.type != b.type)
        return fail();
    int c = 0;
    switch (a.type) {
    case Type::Empty:
        break;
    case Type::Bool:
        c = int(a.b) - int(b.b);
        break;
    case Type::Number:
        c = (a.n > b.n) - (a.n < b.n);
        break;
    case Type::Text:
        c = QString::compare(a.s, b.s, Qt::CaseInsensitive);
        break;
    case Type::Date:
        c = (a.d > b.d) - (a.d < b.d);
        break;
    default:
        return fail();
    }
    if (op == u"="_s)
        return V::boolean(c == 0);
    if (op == u"<>"_s)
        return V::boolean(c != 0);
    if (op == u"<"_s)
        return V::boolean(c < 0);
    if (op == u">"_s)
        return V::boolean(c > 0);
    if (op == u"<="_s)
        return V::boolean(c <= 0);
    return V::boolean(c >= 0); // ">="
}

V arithmetic(const QString &op, const V &a, const V &b)
{
    if (a.type != Type::Number || b.type != Type::Number)
        return fail();
    double r;
    if (op == u"+"_s)
        r = a.n + b.n;
    else if (op == u"-"_s)
        r = a.n - b.n;
    else if (op == u"*"_s)
        r = a.n * b.n;
    else if (op == u"/"_s) {
        if (b.n == 0)
            return fail(FxError::Div0);
        r = a.n / b.n;
    } else
        r = std::pow(a.n, b.n); // "^"
    return std::isfinite(r) ? V::number(r) : fail(FxError::Num);
}

V column(const QString &c, FxFile &f)
{
    if (c == u"NAME"_s)
        return V::text(f.name());
    if (c == u"EXT"_s)
        return V::text(f.ext());
    if (c == u"FULLNAME"_s)
        return V::text(f.fullName());
    if (c == u"FOLDER"_s)
        return V::text(f.folder());
    if (c == u"PATH"_s)
        return V::text(f.path);
    if (c == u"SIZE"_s)
        return V::number(f.isFolder ? 0 : double(f.size));
    if (c == u"MODIFIED"_s)
        return dateOrEmpty(f.modified);
    if (c == u"CREATED"_s)
        return dateOrEmpty(f.created);
    if (c == u"ISFOLDER"_s)
        return V::boolean(f.isFolder);
    if (c == u"DEPTH"_s)
        return V::number(f.depth);
    if (c == u"TAGS"_s)
        return V::text(encodeAll(f.tags()).join(u", "_s));
    if (c == u"WIDTH"_s)
        return V::number(f.photo().width);
    if (c == u"HEIGHT"_s)
        return V::number(f.photo().height);
    if (c == u"TAKEN"_s)
        return dateOrEmpty(f.photo().taken);
    return fail(FxError::Name);
}

struct Evaluator {
    const std::shared_ptr<FxFile> &row;
    FxContext &ctx;

    V eval(const FxExpr &e)
    {
        const FxNode &n = *e;
        switch (n.kind) {
        case FxNode::Literal:
            return n.literal;
        case FxNode::Column:
            return row ? column(n.name, *row) : fail();
        case FxNode::Variable: {
            if (!ctx.variables)
                return fail(FxError::Name);
            const auto it = ctx.variables->constFind(n.name.toLower());
            return it == ctx.variables->constEnd() ? fail(FxError::Name) : *it;
        }
        case FxNode::Call:
            return call(n);
        case FxNode::Member:
        case FxNode::Method:
            return member(n);
        case FxNode::Unary: {
            const V v = eval(n.args[0]);
            if (v.isError())
                return v;
            if (n.op == u"-"_s)
                return v.type == Type::Number ? V::number(-v.n) : fail();
            bool ok;
            const bool t = v.truthy(&ok);
            return ok ? V::boolean(!t) : fail();
        }
        case FxNode::Binary:
            return binary(n);
        }
        return fail();
    }

    V binary(const FxNode &n)
    {
        if (n.op == u"AND"_s || n.op == u"OR"_s)
            return logic(n.op == u"AND"_s, n.args);
        const V a = eval(n.args[0]);
        if (a.isError())
            return a;
        const V b = eval(n.args[1]);
        if (b.isError())
            return b;
        if (n.op == u"&"_s)
            return V::text(a.toText() + b.toText());
        static const QStringList comparisons{u"="_s, u"<>"_s, u"<"_s, u">"_s, u"<="_s, u">="_s};
        if (comparisons.contains(n.op))
            return compare(n.op, a, b);
        return arithmetic(n.op, a, b);
    }

    // AND/OR: left to right, stops at the first operand that decides; non-Bool/Number -> #VALUE!.
    V logic(bool isAnd, const std::vector<FxExpr> &args)
    {
        for (const FxExpr &arg : args) {
            const V v = eval(arg);
            if (v.isError())
                return v;
            bool ok;
            const bool t = v.truthy(&ok);
            if (!ok)
                return fail();
            if (t != isAnd)
                return V::boolean(t);
        }
        return V::boolean(isAnd);
    }

    V member(const FxNode &n)
    {
        const V obj = eval(n.args[0]);
        if (obj.isError())
            return obj;
        if (obj.type != Type::File || !obj.file)
            return fail();
        const QString upper = n.name.toUpper();
        if (n.kind == FxNode::Method) {
            if (upper != u"CONTAINS"_s || n.args.size() != 2)
                return fail(FxError::Name);
            const V part = eval(n.args[1]);
            if (part.isError())
                return part;
            return V::boolean(obj.file->fullName().contains(part.toText(), Qt::CaseInsensitive));
        }
        static const QStringList members = u"NAME EXT FULLNAME FOLDER PATH SIZE MODIFIED CREATED ISFOLDER DEPTH TAGS"_s.split(u' ');
        return members.contains(upper) ? column(upper, *obj.file) : fail(FxError::Name);
    }

    V call(const FxNode &n)
    {
        const QString &f = n.name;
        // Lazy ones first: only the branch taken is evaluated.
        auto test = [this, &n](size_t i, V *out) {
            *out = eval(n.args[i]);
            bool ok;
            const bool t = out->truthy(&ok);
            if (!out->isError() && !ok)
                *out = fail();
            return ok && t;
        };
        if (f == u"IF"_s) {
            V t;
            if (test(0, &t))
                return eval(n.args[1]);
            if (t.isError())
                return t;
            return n.args.size() > 2 ? eval(n.args[2]) : V::boolean(false);
        }
        if (f == u"IFS"_s) {
            for (size_t i = 0; i + 1 < n.args.size(); i += 2) {
                V t;
                if (test(i, &t))
                    return eval(n.args[i + 1]);
                if (t.isError())
                    return t;
            }
            return fail(FxError::NA);
        }
        if (f == u"AND"_s || f == u"OR"_s)
            return logic(f == u"AND"_s, n.args);

        QList<V> a;
        for (const FxExpr &arg : n.args) {
            V v = eval(arg);
            if (v.isError())
                return v;
            a << v;
        }
        auto s = [&a](int i) {
            return a[i].toText();
        };
        double x = 0, y = 0, z = 0;
        auto num = [&a](int i, double *out) {
            return toNumber(a[i], out);
        };

        if (f == u"LEFT"_s || f == u"RIGHT"_s) {
            x = 1;
            if (a.size() > 1 && !num(1, &x))
                return fail();
            if (x < 0)
                return fail();
            return V::text(f == u"LEFT"_s ? s(0).left(toInt(x)) : s(0).right(toInt(x)));
        }
        if (f == u"MID"_s) {
            if (!num(1, &x) || !num(2, &y) || x < 1 || y < 0)
                return fail();
            return V::text(s(0).mid(toInt(x - 1), toInt(y)));
        }
        if (f == u"LEN"_s)
            return V::number(s(0).size());
        if (f == u"FIND"_s) {
            x = 1;
            if ((a.size() > 2 && !num(2, &x)) || x < 1)
                return fail();
            const int at = s(1).indexOf(s(0), toInt(x - 1), Qt::CaseSensitive);
            return at < 0 ? fail() : V::number(at + 1);
        }
        if (f == u"SUBSTITUTE"_s)
            return V::text(s(1).isEmpty() ? s(0) : s(0).replace(s(1), s(2)));
        if (f == u"UPPER"_s)
            return V::text(s(0).toUpper());
        if (f == u"LOWER"_s)
            return V::text(s(0).toLower());
        if (f == u"PROPER"_s) {
            QString t = s(0);
            bool afterLetter = false;
            for (QChar &c : t) {
                c = afterLetter ? c.toLower() : c.toUpper();
                afterLetter = c.isLetter();
            }
            return V::text(t);
        }
        if (f == u"TRIM"_s) // Excel's TRIM: spaces only, inner runs collapse to one
            return V::text(s(0).split(u' ', Qt::SkipEmptyParts).join(u' '));
        if (f == u"TEXT"_s) {
            if (a[0].type == Type::Date)
                return V::text(a[0].d.toString(s(1)));
            if (a[0].type == Type::Text)
                return a[0];
            return num(0, &x) ? textOfNumber(x, s(1)) : fail();
        }
        if (f == u"CONTAINS"_s)
            return V::boolean(s(0).contains(s(1), Qt::CaseInsensitive));
        if (f == u"STARTSWITH"_s)
            return V::boolean(s(0).startsWith(s(1), Qt::CaseInsensitive));
        if (f == u"ENDSWITH"_s)
            return V::boolean(s(0).endsWith(s(1), Qt::CaseInsensitive));
        if (f == u"REGEX"_s) {
            // ponytail: compiled per row; cache by pattern if large result sets get slow.
            const QRegularExpression re(s(1));
            return re.isValid() ? V::boolean(re.match(s(0)).hasMatch()) : fail();
        }
        if (f == u"NOT"_s) {
            bool ok;
            const bool t = a[0].truthy(&ok);
            return ok ? V::boolean(!t) : fail();
        }
        if (f == u"TODAY"_s)
            return V::date(QDateTime(ctx.today, QTime(0, 0)));
        if (f == u"DAYS"_s) {
            QDate end = ctx.today, start;
            if (a.size() == 1 ? !toDate(a[0], &start) : (!toDate(a[0], &end) || !toDate(a[1], &start)))
                return fail();
            return V::number(start.daysTo(end));
        }
        if (f == u"YEAR"_s || f == u"MONTH"_s || f == u"DAY"_s) {
            QDate d;
            if (!toDate(a[0], &d))
                return fail();
            return V::number(f == u"YEAR"_s ? d.year() : f == u"MONTH"_s ? d.month() : d.day());
        }
        if (f == u"DATE"_s) {
            if (!num(0, &x) || !num(1, &y) || !num(2, &z))
                return fail();
            const QDate d{toInt(x), toInt(y), toInt(z)};
            return d.isValid() && d.year() >= 1 && d.year() <= 9999 ? V::date(QDateTime(d, QTime(0, 0))) : fail(FxError::Num); // Excel's year range
        }
        if (f == u"DATEVALUE"_s) {
            const QDate d = QDate::fromString(s(0).trimmed(), Qt::ISODate);
            return d.isValid() ? V::date(QDateTime(d, QTime(0, 0))) : fail();
        }
        if (f == u"COUNTIN"_s) {
            QString p = s(0);
            if (p == u"~"_s || p.startsWith(u"~/"_s))
                p = QDir::homePath() + p.mid(1);
            if (ctx.baseFolder.isEmpty() && !QDir::isAbsolutePath(p))
                return fail(FxError::NA); // no base folder: never resolve against the process cwd
            const QString abs = QDir::cleanPath(QDir(ctx.baseFolder).absoluteFilePath(p));
            auto it = ctx.countCache.find(abs);
            if (it == ctx.countCache.end())
                it = ctx.countCache.insert(abs, ctx.countIn ? ctx.countIn(abs) : defaultCountIn(abs));
            return *it < 0 ? fail(FxError::NA) : V::number(*it);
        }

        // The rest read the row.
        if (!row)
            return fail();
        if (f == u"HASTAG"_s) {
            const QString word = s(0);
            const bool withColour = a.size() > 1;
            const int colour = withColour ? fxColourIndex(s(1)) : -1;
            if (withColour && colour < 0)
                return V::boolean(false);
            for (const Tag &t : row->tags()) {
                const bool wordMatch = t.word.compare(word, Qt::CaseInsensitive) == 0 || t.encode().compare(word, Qt::CaseInsensitive) == 0;
                if (wordMatch && (!withColour || t.colour == colour))
                    return V::boolean(true);
            }
            return V::boolean(false);
        }
        if (f == u"COUNTTAGS"_s)
            return V::number(row->tags().size());
        if (f == u"ROW"_s)
            return V::number(row->row);
        if (f == u"ROWINFOLDER"_s)
            return V::number(row->rowInFolder);
        return fail(FxError::Name); // FILES and anything scripts handle themselves
    }
};
}

FxValue fxEvaluate(const FxExpr &e, const std::shared_ptr<FxFile> &row, FxContext &ctx)
{
    if (!e)
        return fail();
    return Evaluator{row, ctx}.eval(e);
}

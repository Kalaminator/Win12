#include "fxvalue.h"
#include "fxfile.h"

using namespace Qt::Literals::StringLiterals;

FxValue FxValue::empty()
{
    return {};
}

FxValue FxValue::boolean(bool v)
{
    FxValue r;
    r.type = Type::Bool;
    r.b = v;
    return r;
}

FxValue FxValue::number(double v)
{
    FxValue r;
    r.type = Type::Number;
    r.n = v;
    return r;
}

FxValue FxValue::text(const QString &v)
{
    FxValue r;
    r.type = Type::Text;
    r.s = v;
    return r;
}

FxValue FxValue::date(const QDateTime &v)
{
    FxValue r;
    r.type = Type::Date;
    r.d = v;
    return r;
}

FxValue FxValue::fileRef(std::shared_ptr<FxFile> v)
{
    FxValue r;
    r.type = Type::File;
    r.file = std::move(v);
    return r;
}

FxValue FxValue::error(FxError e)
{
    FxValue r;
    r.type = Type::Error;
    r.err = e;
    return r;
}

bool FxValue::isError() const
{
    return type == Type::Error;
}

bool FxValue::truthy(bool *ok) const
{
    *ok = type == Type::Bool || type == Type::Number;
    return type == Type::Bool ? b : type == Type::Number && n != 0;
}

QString FxValue::toText() const
{
    switch (type) {
    case Type::Empty:
        return {};
    case Type::Bool:
        return b ? u"TRUE"_s : u"FALSE"_s;
    case Type::Number: {
        QString t = QString::number(n, 'f', 2);
        while (t.endsWith(u'0'))
            t.chop(1);
        if (t.endsWith(u'.'))
            t.chop(1);
        return t == u"-0"_s ? u"0"_s : t;
    }
    case Type::Text:
        return s;
    case Type::Date:
        return d.toString(u"yyyy-MM-dd hh:mm"_s);
    case Type::File:
        return file ? file->fullName() : QString();
    case Type::Error:
        break;
    }
    static const QString names[] = {u"#VALUE!"_s, u"#NAME?"_s, u"#DIV/0!"_s, u"#N/A"_s, u"#NUM!"_s};
    return names[int(err)];
}

QString FxValue::display() const
{
    return type == Type::File && file ? file->path : toText();
}

bool FxValue::operator==(const FxValue &o) const
{
    if (type != o.type)
        return false;
    switch (type) {
    case Type::Empty:
        return true;
    case Type::Bool:
        return b == o.b;
    case Type::Number:
        return n == o.n;
    case Type::Text:
        return s == o.s;
    case Type::Date:
        return d == o.d;
    case Type::File:
        return file == o.file;
    case Type::Error:
        return err == o.err;
    }
    return false;
}

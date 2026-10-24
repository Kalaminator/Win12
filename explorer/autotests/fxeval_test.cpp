#include "fxeval.h"
#include "fxfile.h"
#include "fxparser.h"
#include <QDir>
#include <QTest>

using namespace Qt::Literals::StringLiterals;

class FxEvalTest : public QObject
{
    Q_OBJECT

    FxContext ctx;
    std::shared_ptr<FxFile> holiday;

    static Tag tag(const char *colour, const QString &word)
    {
        return Tag{TagColours::indexOfName(QString::fromLatin1(colour)), word};
    }
    static std::shared_ptr<FxFile> fileRow(const QString &path, const QDateTime &modified, const QList<Tag> &tags)
    {
        auto f = std::make_shared<FxFile>();
        f->path = path;
        f->size = 1258291;
        f->modified = modified;
        f->depth = 1;
        f->row = 3;
        f->rowInFolder = 2;
        f->loadTags = [tags] { return tags; };
        f->loadPhoto = [] { return FxPhoto{4000, 3000, QDateTime(QDate(2026, 7, 30), QTime(10, 0))}; };
        return f;
    }
    FxValue ev(const QString &text, const std::shared_ptr<FxFile> &row = nullptr)
    {
        FxParseError err;
        const auto e = FxParser::parseFormula(text, &err);
        if (!e) {
            qWarning() << "parse error" << text << err.message;
            return FxValue::error(FxError::Name);
        }
        return fxEvaluate(*e, row ? row : holiday, ctx);
    }
    static FxValue T(const QString &s)
    {
        return FxValue::text(s);
    }
    static FxValue N(double n)
    {
        return FxValue::number(n);
    }
    static FxValue B(bool b)
    {
        return FxValue::boolean(b);
    }
    static FxValue E(FxError e)
    {
        return FxValue::error(e);
    }

private Q_SLOTS:
    void init()
    {
        ctx = FxContext();
        ctx.today = QDate(2026, 10, 24);
        holiday = fileRow(u"/base/Trip/holiday.jpg"_s, QDateTime(QDate(2026, 8, 2), QTime(14, 5)), {tag("Green", u"Work"_s)});
    }

    void columns()
    {
        QCOMPARE(ev(u"NAME"_s), T(u"holiday"_s));
        QCOMPARE(ev(u"EXT"_s), T(u"jpg"_s));
        QCOMPARE(ev(u"FULLNAME"_s), T(u"holiday.jpg"_s));
        QCOMPARE(ev(u"FOLDER"_s), T(u"Trip"_s));
        QCOMPARE(ev(u"PATH"_s), T(u"/base/Trip/holiday.jpg"_s));
        QCOMPARE(ev(u"SIZE"_s), N(1258291));
        QCOMPARE(ev(u"ISFOLDER"_s), B(false));
        QCOMPARE(ev(u"DEPTH"_s), N(1));
        QCOMPARE(ev(u"TAGS"_s), T(u"Green/Work"_s));
        QCOMPARE(ev(u"WIDTH"_s), N(4000));
        QCOMPARE(ev(u"HEIGHT"_s), N(3000));
        QCOMPARE(ev(u"TAKEN"_s), FxValue::date(QDateTime(QDate(2026, 7, 30), QTime(10, 0))));
        QCOMPARE(ev(u"CREATED"_s), FxValue::empty());
        QCOMPARE(ev(u"MODIFIED"_s), FxValue::date(QDateTime(QDate(2026, 8, 2), QTime(14, 5))));
    }

    void hugeNumbersClamp()
    {
        QCOMPARE(ev(u"LEFT(\"abc\", 99999999999999999999)"_s), T(u"abc"_s));
        QCOMPARE(ev(u"MID(\"abc\", 99999999999999999999, 1)"_s), T(u""_s));
        QCOMPARE(ev(u"DATE(99999999999999999999, 1, 1)"_s), E(FxError::Num));
    }
    void textFunctions()
    {
        QCOMPARE(ev(u"LEFT(NAME,3)"_s), T(u"hol"_s));
        QCOMPARE(ev(u"RIGHT(NAME)"_s), T(u"y"_s));
        QCOMPARE(ev(u"MID(NAME,2,3)"_s), T(u"oli"_s));
        QCOMPARE(ev(u"LEN(FULLNAME)"_s), N(11));
        QCOMPARE(ev(u"FIND(\"day\",NAME)"_s), N(5));
        QCOMPARE(ev(u"FIND(\"x\",NAME)"_s), E(FxError::Value));
        QCOMPARE(ev(u"FIND(\"DAY\",NAME)"_s), E(FxError::Value)); // case-sensitive
        QCOMPARE(ev(u"SUBSTITUTE(NAME,\"day\",\"night\")"_s), T(u"holinight"_s));
        QCOMPARE(ev(u"PROPER(\"hELLO wORLD\")"_s), T(u"Hello World"_s));
        QCOMPARE(ev(u"TRIM(\"  a  b \")"_s), T(u"a b"_s));
        QCOMPARE(ev(u"UPPER(NAME)"_s), T(u"HOLIDAY"_s));
        QCOMPARE(ev(u"LOWER(\"AbC\")"_s), T(u"abc"_s));
    }

    void textFormat()
    {
        QCOMPARE(ev(u"TEXT(7,\"00\")"_s), T(u"07"_s));
        QCOMPARE(ev(u"TEXT(ROWINFOLDER(),\"00\")"_s), T(u"02"_s));
        QCOMPARE(ev(u"TEXT(1.25,\"0.0\")"_s), T(u"1.3"_s)); // R3: half away from zero
        QCOMPARE(ev(u"TEXT(-1.25,\"0.0\")"_s), T(u"-1.3"_s));
        QCOMPARE(ev(u"TEXT(MODIFIED,\"yyyy\")"_s), T(u"2026"_s));
    }

    void predicates()
    {
        QCOMPARE(ev(u"CONTAINS(NAME,\"HOLI\")"_s), B(true));
        QCOMPARE(ev(u"STARTSWITH(NAME,\"HOL\")"_s), B(true));
        QCOMPARE(ev(u"STARTSWITH(NAME,\"day\")"_s), B(false));
        QCOMPARE(ev(u"ENDSWITH(NAME,\"DAY\")"_s), B(true));
        QCOMPARE(ev(u"REGEX(NAME,\"^h.*y$\")"_s), B(true));
        QCOMPARE(ev(u"REGEX(NAME,\"(\")"_s), E(FxError::Value));
    }

    void logic()
    {
        QCOMPARE(ev(u"IF(SIZE>10MB,\"big\",\"small\")"_s), T(u"small"_s));
        QCOMPARE(ev(u"IF(FALSE,1)"_s), B(false));
        QCOMPARE(ev(u"IFS(SIZE>10MB,\"a\",SIZE>1MB,\"b\")"_s), T(u"b"_s));
        QCOMPARE(ev(u"IFS(FALSE,1)"_s), E(FxError::NA));
        QCOMPARE(ev(u"AND(TRUE,1=2)"_s), B(false));
        QCOMPARE(ev(u"OR(1=2,TRUE)"_s), B(true));
        QCOMPARE(ev(u"NOT(TRUE)"_s), B(false));
        QCOMPARE(ev(u"AND(\"x\")"_s), E(FxError::Value));
        QCOMPARE(ev(u"AND(FALSE,\"x\")"_s), B(false)); // short-circuit
    }

    void dates()
    {
        QCOMPARE(ev(u"DAYS(MODIFIED)"_s), N(83));
        QCOMPARE(ev(u"DAYS(DATE(2026,10,24),MODIFIED)"_s), N(83));
        QCOMPARE(ev(u"YEAR(MODIFIED)"_s), N(2026));
        QCOMPARE(ev(u"MONTH(MODIFIED)"_s), N(8));
        QCOMPARE(ev(u"DAY(MODIFIED)"_s), N(2));
        QCOMPARE(ev(u"DATEVALUE(\"2026-01-31\")"_s), FxValue::date(QDateTime(QDate(2026, 1, 31), QTime(0, 0))));
        QCOMPARE(ev(u"DAYS(CREATED)"_s), E(FxError::Value));
        QCOMPARE(ev(u"YEAR(CREATED)"_s), E(FxError::Value));
        QCOMPARE(ev(u"TODAY()"_s), FxValue::date(QDateTime(QDate(2026, 10, 24), QTime(0, 0))));
    }

    void tags()
    {
        QCOMPARE(ev(u"HASTAG(\"work\")"_s), B(true));
        QCOMPARE(ev(u"HASTAG(\"Work\",\"Green\")"_s), B(true));
        QCOMPARE(ev(u"HASTAG(\"Work\",\"Red\")"_s), B(false));
        QCOMPARE(ev(u"HASTAG(\"Green/Work\")"_s), B(true));
        QCOMPARE(ev(u"HASTAG(\"Work\",\"NoSuchColour\")"_s), B(false));
        QCOMPARE(ev(u"COUNTTAGS()"_s), N(1));
    }

    void tagColourPrefix() // R2
    {
        QCOMPARE(fxColourIndex(u"orange"_s), TagColours::indexOfName(u"OrangeBright"_s));
        QCOMPARE(fxColourIndex(u"red"_s), TagColours::indexOfName(u"Red"_s)); // exact beats a prefix
        QCOMPARE(fxColourIndex(u"Nope"_s), -1);
        auto f = fileRow(u"/base/a.txt"_s, {}, {tag("OrangeBright", u"Fruit"_s)});
        QCOMPARE(ev(u"HASTAG(\"Fruit\",\"Orange\")"_s, f), B(true));
    }

    void rows()
    {
        QCOMPARE(ev(u"ROW()"_s), N(3));
        QCOMPARE(ev(u"ROWINFOLDER()"_s), N(2));
    }

    void countInEmptyBaseRefusesRelative()
    {
        ctx.baseFolder.clear();
        ctx.countIn = [](const QString &) { return 4; };
        QCOMPARE(ev(u"COUNTIN(\"Trip\")"_s), E(FxError::NA));
        QCOMPARE(ev(u"COUNTIN(\"/abs\")"_s), N(4));
        QCOMPARE(ev(u"COUNTIN(\"~\")"_s), N(4));
    }
    void countIn()
    {
        QStringList asked;
        ctx.baseFolder = u"/base"_s;
        ctx.countIn = [&asked](const QString &abs) {
            asked << abs;
            return abs == u"/base/Trip"_s ? 4 : -1;
        };
        QCOMPARE(ev(u"COUNTIN(\"Trip\")"_s), N(4));
        QCOMPARE(ev(u"COUNTIN(\"Trip\")"_s), N(4));
        QCOMPARE(asked.count(u"/base/Trip"_s), 1); // cached per folder
        QCOMPARE(ev(u"COUNTIN(\"~\")"_s), E(FxError::NA));
        QVERIFY(asked.contains(QDir::homePath()));
    }

    void arithmeticAndCompare()
    {
        QCOMPARE(ev(u"1+2*3"_s), N(7));
        QCOMPARE(ev(u"2^10"_s), N(1024));
        QCOMPARE(ev(u"1/0"_s), E(FxError::Div0));
        QCOMPARE(ev(u"\"a\"<>\"A\""_s), B(false));
        QCOMPARE(ev(u"\"b\">\"a\""_s), B(true));
        QCOMPARE(ev(u"SIZE>\"x\""_s), E(FxError::Value));
        QCOMPARE(ev(u"\"a\"&1&TRUE"_s), T(u"a1TRUE"_s));
        QCOMPARE(ev(u"CREATED=\"\""_s), B(true));
        QCOMPARE(ev(u"-SIZE"_s), N(-1258291));
    }

    void mockupSelectFormula()
    {
        const QString f = u"=AND(LEFT(NAME,3)=\"CED\", DAYS(MODIFIED)>30, NOT(HASTAG(\"Xiaomi\",\"Red\")))"_s;
        const QDateTime mod(QDate(2026, 8, 2), QTime(9, 0));
        QCOMPARE(ev(f, fileRow(u"/base/CED_report_01.docx"_s, mod, {tag("Green", u"Work"_s)})), B(true));
        QCOMPARE(ev(f, fileRow(u"/base/CED_mi13.pdf"_s, mod, {tag("Red", u"Xiaomi"_s)})), B(false));
    }

    void mockupRenameFormula()
    {
        QCOMPARE(ev(u"TEXT(ROWINFOLDER(),\"00\") & \" - \" & PROPER(NAME)"_s), T(u"02 - Holiday"_s));
    }

    void lazyTagsCalledOnce()
    {
        int calls = 0;
        holiday->loadTags = [&calls] {
            ++calls;
            return QList<Tag>{tag("Green", u"Work"_s)};
        };
        QCOMPARE(ev(u"LEN(NAME)"_s), N(7));
        QCOMPARE(calls, 0);
        QCOMPARE(ev(u"HASTAG(\"a\") & TAGS"_s), T(u"FALSEGreen/Work"_s));
        QCOMPARE(calls, 1);
    }

    void scriptMembers()
    {
        auto script = [](const QString &text) {
            QString lexErr;
            const auto toks = fxLex(text, &lexErr);
            FxParseError err;
            int pos = 0;
            const auto e = FxParser::parseExpression(toks, &pos, FxParser::Mode::Script, &err);
            if (!e)
                qWarning() << "parse error" << err.message;
            return e ? *e : nullptr;
        };
        QHash<QString, FxValue> vars{{u"f"_s, FxValue::fileRef(holiday)}};
        ctx.variables = &vars;
        const auto e = script(u"f.Ext = \"jpg\" And f.Contains(\"HOLI\")"_s);
        QVERIFY(e);
        QCOMPARE(fxEvaluate(e, nullptr, ctx), B(true));
        const auto g = script(u"g.Name"_s);
        QVERIFY(g);
        QCOMPARE(fxEvaluate(g, nullptr, ctx), E(FxError::Name));
        const auto u = script(u"f.Nope"_s);
        QCOMPARE(fxEvaluate(u, nullptr, ctx), E(FxError::Name));
        QCOMPARE(fxEvaluate(script(u"F.Size"_s), nullptr, ctx), N(1258291));
    }

    void folderSplit()
    {
        auto dir = std::make_shared<FxFile>();
        dir->path = u"/x/a.b"_s;
        dir->isFolder = true;
        dir->size = 4096;
        QCOMPARE(ev(u"NAME"_s, dir), T(u"a.b"_s));
        QCOMPARE(ev(u"EXT"_s, dir), T(QString()));
        QCOMPARE(ev(u"SIZE"_s, dir), N(0));
        QCOMPARE(ev(u"WIDTH"_s, dir), N(0));
        auto dot = std::make_shared<FxFile>();
        dot->path = u"/x/.bashrc"_s;
        QCOMPARE(ev(u"NAME"_s, dot), T(u".bashrc"_s));
        QCOMPARE(ev(u"EXT"_s, dot), T(QString()));
        dot->path = u"/x/a."_s;
        QCOMPARE(ev(u"NAME"_s, dot), T(u"a."_s));
        QCOMPARE(ev(u"EXT"_s, dot), T(QString()));
    }
};

QTEST_GUILESS_MAIN(FxEvalTest)
#include "fxeval_test.moc"

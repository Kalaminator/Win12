#include "fxlexer.h"
#include <QTest>

using namespace Qt::Literals::StringLiterals;

class FxLexerTest : public QObject
{
    Q_OBJECT

    static QList<FxToken> lex(const QString &src)
    {
        QString err;
        const auto t = fxLex(src, &err);
        if (!err.isEmpty())
            qWarning() << "lex error" << err;
        return t;
    }

private Q_SLOTS:
    void numbersAndSizes()
    {
        const auto t = lex(u"12 1.5 10MB 200KB"_s);
        QCOMPARE(t.size(), 5);
        const double want[] = {12, 1.5, 10485760, 204800};
        for (int i = 0; i < 4; ++i) {
            QCOMPARE(t[i].kind, FxToken::Number);
            QCOMPARE(t[i].number, want[i]);
        }
        QCOMPARE(t[2].text, u"10MB"_s);
        QCOMPARE(t[4].kind, FxToken::End);
    }
    void decimalSize()
    {
        const auto t = lex(u"1.5GB 3B"_s);
        QCOMPARE(t[0].number, 1.5 * 1024 * 1024 * 1024);
        QCOMPARE(t[1].number, 3.0);
    }
    void stringsWithDoubledQuote()
    {
        const auto t = lex(u"\"a \"\"q\"\" b\""_s);
        QCOMPARE(t.size(), 2);
        QCOMPARE(t[0].kind, FxToken::String);
        QCOMPARE(t[0].text, u"a \"q\" b"_s);
    }
    void operators()
    {
        const auto t = lex(u"<> <= := & ^"_s);
        QCOMPARE(t.size(), 6);
        const QStringList want = {u"<>"_s, u"<="_s, u":="_s, u"&"_s, u"^"_s};
        for (int i = 0; i < 5; ++i) {
            QCOMPARE(t[i].kind, FxToken::Op);
            QCOMPARE(t[i].text, want[i]);
        }
    }
    void commentAndNewline()
    {
        const auto t = lex(u"x ' hi\ny"_s);
        QCOMPARE(t.size(), 4);
        QCOMPARE(t[0].kind, FxToken::Ident);
        QCOMPARE(t[0].text, u"x"_s);
        QCOMPARE(t[1].kind, FxToken::Newline);
        QCOMPARE(t[2].kind, FxToken::Ident);
        QCOMPARE(t[2].text, u"y"_s);
        QCOMPARE(t[3].kind, FxToken::End);
    }
    void continuationLine()
    {
        const auto t = lex(u"a _\n& b"_s);
        QCOMPARE(t.size(), 4);
        QCOMPARE(t[0].kind, FxToken::Ident);
        QCOMPARE(t[1].kind, FxToken::Op);
        QCOMPARE(t[2].kind, FxToken::Ident);
        QCOMPARE(t[3].kind, FxToken::End);
    }
    void unterminatedStringIsError()
    {
        QString err;
        int line = 0, col = 0;
        fxLex(u"\"abc"_s, &err, &line, &col);
        QCOMPARE(err, u"Unterminated string"_s);
        QCOMPARE(line, 1);
        QCOMPARE(col, 1);
    }
    void positions()
    {
        const auto t = lex(u"\n  LEFT("_s);
        QCOMPARE(t[0].kind, FxToken::Newline);
        QCOMPARE(t[1].kind, FxToken::Ident);
        QCOMPARE(t[1].line, 2);
        QCOMPARE(t[1].col, 3);
        QCOMPARE(t[2].kind, FxToken::LParen);
    }
    void punctuation()
    {
        const auto t = lex(u"f.Name(a, b)"_s);
        const QList<FxToken::Kind> want = {FxToken::Ident, FxToken::Dot, FxToken::Ident, FxToken::LParen, FxToken::Ident,
                                           FxToken::Comma, FxToken::Ident, FxToken::RParen, FxToken::End};
        QCOMPARE(t.size(), want.size());
        for (int i = 0; i < want.size(); ++i)
            QCOMPARE(t[i].kind, want[i]);
    }
};

QTEST_GUILESS_MAIN(FxLexerTest)
#include "fxlexer_test.moc"

#include "fxlexer.h"
#include "rulematcher.h"
#include <KLocalizedString>

using namespace Qt::Literals::StringLiterals;

namespace
{
bool isIdentStart(QChar c)
{
    return (c >= u'A' && c <= u'Z') || (c >= u'a' && c <= u'z') || c == u'_';
}

bool isIdentChar(QChar c)
{
    return isIdentStart(c) || (c >= u'0' && c <= u'9');
}

bool isDigit(QChar c)
{
    return c >= u'0' && c <= u'9';
}
}

QList<FxToken> fxLex(const QString &src, QString *error, int *errLine, int *errCol)
{
    QList<FxToken> out;
    int i = 0, line = 1, lineStart = 0;
    const int len = src.size();
    auto at = [&](int k) { return k < len ? src[k] : QChar(); };
    auto fail = [&](const QString &msg, int l, int c) {
        *error = msg;
        if (errLine)
            *errLine = l;
        if (errCol)
            *errCol = c;
        return QList<FxToken>();
    };
    error->clear();

    while (i < len) {
        const QChar c = src[i];
        const int col = i - lineStart + 1;
        auto push = [&](FxToken::Kind k, const QString &text, int width) {
            out.append({k, text, 0, line, col});
            i += width;
        };

        if (c == u' ' || c == u'\t' || c == u'\r') {
            ++i;
        } else if (c == u'\'') {
            while (i < len && src[i] != u'\n')
                ++i;
        } else if (c == u'\n') {
            push(FxToken::Newline, u"\n"_s, 1);
            line++;
            lineStart = i;
        } else if (c == u'_' && !isIdentChar(at(i + 1))) {
            // VBA continuation: " _" then only blanks up to the line end joins the next line
            int k = i + 1;
            while (at(k) == u' ' || at(k) == u'\t' || at(k) == u'\r')
                ++k;
            if (at(k) != u'\n')
                return fail(i18n("Unexpected \"_\""), line, col);
            i = k + 1;
            line++;
            lineStart = i;
        } else if (isDigit(c)) {
            int k = i;
            while (isDigit(at(k)))
                ++k;
            if (at(k) == u'.' && isDigit(at(k + 1))) {
                ++k;
                while (isDigit(at(k)))
                    ++k;
            }
            double value = src.mid(i, k - i).toDouble();
            int u = k;
            while (isIdentStart(at(u)) && at(u) != u'_')
                ++u;
            const QString unit = src.mid(k, u - k);
            if (!unit.isEmpty() && !isIdentChar(at(u))) {
                bool ok;
                const qint64 mult = parseSize(u"1"_s + unit, &ok); // B KB MB GB TB
                if (ok) {
                    value *= mult;
                    k = u;
                }
            }
            out.append({FxToken::Number, src.mid(i, k - i), value, line, col});
            i = k;
        } else if (c == u'"') {
            QString s;
            int k = i + 1;
            for (;;) {
                if (k >= len || src[k] == u'\n')
                    return fail(i18n("Unterminated string"), line, col);
                if (src[k] == u'"') {
                    if (at(k + 1) != u'"')
                        break;
                    ++k;
                }
                s += src[k++];
            }
            push(FxToken::String, s, k + 1 - i);
        } else if (isIdentStart(c)) {
            int k = i;
            while (isIdentChar(at(k)))
                ++k;
            push(FxToken::Ident, src.mid(i, k - i), k - i);
        } else if (c == u'(') {
            push(FxToken::LParen, c, 1);
        } else if (c == u')') {
            push(FxToken::RParen, c, 1);
        } else if (c == u',') {
            push(FxToken::Comma, c, 1);
        } else if (c == u'.') {
            push(FxToken::Dot, c, 1);
        } else {
            const QString two = src.mid(i, 2);
            if (two == u"<>"_s || two == u"<="_s || two == u">="_s || two == u":="_s)
                push(FxToken::Op, two, 2);
            else if (u"+-*/^&=<>"_s.contains(c))
                push(FxToken::Op, c, 1);
            else
                return fail(i18n("Unexpected \"%1\"", c), line, col);
        }
    }
    out.append({FxToken::End, QString(), 0, line, len - lineStart + 1});
    return out;
}

#pragma once
#include <QList>
#include <QString>

struct FxToken {
    enum Kind { Number, String, Ident, Op, LParen, RParen, Comma, Dot, Newline, End };
    Kind kind;
    QString text; // source text; a String's unescaped contents
    double number = 0;
    int line = 1, col = 1;
};

// On error returns {} and sets *error (and the position, 1-based).
QList<FxToken> fxLex(const QString &src, QString *error, int *errLine = nullptr, int *errCol = nullptr);

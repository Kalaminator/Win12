#pragma once
#include "fxlexer.h"
#include "fxvalue.h"
#include <QStringList>
#include <memory>
#include <optional>
#include <vector>

struct FxNode {
    enum Kind { Literal, Column, Variable, Call, Member, Method, Unary, Binary };
    Kind kind;
    FxValue literal;
    QString name; // column, variable, function, member, method
    QString op; // Unary "-" "NOT"; Binary "+" ... "&" "=" "<>" ... "AND" "OR"
    std::vector<std::shared_ptr<FxNode>> args; // Member/Method: args[0] is the object, then the call's arguments
    int line = 1, col = 1;
};
using FxExpr = std::shared_ptr<FxNode>;

struct FxParseError {
    QString message;
    int line = 1, col = 1;
};

struct FxFunctionSpec {
    QString name;
    int minArgs, maxArgs; // -1 = any
    QString signature;
    QString help;
};

namespace FxFunctions
{
const QList<FxFunctionSpec> &all();
const FxFunctionSpec *find(const QString &upperName);
}

namespace FxColumns
{
const QStringList &names();
bool isColumn(const QString &upperName);
}

class FxParser
{
public:
    enum class Mode { Formula, Script };
    // Leading '=' optional; whole text consumed; Mode::Formula.
    static std::optional<FxExpr> parseFormula(const QString &text, FxParseError *err);
    // Stops before Newline/End/unmatched ')'; *pos is left on that token.
    static std::optional<FxExpr> parseExpression(const QList<FxToken> &, int *pos, Mode, FxParseError *err);
};

#pragma once
#include "fxparser.h"
#include <optional>
#include <utility>
#include <vector>

// A parsed VBA-like script. Building it never touches files; Task 8's interpreter turns it into a plan.
struct ScriptStmt {
    enum Kind { Assign, ForEach, If, Action, ExitFor };
    Kind kind;
    int line = 1;
    QString var;
    FxExpr expr; // Assign: var = expr | Dim var [= expr] ("As …" ignored; null without "="); ForEach: var In expr (a Call FILES)
    std::vector<ScriptStmt> body; // ForEach
    std::vector<std::pair<FxExpr, std::vector<ScriptStmt>>> branches; // If/ElseIf (condition, body)
    std::vector<ScriptStmt> elseBody;
    FxExpr target;
    QString action; // MoveTo CopyTo Rename Tag Untag Trash (canonical case)
    std::vector<FxExpr> args;
};
struct Script {
    std::vector<ScriptStmt> body;
};

std::optional<Script> parseScript(const QString &text, FxParseError *err);

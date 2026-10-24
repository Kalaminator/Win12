#pragma once
#include "fxfile.h"
#include "fxparser.h"
#include <QDate>
#include <QHash>
#include <functional>

struct FxContext {
    QDate today = QDate::currentDate();
    QString baseFolder; // COUNTIN relative paths; "~" expands to QDir::homePath()
    // default: QDir(..).entryList(AllEntries|Hidden|NoDotAndDotDot).size(); -1 -> #N/A
    std::function<int(const QString &absFolder)> countIn;
    QHash<QString, int> countCache; // COUNTIN, per absolute folder
    QHash<QString, FxValue> *variables = nullptr; // scripts; keys stored lower-case
};

// Excel semantics where Excel has the function; errors are values (#VALUE!, #NAME?, ...).
FxValue fxEvaluate(const FxExpr &, const std::shared_ptr<FxFile> &row, FxContext &);

// A colour word to a TagColours index: exact palette name (any case) first, else the first
// palette name starting with the word ("Orange" -> OrangeBright); -1 when none.
int fxColourIndex(const QString &word);

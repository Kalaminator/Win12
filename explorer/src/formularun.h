#pragma once
#include "fxparser.h"
#include "rulesearch.h"
#include <KFileItem>
#include <QObject>
#include <QUrl>
#include <memory>

struct FxFile;

struct FxRowResult {
    KFileItem item;
    std::shared_ptr<FxFile> row;
    FxValue value;
};

// Evaluates one formula per file/folder row: lists the folder through KIO (RuleSearch), then
// sorts, numbers and evaluates off the UI thread. Errors are per-row values, not a failed run.
class FormulaRun : public QObject
{
    Q_OBJECT
public:
    explicit FormulaRun(QObject *parent = nullptr);
    ~FormulaRun() override;
    // Rows sorted by relative path (numeric, case-insensitive, a folder before its children); stops a running one first
    void start(const QUrl &folder, bool recursive, const FxExpr &);
    // No listing, `items` keep their order (Rename many)
    void startOn(const KFileItemList &items, const QUrl &base, const FxExpr &);
    void stop(); // no results()/finished() afterwards
    bool isRunning() const;

Q_SIGNALS:
    void results(const QList<FxRowResult> &); // batches of <= 200, row order
    void finished(bool ok, const QString &error);

private:
    struct Run;
    void evaluate(KFileItemList items, const QUrl &base, const FxExpr &, bool sort);
    RuleSearch m_search;
    KFileItemList m_listed;
    QUrl m_folder;
    FxExpr m_expr;
    std::shared_ptr<Run> m_run; // shared with the worker; the worker never holds `this` past stop()
};

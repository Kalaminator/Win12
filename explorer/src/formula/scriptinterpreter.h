#pragma once
#include "planstep.h"
#include "script.h"
#include <atomic>
#include <functional>
#include <memory>

struct FxFile;

// Runs a parsed script as a dry run: it only builds a plan, it never changes files.
// Every executed statement counts toward the limit and checks the cancel flag.
class ScriptInterpreter
{
public:
    static constexpr qint64 DefaultMaxStatements = 1'000'000; // Call 11
    explicit ScriptInterpreter(const QString &baseFolder, qint64 maxStatements = DefaultMaxStatements);

    // Rows of Files(folder[, subfolders]): files only (folders are walked, never yielded); a lister should stop early when *cancel is set; a non-empty *error fails the script at the For Each line
    using Lister = std::function<QList<std::shared_ptr<FxFile>>(const QString &absFolder, bool subfolders, QString *error, const std::atomic_bool *cancel)>;
    void setLister(Lister); // default: QDirIterator, rows sorted and numbered like FormulaRun

    void setProgress(std::function<void(qint64 statements, int planSteps)>, int everyNStatements = 1000); // called from run()'s thread every N statements

    ScriptResult run(const Script &, const std::atomic_bool *cancel); // cancel -> ok = false, error "Stopped"

private:
    QString m_base;
    qint64 m_max;
    Lister m_lister;
    std::function<void(qint64, int)> m_progress;
    int m_progressEvery = 1000;
};

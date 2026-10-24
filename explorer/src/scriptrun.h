#pragma once
#include "planstep.h"
#include <QObject>
#include <memory>

// Runs a script (dry run, builds a plan) off the UI thread. Parsing happens on the caller's thread.
class ScriptRun : public QObject
{
    Q_OBJECT
public:
    explicit ScriptRun(QObject *parent = nullptr);
    ~ScriptRun() override;
    void start(const QString &text, const QString &baseFolder); // stops a running one first
    void stop(); // no progress()/finished() afterwards
    bool isRunning() const;

Q_SIGNALS:
    void progress(qint64 statements, int planSteps); // at most 4 per second
    void finished(const ScriptResult &); // a parse error: ok = false and its line

private:
    struct Run;
    std::shared_ptr<Run> m_run; // shared with the worker; the worker never touches `this` past stop()
};

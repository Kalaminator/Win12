#pragma once
#include "planstep.h"
#include <QList>
#include <QObject>
#include <QPointer>
#include <QUrl>
#include <QWidget>

class TagStore;

// Runs a planned list of PlanSteps as sequential KIO jobs (the only place phase 4 changes files).
// One step at a time; a failed step is counted and the run continues; cancel() stops after the current step.
class PlanRunner : public QObject
{
    Q_OBJECT
public:
    PlanRunner(QWidget *window, TagStore *tags, QObject *parent = nullptr);
    void run(const QList<PlanStep> &); // skipped rows ignored; emits progress(i, total) before each step
    void cancel();                     // after the current step
    bool isRunning() const { return m_running; }
    int executedCount() const { return m_executed.size(); }
    void undo();              // reverses executed steps back to front; afterwards executedCount() == 0
    QString undoNote() const; // "%1 trashed item stays in the Trash" when executed steps include Trash, else empty

Q_SIGNALS:
    void progress(int done, int total);
    void finished(int done, int failed, const QString &firstError);
    void undone(int reversed, const QString &firstError);

private:
    struct Done {
        PlanStep::Kind kind;
        QUrl from; // the original url
        QUrl to;   // where it is now (Move/Copy/Rename)
        ::Tag tag;
    };
    void next();
    void start(const PlanStep &);
    void transfer(const PlanStep &);
    void stepDone(const QString &error);
    void undoNext();
    void undoDone(const QString &error);

    QPointer<QWidget> m_window;
    TagStore *m_tags;
    QList<PlanStep> m_queue; // run: the non-skipped steps
    QList<Done> m_undoQueue; // undo: executed steps, back to front
    QList<Done> m_executed;
    int m_index = 0, m_done = 0, m_failed = 0;
    QString m_firstError;
    bool m_running = false, m_cancel = false;
};

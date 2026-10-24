#pragma once
#include "planrunner.h"
#include <QWidget>

class QLabel;
class QProgressBar;
class QPushButton;
class QTreeWidget;

// The dry-run plan table with Run plan / Stop / Undo plan. Nothing runs until the user clicks Run plan.
class PlanView : public QWidget
{
    Q_OBJECT
public:
    PlanView(QWidget *window, TagStore *tags, QWidget *parent = nullptr);
    void setPlan(const QList<PlanStep> &);
    void setBusy(bool); // a script build is in flight: Run plan stays disabled
    QList<PlanStep> plan() const { return m_plan; }
    PlanRunner &runner() { return m_runner; }

Q_SIGNALS:
    void runStarted();
    void runFinished(int done, int failed);

private:
    void updateButtons();
    PlanRunner m_runner;
    QList<PlanStep> m_plan;
    QTreeWidget *m_tree;
    QLabel *m_status;
    QProgressBar *m_progress;
    QPushButton *m_run, *m_stop, *m_undo;
    QString m_pendingNote;
    bool m_busy = false;
    bool m_fresh = false; // a plan is shown that has not been run yet
};

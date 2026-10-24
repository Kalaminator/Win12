#pragma once
#include "scriptrun.h"
#include <QDialog>
#include <QUrl>

class PlanView;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class RuleLibrary;
class TagStore;

// "Script editor": builds a dry-run plan from a script; files change only when the user clicks Run plan.
class ScriptDialog : public QDialog
{
    Q_OBJECT
public:
    ScriptDialog(const QUrl &baseFolder, TagStore *tags, RuleLibrary *library, QWidget *parent);
    void setScript(const QString &);
    QString script() const;
    void buildPlan();
    PlanView *planView() const { return m_plan; }
    void reject() override;

private:
    void updateButtons();
    QUrl m_base;
    RuleLibrary *m_library;
    ScriptRun m_run;
    QPlainTextEdit *m_edit;
    QLabel *m_status;
    QPushButton *m_build, *m_stop, *m_save;
    PlanView *m_plan;
};

#include "scriptdialog.h"
#include "planview.h"
#include "rulelibrary.h"
#include <KLocalizedString>
#include <KMessageBox>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextBlock>
#include <QVBoxLayout>

ScriptDialog::ScriptDialog(const QUrl &baseFolder, TagStore *tags, RuleLibrary *library, QWidget *parent)
    : QDialog(parent)
    , m_base(baseFolder)
    , m_library(library)
{
    setWindowTitle(i18n("Script editor"));
    resize(680, 620);
    auto *top = new QVBoxLayout(this);
    m_edit = new QPlainTextEdit;
    m_edit->setObjectName(QStringLiteral("scriptEdit"));
    m_edit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_edit->setTabChangesFocus(false);
    top->addWidget(m_edit, 1);

    auto *bar = new QHBoxLayout;
    auto mk = [&](const QString &text, const char *name) {
        auto *b = new QPushButton(text);
        b->setObjectName(QLatin1String(name));
        b->setAutoDefault(false);
        bar->addWidget(b);
        return b;
    };
    m_build = mk(i18n("Build plan"), "buildPlan");
    m_stop = mk(i18n("Stop"), "stopScript");
    m_save = mk(i18n("Save script…"), "saveScript");
    bar->addStretch();
    top->addLayout(bar);
    m_status = new QLabel;
    m_status->setObjectName(QStringLiteral("scriptStatus"));
    top->addWidget(m_status);

    m_plan = new PlanView(this, tags);
    top->addWidget(m_plan, 2);
    auto *close = new QPushButton(i18n("Close"));
    close->setAutoDefault(false);
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
    auto *cb = new QHBoxLayout;
    cb->addStretch();
    cb->addWidget(close);
    top->addLayout(cb);

    connect(m_build, &QPushButton::clicked, this, &ScriptDialog::buildPlan);
    connect(m_stop, &QPushButton::clicked, this, [this] {
        m_run.stop();
        m_plan->setPlan({});
        m_status->setText(i18n("Stopped"));
        updateButtons();
    });
    connect(m_save, &QPushButton::clicked, this, [this] {
        if (!m_library)
            return;
        saveRuleInteractively(*m_library, SavedRule::Kind::Script, script(), this);
    });
    connect(&m_run, &ScriptRun::progress, this, [this](qint64 statements, int steps) {
        m_status->setText(i18n("Building plan… %1 statements, %2 steps", statements, steps));
    });
    connect(&m_run, &ScriptRun::finished, this, [this](const ScriptResult &r) {
        m_plan->setBusy(false);
        if (!r.ok) {
            m_plan->setPlan({});
            m_status->setText(i18n("Line %1: %2", r.line, r.error));
            if (r.line > 0)
                m_edit->setTextCursor(QTextCursor(m_edit->document()->findBlockByNumber(r.line - 1)));
        } else {
            m_plan->setPlan(r.plan);
            m_status->setText(i18np("%1 step", "%1 steps", r.plan.size()));
        }
        updateButtons();
    });
    connect(m_edit, &QPlainTextEdit::textChanged, this, [this] {
        if (!m_plan->runner().isRunning()) // a plan always matches the text it was built from
            m_plan->setPlan({});
    });
    connect(&m_plan->runner(), &PlanRunner::progress, this, &ScriptDialog::updateButtons);
    connect(m_plan, &PlanView::runStarted, this, &ScriptDialog::updateButtons);
    connect(m_plan, &PlanView::runFinished, this, &ScriptDialog::updateButtons);
    if (!m_base.isLocalFile())
        m_status->setText(i18n("Scripts work on local folders only"));
    updateButtons();
}

void ScriptDialog::setScript(const QString &text)
{
    m_edit->setPlainText(text);
}

QString ScriptDialog::script() const
{
    return m_edit->toPlainText();
}

void ScriptDialog::updateButtons()
{
    m_build->setEnabled(m_base.isLocalFile() && !m_plan->runner().isRunning());
    m_stop->setEnabled(m_run.isRunning());
    m_edit->setReadOnly(m_plan->runner().isRunning());
}

void ScriptDialog::buildPlan()
{
    if (!m_base.isLocalFile() || m_plan->runner().isRunning())
        return;
    m_plan->setPlan({});
    m_status->setText(i18n("Building plan… %1 statements, %2 steps", 0, 0));
    m_run.start(script(), m_base.toLocalFile());
    m_plan->setBusy(m_run.isRunning()); // a parse error may finish synchronously
    updateButtons();
}

void ScriptDialog::reject()
{
    if (m_plan->runner().isRunning()) {
        KMessageBox::information(this, i18n("The plan is running. Stop it first."));
        return;
    }
    m_run.stop();
    m_plan->setBusy(false);
    QDialog::reject();
}

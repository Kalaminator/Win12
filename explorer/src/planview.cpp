#include "planview.h"
#include <KLocalizedString>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

PlanView::PlanView(QWidget *window, TagStore *tags, QWidget *parent)
    : QWidget(parent)
    , m_runner(window, tags)
{
    auto *top = new QVBoxLayout(this);
    top->setContentsMargins(0, 0, 0, 0);
    m_tree = new QTreeWidget;
    m_tree->setObjectName(QStringLiteral("planTree"));
    m_tree->setRootIsDecorated(false);
    m_tree->setHeaderLabels({i18n("Action"), i18n("File"), i18n("Note")});
    m_tree->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_tree->header()->setStretchLastSection(true);
    top->addWidget(m_tree, 1);
    m_status = new QLabel;
    m_status->setObjectName(QStringLiteral("planStatus"));
    top->addWidget(m_status);
    m_progress = new QProgressBar;
    m_progress->setObjectName(QStringLiteral("planProgress"));
    m_progress->hide();
    top->addWidget(m_progress);
    auto *bar = new QHBoxLayout;
    bar->addStretch();
    auto mk = [&](const QString &text, const char *name) {
        auto *b = new QPushButton(text);
        b->setObjectName(QLatin1String(name));
        b->setAutoDefault(false);
        bar->addWidget(b);
        return b;
    };
    m_undo = mk(i18n("Undo plan"), "undoPlan");
    m_stop = mk(i18n("Stop"), "stopPlan");
    m_run = mk(i18n("Run plan (%1 steps)", 0), "runPlan");
    top->addLayout(bar);

    connect(m_run, &QPushButton::clicked, this, [this] {
        m_fresh = false;
        m_progress->setRange(0, 0);
        m_progress->show();
        m_status->setText(i18n("Running…"));
        Q_EMIT runStarted();
        m_runner.run(m_plan);
        updateButtons();
    });
    connect(m_stop, &QPushButton::clicked, &m_runner, &PlanRunner::cancel);
    connect(m_undo, &QPushButton::clicked, this, [this] {
        const QString note = m_runner.undoNote(); // before undo() clears the executed list
        m_undo->setEnabled(false);
        m_status->setText(i18n("Undoing…"));
        m_runner.undo();
        m_pendingNote = note;
    });
    connect(&m_runner, &PlanRunner::progress, this, [this](int done, int total) {
        m_progress->setRange(0, total);
        m_progress->setValue(done);
        m_status->setText(i18n("Running… %1 of %2", done, total));
    });
    connect(&m_runner, &PlanRunner::finished, this, [this](int done, int failed, const QString &err) {
        m_progress->hide();
        m_status->setText(failed ? i18n("%1 done, %2 failed: %3", done, failed, err) : i18np("%1 step done", "%1 steps done", done));
        updateButtons();
        Q_EMIT runFinished(done, failed);
    });
    connect(&m_runner, &PlanRunner::undone, this, [this](int reversed, const QString &err) {
        QString text = err.isEmpty() ? i18np("%1 step undone", "%1 steps undone", reversed) : i18n("%1 undone: %2", reversed, err);
        if (!m_pendingNote.isEmpty())
            text += QStringLiteral(" — ") + m_pendingNote;
        m_pendingNote.clear();
        m_status->setText(text);
        updateButtons();
    });
    updateButtons();
}

void PlanView::setPlan(const QList<PlanStep> &plan)
{
    m_plan = plan;
    m_fresh = true;
    m_tree->clear();
    for (const PlanStep &s : plan) {
        auto *it = new QTreeWidgetItem(m_tree, {s.actionText(), s.fileText(), s.note});
        if (s.skipped)
            for (int c = 0; c < 3; ++c)
                it->setForeground(c, palette().placeholderText());
    }
    updateButtons();
}

void PlanView::setBusy(bool busy)
{
    m_busy = busy;
    updateButtons();
}

void PlanView::updateButtons()
{
    int n = 0;
    for (const PlanStep &s : std::as_const(m_plan))
        n += !s.skipped;
    const bool running = m_runner.isRunning();
    m_run->setText(i18n("Run plan (%1 steps)", n));
    m_run->setEnabled(m_fresh && n > 0 && !running && !m_busy);
    m_stop->setEnabled(running);
    m_undo->setEnabled(!running && m_runner.executedCount() > 0);
}

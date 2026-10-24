#include "scriptrun.h"
#include "scriptinterpreter.h"
#include <QElapsedTimer>
#include <QMutex>
#include <QtConcurrent/QtConcurrentRun>
#include <atomic>

// stop() sets `cancel` under `lock`; the worker posts to the ScriptRun only while holding `lock`
// with `cancel` clear (FormulaRun's pattern).
struct ScriptRun::Run {
    QMutex lock;
    std::atomic_bool cancel{false};
};

ScriptRun::ScriptRun(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<ScriptResult>();
}

ScriptRun::~ScriptRun()
{
    stop();
}

bool ScriptRun::isRunning() const
{
    return m_run != nullptr;
}

void ScriptRun::stop()
{
    if (!m_run)
        return;
    {
        QMutexLocker l(&m_run->lock);
        m_run->cancel = true;
    }
    m_run.reset();
}

void ScriptRun::start(const QString &text, const QString &baseFolder)
{
    stop();
    auto run = std::make_shared<Run>();
    m_run = run;
    ScriptRun *self = this;
    auto post = [run, self](auto f) {
        QMutexLocker l(&run->lock);
        if (run->cancel)
            return;
        QMetaObject::invokeMethod(self, [run, f] { if (!run->cancel) f(); }, Qt::QueuedConnection);
    };

    FxParseError err;
    const auto script = parseScript(text, &err);
    if (!script) {
        ScriptResult r;
        r.ok = false;
        r.error = err.message;
        r.line = err.line;
        post([self, r] {
            self->m_run.reset();
            Q_EMIT self->finished(r);
        });
        return;
    }

    (void)QtConcurrent::run([=, script = *script] {
        ScriptInterpreter in(baseFolder);
        QElapsedTimer since;
        since.start();
        bool first = true;
        in.setProgress([&](qint64 n, int steps) {
            if (!first && !since.hasExpired(250))
                return;
            first = false;
            since.restart();
            post([self, n, steps] { Q_EMIT self->progress(n, steps); });
        }, 1000);
        const ScriptResult r = in.run(script, &run->cancel);
        post([self, r] {
            self->m_run.reset();
            Q_EMIT self->finished(r);
        });
    });
}

#include "formularun.h"
#include "fxeval.h"
#include "fxfile.h"
#include <QCollator>
#include <QElapsedTimer>
#include <QHash>
#include <QMutex>
#include <QtConcurrent/QtConcurrentRun>
#include <algorithm>
#include <atomic>
#include <numeric>

// One per evaluation. stop() sets `cancel` under `lock`, and the worker posts to the
// FormulaRun only while holding `lock` with `cancel` clear (TagSearch's pattern).
struct FormulaRun::Run {
    QMutex lock;
    std::atomic_bool cancel{false};
};

FormulaRun::FormulaRun(QObject *parent)
    : QObject(parent)
{
    connect(&m_search, &RuleSearch::found, this, [this](const KFileItemList &items) {
        m_listed += items;
    });
    connect(&m_search, &RuleSearch::finished, this, [this](bool ok, const QString &error) {
        KFileItemList items = std::move(m_listed);
        m_listed.clear();
        if (!ok)
            Q_EMIT finished(false, error);
        else
            evaluate(std::move(items), m_folder, m_expr, true);
    });
}

FormulaRun::~FormulaRun()
{
    stop();
}

bool FormulaRun::isRunning() const
{
    return m_search.isRunning() || m_run;
}

void FormulaRun::stop()
{
    m_search.stop();
    m_listed.clear();
    if (!m_run)
        return;
    {
        QMutexLocker l(&m_run->lock);
        m_run->cancel = true;
    }
    m_run.reset();
}

void FormulaRun::start(const QUrl &folder, bool recursive, const FxExpr &expr)
{
    stop();
    m_folder = folder;
    m_expr = expr;
    m_search.start(folder, recursive, *RuleMatcher::compile({}, nullptr));
}

void FormulaRun::startOn(const KFileItemList &items, const QUrl &base, const FxExpr &expr)
{
    stop();
    evaluate(items, base, expr, false);
}

void FormulaRun::evaluate(KFileItemList items, const QUrl &base, const FxExpr &expr, bool sort)
{
    auto run = std::make_shared<Run>();
    m_run = run;
    FormulaRun *self = this;

    auto post = [run, self](auto f) {
        QMutexLocker l(&run->lock);
        if (run->cancel)
            return;
        QMetaObject::invokeMethod(self, [run, f] { if (!run->cancel) f(); }, Qt::QueuedConnection);
    };

    (void)QtConcurrent::run([=, items = std::move(items)]() mutable {
        // Relative paths split once; compared part by part so "sub" and its children stay together
        QList<QStringList> parts;
        parts.reserve(items.size());
        for (const KFileItem &item : std::as_const(items))
            parts << fxRelativePath(item.url(), base).split(u'/');
        QList<int> order(items.size());
        std::iota(order.begin(), order.end(), 0);
        if (sort) {
            QCollator coll;
            coll.setNumericMode(true);
            coll.setCaseSensitivity(Qt::CaseInsensitive);
            std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
                const QStringList &x = parts[a], &y = parts[b];
                for (int i = 0; i < x.size() && i < y.size(); ++i)
                    if (const int c = coll.compare(x[i], y[i]))
                        return c < 0;
                return x.size() < y.size();
            });
        }

        FxContext ctx; // one per run: COUNTIN's cache lives here
        ctx.baseFolder = base.isLocalFile() ? base.toLocalFile() : QString();
        QHash<QString, int> inFolder; // parent relative path -> rows so far
        QList<FxRowResult> batch;
        QElapsedTimer sinceFlush;
        sinceFlush.start();
        int row = 0;
        for (const int i : std::as_const(order)) {
            if (run->cancel)
                return;
            const KFileItem &item = items[i];
            const QStringList &p = parts[i];
            const int rowInFolder = ++inFolder[p.mid(0, p.size() - 1).join(u'/')];
            auto file = fxFileFor(item, base, ++row, rowInFolder);
            FxValue value = fxEvaluate(expr, file, ctx);
            batch << FxRowResult{item, std::move(file), std::move(value)};
            if (batch.size() >= 200 || sinceFlush.hasExpired(250)) {
                post([self, batch] { Q_EMIT self->results(batch); });
                batch.clear();
                sinceFlush.restart();
            }
        }
        if (!batch.isEmpty())
            post([self, batch] { Q_EMIT self->results(batch); });
        post([self] {
            self->m_run.reset();
            Q_EMIT self->finished(true, QString());
        });
    });
}

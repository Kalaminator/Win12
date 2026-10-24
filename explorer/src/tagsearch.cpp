#include "tagsearch.h"
#include "tagstore.h"
#include <Baloo/IndexerConfig>
#include <Baloo/Query>
#include <KLocalizedString>
#include <QDirIterator>
#include <QFileInfo>
#include <QMutex>
#include <QtConcurrent/QtConcurrentRun>
#include <algorithm>
#include <atomic>

// One per start(). stop() sets `cancel` under `lock`, and the worker posts to the
// TagSearch only while holding `lock` with `cancel` clear, so once stop() returns
// (the destructor calls it) the worker never touches the object again.
struct TagSearch::Run {
    QMutex lock;
    std::atomic_bool cancel{false};
};

TagSearch::TagSearch(QObject *parent)
    : QObject(parent)
{
}

TagSearch::~TagSearch()
{
    stop();
}

TagSearch::Engine TagSearch::engineFor(const QUrl &folder)
{
    if (!folder.isLocalFile())
        return Engine::Walk;
    const Baloo::IndexerConfig c;
    return c.fileIndexingEnabled() && c.shouldBeIndexed(folder.toLocalFile()) ? Engine::Baloo : Engine::Walk;
}

bool TagSearch::matches(const QStringList &have, const QStringList &wanted, Mode mode)
{
    if (have.isEmpty())
        return false;
    const auto hasTag = [&](const QString &t) { return have.contains(t); };
    return mode == Mode::All ? std::all_of(wanted.begin(), wanted.end(), hasTag) : std::any_of(wanted.begin(), wanted.end(), hasTag);
}

void TagSearch::stop()
{
    if (!m_run)
        return;
    {
        QMutexLocker l(&m_run->lock);
        m_run->cancel = true;
    }
    m_run.reset();
}

void TagSearch::start(const QUrl &folder, const QList<Tag> &tags, Mode mode, Engine engine)
{
    stop();
    auto run = std::make_shared<Run>();
    m_run = run;
    TagSearch *self = this;
    const QString path = folder.isLocalFile() ? folder.toLocalFile() : QString();
    const QStringList wanted = encodeAll(tags);

    // Worker side: queue `f` into the object's thread unless stopped; `f` re-checks there,
    // dropping a batch posted just before stop().
    auto post = [run, self](auto f) {
        QMutexLocker l(&run->lock);
        if (run->cancel)
            return;
        QMetaObject::invokeMethod(self, [run, f] { if (!run->cancel) f(); }, Qt::QueuedConnection);
    };

    (void)QtConcurrent::run([=] {
        auto end = [&](bool ok) {
            post([self, ok, folder, engine] {
                self->m_run.reset();
                // i18n on the UI thread: KLocalizedString warns when first used from a pool thread
                Q_EMIT self->finished(ok, ok ? QString() : i18n("The folder %1 does not exist.", folder.toDisplayString(QUrl::PreferLocalFile)), engine);
            });
        };
        if (path.isEmpty() || !QFileInfo(path).isDir())
            return end(false);

        KFileItemList batch;
        auto add = [&](const QString &file) {
            batch << KFileItem(QUrl::fromLocalFile(file)); // mime type resolved later, on demand
            if (batch.size() >= 200) {
                post([self, batch] { Q_EMIT self->found(batch); });
                batch.clear();
            }
        };

        if (engine == Engine::Baloo) {
            // ponytail: Query::exec() is what QueryRunnable::run() does; running it here keeps one cancel path
            QStringList terms;
            for (const QString &t : wanted)
                terms << QStringLiteral("tag:\"%1\"").arg(t);
            Baloo::Query q;
            q.setSearchString(terms.join(mode == Mode::All ? QStringLiteral(" AND ") : QStringLiteral(" OR ")));
            q.setIncludeFolder(path);
            Baloo::ResultIterator it = q.exec();
            // Baloo's tag: is a loose word match; keep only exact hits (also drops stale index entries)
            while (!run->cancel && it.next())
                if (matches(encodeAll(TagStore::read(QUrl::fromLocalFile(it.filePath()))), wanted, mode))
                    add(it.filePath());
        } else {
            // ponytail: the walker re-reads xattrs on every search; a result cache if the user filters the same USB stick repeatedly
            QDirIterator it(path, QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
            while (!run->cancel && it.hasNext()) {
                const QString file = it.next();
                if (matches(encodeAll(TagStore::read(QUrl::fromLocalFile(file))), wanted, mode))
                    add(file);
            }
        }
        if (run->cancel)
            return;
        if (!batch.isEmpty())
            post([self, batch] { Q_EMIT self->found(batch); });
        end(true);
    });
}

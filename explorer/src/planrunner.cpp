#include "planrunner.h"
#include "exploreractions.h"
#include "tagstore.h"
#include <KFileItem>
#include <KIO/CopyJob>
#include <KIO/FileUndoManager>
#include <KIO/MkpathJob>
#include <KJobWidgets>
#include <KLocalizedString>
#include <QFileInfo>
#include <QPointer>
#include <QTimer>
#include <memory>

PlanRunner::PlanRunner(QWidget *window, TagStore *tags, QObject *parent)
    : QObject(parent)
    , m_window(window)
    , m_tags(tags)
{
}

void PlanRunner::run(const QList<PlanStep> &plan)
{
    if (m_running)
        return;
    m_queue.clear();
    for (const PlanStep &s : plan)
        if (!s.skipped)
            m_queue << s;
    m_index = m_done = m_failed = 0;
    m_firstError.clear();
    m_cancel = false;
    m_running = true;
    QTimer::singleShot(0, this, &PlanRunner::next);
}

void PlanRunner::cancel()
{
    m_cancel = true;
}

void PlanRunner::next()
{
    if (m_cancel || m_index >= m_queue.size()) {
        m_running = false;
        Q_EMIT finished(m_done, m_failed, m_firstError);
        return;
    }
    Q_EMIT progress(m_index, m_queue.size());
    start(m_queue.at(m_index));
}

void PlanRunner::stepDone(const QString &error)
{
    ++m_index;
    if (error.isEmpty()) {
        ++m_done;
    } else {
        ++m_failed;
        if (m_firstError.isEmpty())
            m_firstError = error;
    }
    // queued: Rename and Tag finish synchronously, this keeps the stack flat over long plans
    QTimer::singleShot(0, this, &PlanRunner::next);
}

void PlanRunner::start(const PlanStep &s)
{
    switch (s.kind) {
    case PlanStep::Move:
    case PlanStep::Copy:
        transfer(s);
        return;
    case PlanStep::Rename: {
        QPointer<PlanRunner> self(this); // rename spins a nested event loop: the runner may be deleted inside it
        const QString error = ExplorerActions::rename(KFileItem(s.source), s.newName, m_window); // refuses existing targets
        if (!self)
            return;
        if (error.isEmpty() && s.newName != s.source.fileName()) {
            QUrl to = s.source.adjusted(QUrl::RemoveFilename);
            to.setPath(to.path() + s.newName);
            m_executed << Done{s.kind, s.source, to, {}};
        }
        stepDone(error);
        return;
    }
    case PlanStep::Tag:
    case PlanStep::Untag: {
        // a no-op (tag already there / not there) is not recorded, so Undo never strips a tag the file had before
        const bool has = TagStore::read(s.source).contains(s.tag);
        if (has == (s.kind == PlanStep::Tag)) {
            stepDone({});
            return;
        }
        const TagStore::Error e = s.kind == PlanStep::Tag ? m_tags->add({s.source}, s.tag) : m_tags->remove({s.source}, s.tag);
        if (e == TagStore::Error::None)
            m_executed << Done{s.kind, s.source, s.source, s.tag};
        stepDone(e == TagStore::Error::None ? QString() : TagStore::errorText(e));
        return;
    }
    case PlanStep::Trash: {
        KIO::Job *job = KIO::trash({s.source}, KIO::HideProgressInfo);
        KJobWidgets::setWindow(job, m_window);
        KIO::FileUndoManager::self()->recordJob(KIO::FileUndoManager::Trash, {s.source}, QUrl(QStringLiteral("trash:/")), job);
        connect(job, &KJob::result, this, [this, s](KJob *j) {
            if (!j->error())
                m_executed << Done{s.kind, s.source, {}, {}};
            stepDone(j->error() ? j->errorString() : QString());
        });
        return;
    }
    }
}

void PlanRunner::transfer(const PlanStep &s)
{
    const auto go = [this, s] {
        // no Overwrite flag: an existing name brings up KIO's conflict dialog (window set below)
        KIO::CopyJob *job = s.kind == PlanStep::Move ? KIO::move(s.source, s.destFolder, KIO::HideProgressInfo)
                                                     : KIO::copy(s.source, s.destFolder, KIO::HideProgressInfo);
        KJobWidgets::setWindow(job, m_window);
        KIO::FileUndoManager::self()->recordCopyJob(job);
        // the real destination: the conflict dialog may rename it; folders also report every file inside
        auto dest = std::make_shared<QUrl>();
        connect(job, &KIO::CopyJob::copyingDone, this, [s, dest](KIO::Job *, const QUrl &from, const QUrl &to) {
            if (from == s.source)
                *dest = to;
        });
        connect(job, &KJob::result, this, [this, s, dest](KJob *j) {
            if (!j->error() && dest->isValid()) // valid only when it really landed (not "Skip")
                m_executed << Done{s.kind, s.source, *dest, {}};
            if (j->error() == KIO::ERR_USER_CANCELED) { // "Cancel" in the conflict dialog = Stop, not a failed step
                m_cancel = true;
                ++m_index;
                QTimer::singleShot(0, this, &PlanRunner::next);
                return;
            }
            stepDone(j->error() ? j->errorString() : QString());
        });
    };
    if (s.destFolder.isLocalFile() && QFileInfo(s.destFolder.toLocalFile()).isDir()) {
        go();
        return;
    }
    KIO::MkpathJob *mk = KIO::mkpath(s.destFolder, QUrl(), KIO::HideProgressInfo);
    KJobWidgets::setWindow(mk, m_window);
    connect(mk, &KJob::result, this, [this, go](KJob *j) {
        if (j->error())
            stepDone(j->errorString());
        else
            go();
    });
}

void PlanRunner::undo()
{
    if (m_running)
        return;
    m_undoQueue.clear();
    for (auto it = m_executed.crbegin(); it != m_executed.crend(); ++it)
        m_undoQueue << *it;
    m_executed.clear();
    m_index = m_done = 0;
    m_firstError.clear();
    m_running = true;
    QTimer::singleShot(0, this, &PlanRunner::undoNext);
}

void PlanRunner::undoNext()
{
    if (m_index >= m_undoQueue.size()) {
        m_running = false;
        m_undoQueue.clear();
        Q_EMIT undone(m_done, m_firstError);
        return;
    }
    const Done d = m_undoQueue.at(m_index++);
    const auto jobDone = [this](KJob *j) {
        undoDone(j->error() ? j->errorString() : QString());
    };
    switch (d.kind) {
    case PlanStep::Move: {
        // ponytail: folders mkpath created stay behind (empty); the user's Ctrl+Z history is not touched
        KIO::CopyJob *job = KIO::moveAs(d.to, d.from, KIO::HideProgressInfo);
        KJobWidgets::setWindow(job, m_window);
        connect(job, &KJob::result, this, jobDone);
        return;
    }
    case PlanStep::Copy: {
        KIO::Job *job = KIO::trash({d.to}, KIO::HideProgressInfo);
        KJobWidgets::setWindow(job, m_window);
        connect(job, &KJob::result, this, jobDone);
        return;
    }
    case PlanStep::Rename: {
        QPointer<PlanRunner> self(this); // nested event loop, see start()
        const QString error = ExplorerActions::rename(KFileItem(d.to), d.from.fileName(), m_window);
        if (self)
            undoDone(error);
        return;
    }
    case PlanStep::Tag:
    case PlanStep::Untag: {
        const TagStore::Error e = d.kind == PlanStep::Tag ? m_tags->remove({d.from}, d.tag) : m_tags->add({d.from}, d.tag);
        undoDone(e == TagStore::Error::None ? QString() : TagStore::errorText(e));
        return;
    }
    case PlanStep::Trash: // not reversed (undoNote); the user's Ctrl+Z restores it through FileUndoManager
        QTimer::singleShot(0, this, &PlanRunner::undoNext);
        return;
    }
}

void PlanRunner::undoDone(const QString &error)
{
    if (error.isEmpty())
        ++m_done;
    else if (m_firstError.isEmpty())
        m_firstError = error;
    QTimer::singleShot(0, this, &PlanRunner::undoNext);
}

QString PlanRunner::undoNote() const
{
    const auto trashed = std::count_if(m_executed.cbegin(), m_executed.cend(), [](const Done &d) {
        return d.kind == PlanStep::Trash;
    });
    return trashed ? i18np("%1 trashed item stays in the Trash", "%1 trashed items stay in the Trash", int(trashed)) : QString();
}

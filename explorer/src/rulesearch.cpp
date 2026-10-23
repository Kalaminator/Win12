#include "rulesearch.h"
#include <QDir>

RuleSearch::RuleSearch(QObject *parent)
    : QObject(parent)
{
}

bool RuleSearch::isRunning() const
{
    return !m_job.isNull();
}

void RuleSearch::stop()
{
    if (m_job) {
        m_job->disconnect(this);
        m_job->kill(KJob::Quietly);
        m_job = nullptr;
    }
}

void RuleSearch::start(const QUrl &folder, bool recursive, const RuleMatcher &matcher)
{
    stop();
    m_folder = folder;
    m_matcher = matcher;
    KIO::ListJob *job = recursive ? KIO::listRecursive(folder, KIO::HideProgressInfo) : KIO::listDir(folder, KIO::HideProgressInfo);
    m_job = job;
    connect(job, &KIO::ListJob::entries, this, [this](KIO::Job *, const KIO::UDSEntryList &list) {
        KFileItemList hits;
        for (const KIO::UDSEntry &e : list) {
            const QString n = e.stringValue(KIO::UDSEntry::UDS_NAME);
            if (n == QLatin1String(".") || n == QLatin1String(".."))
                continue;
            // listRecursive prefixes UDS_NAME with the relative folder ("sub/b.txt"): keep the full URL, give name() the bare name
            QUrl url = m_folder;
            url.setPath(QDir::cleanPath(m_folder.path() + QLatin1Char('/') + n));
            KIO::UDSEntry bare = e;
            bare.replace(KIO::UDSEntry::UDS_NAME, n.section(QLatin1Char('/'), -1));
            KFileItem item(bare, url, true, false);
            if (m_matcher.matches(item))
                hits << item;
        }
        if (!hits.isEmpty())
            Q_EMIT found(hits);
    });
    connect(job, &KJob::result, this, [this](KJob *j) {
        m_job = nullptr;
        Q_EMIT finished(j->error() == 0, j->errorString());
    });
}

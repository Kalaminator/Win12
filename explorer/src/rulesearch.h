#pragma once
#include "rulematcher.h"
#include <KFileItem>
#include <KIO/ListJob>
#include <QObject>
#include <QPointer>
#include <QUrl>

class RuleSearch : public QObject
{
    Q_OBJECT
public:
    explicit RuleSearch(QObject *parent = nullptr);
    void start(const QUrl &folder, bool recursive, const RuleMatcher &); // stops a running search first
    void stop(); // kills the job, no finished()
    bool isRunning() const;

Q_SIGNALS:
    void found(const KFileItemList &);
    void finished(bool ok, const QString &error);

private:
    QPointer<KIO::ListJob> m_job;
    QUrl m_folder;
    RuleMatcher m_matcher = *RuleMatcher::compile({}, nullptr);
};

#pragma once
#include "tag.h"
#include <KFileItem>
#include <QObject>
#include <QUrl>
#include <memory>

// Finds files carrying tags below a folder: Baloo where the folder is indexed, a
// folder walk reading xattrs elsewhere (USB/NTFS drives). Both run off the UI thread.
class TagSearch : public QObject
{
    Q_OBJECT
public:
    enum class Mode { All, Any };
    enum class Engine { Baloo, Walk };
    Q_ENUM(Engine)
    explicit TagSearch(QObject *parent = nullptr);
    ~TagSearch() override;
    static Engine engineFor(const QUrl &folder); // Baloo iff local, indexing on and the path is indexed
    // Exact match of a file's encoded tags against the picked, encoded ones; both engines filter through this
    static bool matches(const QStringList &have, const QStringList &wanted, Mode);
    void start(const QUrl &folder, const QList<Tag> &tags, Mode, Engine); // stops a running search first
    void start(const QUrl &folder, const QList<Tag> &tags, Mode mode) { start(folder, tags, mode, engineFor(folder)); }
    void stop(); // no found()/finished() afterwards
    bool isRunning() const { return bool(m_run); }

Q_SIGNALS:
    void found(const KFileItemList &); // batches of <= 200
    void finished(bool ok, const QString &error, TagSearch::Engine used);

private:
    struct Run;
    std::shared_ptr<Run> m_run; // shared with the worker; the worker never holds `this` past stop()
};

#pragma once
#include "tag.h"
#include <QList>
#include <QObject>
#include <QUrl>

// Reads and writes the user.xdg.tags xattr (KFileMetaData::UserMetaData).
class TagStore : public QObject
{
    Q_OBJECT
public:
    enum class Error { None, NotLocal, Unsupported, TooMany, WriteFailed };
    using QObject::QObject;

    static QList<Tag> read(const QUrl &); // empty for non-local
    static bool supported(const QUrl &);  // local && UserMetaData::isSupported()
    Error add(const QList<QUrl> &, const Tag &);    // stops at the first error, earlier items stay tagged
    Error remove(const QList<QUrl> &, const Tag &);
    Error replace(const QUrl &, const Tag &from, const Tag &to); // Manage tags (rename/recolour)
    static QString errorText(Error);

Q_SIGNALS:
    void tagsChanged(const QList<QUrl> &); // after every successful write, written urls only

private:
    static Error write(const QUrl &, const QList<Tag> &);
};

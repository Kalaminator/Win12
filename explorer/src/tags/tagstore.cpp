#include "tagstore.h"
#include <KFileMetaData/UserMetaData>
#include <KLocalizedString>

using KFileMetaData::UserMetaData;

QList<Tag> TagStore::read(const QUrl &url)
{
    return url.isLocalFile() ? decodeAll(UserMetaData(url.toLocalFile()).tags()) : QList<Tag>();
}

bool TagStore::supported(const QUrl &url)
{
    return url.isLocalFile() && UserMetaData(url.toLocalFile()).isSupported();
}

TagStore::Error TagStore::write(const QUrl &url, const QList<Tag> &tags)
{
    if (!url.isLocalFile())
        return Error::NotLocal;
    UserMetaData md(url.toLocalFile());
    if (!md.isSupported())
        return Error::Unsupported;
    return md.setTags(encodeAll(tags)) == UserMetaData::NoError ? Error::None : Error::WriteFailed;
}

TagStore::Error TagStore::add(const QList<QUrl> &urls, const Tag &tag)
{
    QList<QUrl> done;
    Error err = Error::None;
    for (const QUrl &u : urls) {
        if (!u.isLocalFile()) {
            err = Error::NotLocal;
            break;
        }
        QList<Tag> tags = read(u);
        if (tags.contains(tag))
            continue; // already there: no-op success
        if (!tag.isForeign() && win12TagCount(tags) >= MaxTags) {
            err = Error::TooMany;
            break;
        }
        tags.append(tag);
        if ((err = write(u, tags)) != Error::None)
            break;
        done << u;
    }
    if (!done.isEmpty())
        Q_EMIT tagsChanged(done);
    return err;
}

TagStore::Error TagStore::remove(const QList<QUrl> &urls, const Tag &tag)
{
    QList<QUrl> done;
    Error err = Error::None;
    for (const QUrl &u : urls) {
        QList<Tag> tags = read(u);
        if (!tags.removeAll(tag)) {
            if (!u.isLocalFile())
                err = Error::NotLocal;
            if (err != Error::None)
                break;
            continue;
        }
        if ((err = write(u, tags)) != Error::None)
            break;
        done << u;
    }
    if (!done.isEmpty())
        Q_EMIT tagsChanged(done);
    return err;
}

TagStore::Error TagStore::replace(const QUrl &url, const Tag &from, const Tag &to)
{
    QList<Tag> tags = read(url);
    const int i = tags.indexOf(from);
    if (i < 0)
        return url.isLocalFile() ? Error::None : Error::NotLocal;
    if (tags.contains(to))
        tags.removeAt(i); // target already present: merge
    else
        tags[i] = to;
    const Error err = write(url, tags);
    if (err == Error::None)
        Q_EMIT tagsChanged({url});
    return err;
}

QString TagStore::errorText(Error e)
{
    switch (e) {
    case Error::None:
        return {};
    case Error::NotLocal:
        return i18n("Tags can only be set on local files");
    case Error::Unsupported:
        return i18n("Tags are not supported on this drive");
    case Error::TooMany:
        return i18n("A file can have at most %1 tags", MaxTags);
    case Error::WriteFailed:
        return i18n("The tags could not be saved");
    }
    return {};
}

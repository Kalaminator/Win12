#include "fxfile.h"
#include <QFileInfo>

QList<Tag> FxFile::tags()
{
    if (!m_tags)
        m_tags = loadTags ? loadTags() : QList<Tag>();
    return *m_tags;
}

FxPhoto FxFile::photo()
{
    if (!m_photo)
        m_photo = loadPhoto ? loadPhoto() : FxPhoto();
    return *m_photo;
}

void FxFile::setTags(const QList<Tag> &tags)
{
    m_tags = tags;
}

QString FxFile::fullName() const
{
    return QFileInfo(path).fileName();
}

int FxFile::extDot() const
{
    if (isFolder)
        return -1;
    const QString full = fullName();
    const int dot = full.lastIndexOf(u'.');
    return dot > 0 && dot < full.size() - 1 ? dot : -1;
}

QString FxFile::name() const
{
    const int dot = extDot();
    return dot < 0 ? fullName() : fullName().left(dot);
}

QString FxFile::ext() const
{
    const int dot = extDot();
    return dot < 0 ? QString() : fullName().mid(dot + 1);
}

QString FxFile::folder() const
{
    return QFileInfo(QFileInfo(path).path()).fileName();
}

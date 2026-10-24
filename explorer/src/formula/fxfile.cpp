#include "fxfile.h"
#include "tagstore.h"
#include <KFileItem>
#include <KFileMetaData/Extractor>
#include <KFileMetaData/ExtractorCollection>
#include <KFileMetaData/SimpleExtractionResult>
#include <QCollator>
#include <QFileInfo>
#include <QImageReader>
#include <QMimeDatabase>
#include <algorithm>
#include <numeric>

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

QString fxRelativePath(const QUrl &url, const QUrl &base)
{
    QString b = base.adjusted(QUrl::StripTrailingSlash).path();
    if (!b.endsWith(u'/'))
        b += u'/';
    const QString p = url.path();
    return url.scheme() == base.scheme() && url.host() == base.host() && p.startsWith(b) ? p.mid(b.size()) : url.fileName();
}

QList<int> fxRowOrder(const QList<QStringList> &parts)
{
    QList<int> order(parts.size());
    std::iota(order.begin(), order.end(), 0);
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
    return order;
}

static FxPhoto readPhoto(const QString &path)
{
    FxPhoto photo;
    const QString mime = QMimeDatabase().mimeTypeForFile(path).name();
    if (!mime.startsWith(QLatin1String("image/")))
        return photo;
    // ponytail: one collection per pool thread (plugin lookup is slow), kept until the thread ends
    thread_local KFileMetaData::ExtractorCollection extractors;
    KFileMetaData::SimpleExtractionResult result(path, mime, KFileMetaData::ExtractionResult::ExtractMetaData);
    const QList<KFileMetaData::Extractor *> list = extractors.fetchExtractors(mime);
    if (!list.isEmpty())
        list.first()->extract(&result);
    const auto props = result.properties();
    photo.width = props.value(KFileMetaData::Property::Width).toInt();
    photo.height = props.value(KFileMetaData::Property::Height).toInt();
    photo.taken = props.value(KFileMetaData::Property::PhotoDateTimeOriginal).toDateTime();
    if (photo.width <= 0 || photo.height <= 0) { // no extractor gave a size: read the image header
        const QSize s = QImageReader(path).size();
        photo.width = qMax(0, s.width());
        photo.height = qMax(0, s.height());
    }
    return photo;
}

std::shared_ptr<FxFile> fxFileFor(const KFileItem &item, const QUrl &base, int row, int rowInFolder)
{
    auto f = std::make_shared<FxFile>();
    const QUrl url = item.url();
    f->isFolder = item.isDir();
    f->size = f->isFolder ? 0 : qMax<qint64>(0, item.size());
    f->modified = item.time(KFileItem::ModificationTime);
    f->created = item.time(KFileItem::CreationTime);
    f->depth = fxRelativePath(url, base).count(u'/');
    f->row = row;
    f->rowInFolder = rowInFolder;
    if (url.isLocalFile()) {
        f->path = url.toLocalFile();
        f->loadTags = [url] { return TagStore::read(url); };
        if (!f->isFolder)
            f->loadPhoto = [path = f->path] { return readPhoto(path); };
    } else {
        f->path = url.toDisplayString();
    }
    return f;
}

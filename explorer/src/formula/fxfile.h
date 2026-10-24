#pragma once
#include "tag.h"
#include <QDateTime>
#include <QList>
#include <QString>
#include <functional>
#include <memory>
#include <optional>

class KFileItem;
class QUrl;

struct FxPhoto {
    int width = 0, height = 0;
    QDateTime taken;
};

// One row a formula or script sees. Built by fxFileFor (Task 3); tags and photo are read lazily.
struct FxFile {
    QString path; // local absolute path; scripts update it after a planned Rename/MoveTo
    bool isFolder = false;
    qint64 size = 0;
    QDateTime modified, created;
    int depth = 0;
    int row = 1, rowInFolder = 1;
    std::function<QList<Tag>()> loadTags; // optional, called once, cached
    std::function<FxPhoto()> loadPhoto; // optional, called once, cached

    QList<Tag> tags();
    FxPhoto photo();
    void setTags(const QList<Tag> &); // scripts' .Tag/.Untag

    QString fullName() const;
    QString name() const; // "a.b.txt" -> "a.b"; folders, "a." and ".bashrc" keep the whole name
    QString ext() const; // "txt"; "" for folders, "a." and ".bashrc"
    QString folder() const; // the parent folder's name

private:
    std::optional<QList<Tag>> m_tags;
    std::optional<FxPhoto> m_photo;
    int extDot() const; // index of the name/ext '.', -1 when none
};

// One row for `item` below `base`. Local: tags from TagStore, photo (images only) through
// KFileMetaData, both lazy; non-local: path is the display URL, no tags, no photo.
// depth = '/' count of the path relative to base (0 for a direct child).
std::shared_ptr<FxFile> fxFileFor(const KFileItem &item, const QUrl &base, int row, int rowInFolder);
// item's path relative to base ("sub/b.txt"); the file name when it is not below base
QString fxRelativePath(const QUrl &url, const QUrl &base);

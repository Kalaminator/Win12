#pragma once
#include "tag.h"
#include <QDateTime>
#include <QObject>

// The known tags (tags.json): what the tag pickers offer, most recently used first.
class TagRegistry : public QObject
{
    Q_OBJECT
public:
    explicit TagRegistry(QObject *parent = nullptr); // defaultPath()
    explicit TagRegistry(const QString &file, QObject *parent = nullptr);
    static QString defaultPath();

    QList<Tag> tags() const; // most recently used first, Win12 tags only
    void touch(const Tag &); // insert or move to front, save
    void rename(const Tag &from, const QString &word); // registry only
    void recolour(const Tag &from, int colour);
    void remove(const Tag &);
    bool contains(const Tag &) const;

Q_SIGNALS:
    void changed();

private:
    struct Entry {
        Tag tag;
        QDateTime used;
    };
    int indexOf(const Tag &) const;
    void load();
    void save();
    QString m_file;
    QList<Entry> m_entries;
};

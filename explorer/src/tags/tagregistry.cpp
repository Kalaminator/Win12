#include "tagregistry.h"
using namespace Qt::StringLiterals;
#include <KLocalizedString>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

TagRegistry::TagRegistry(QObject *parent)
    : TagRegistry(defaultPath(), parent)
{
}

TagRegistry::TagRegistry(const QString &file, QObject *parent)
    : QObject(parent)
    , m_file(file)
{
    load();
}

QString TagRegistry::defaultPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/win12/tags.json");
}

QList<Tag> TagRegistry::tags() const
{
    QList<Tag> out;
    for (const Entry &e : m_entries)
        out << e.tag;
    return out;
}

int TagRegistry::indexOf(const Tag &t) const
{
    for (int i = 0; i < m_entries.size(); ++i)
        if (m_entries[i].tag == t)
            return i;
    return -1;
}

bool TagRegistry::contains(const Tag &t) const
{
    return indexOf(t) >= 0;
}

bool TagRegistry::touch(const Tag &t)
{
    if (t.isForeign())
        return true;
    const int i = indexOf(t);
    if (i >= 0)
        m_entries.removeAt(i);
    m_entries.prepend({t, QDateTime::currentDateTimeUtc()});
    return save();
}

// the edited entry i now equals another one: merge by dropping i
void TagRegistry::dropIfDuplicate(int i)
{
    const Tag t = m_entries[i].tag;
    for (int j = 0; j < m_entries.size(); ++j)
        if (j != i && m_entries[j].tag == t) {
            m_entries.removeAt(i);
            return;
        }
}

bool TagRegistry::rename(const Tag &from, const QString &word)
{
    const int i = indexOf(from);
    if (i < 0)
        return true;
    m_entries[i].tag.word = word;
    dropIfDuplicate(i);
    return save();
}

bool TagRegistry::recolour(const Tag &from, int colour)
{
    const int i = indexOf(from);
    if (i < 0 || colour < 0 || colour >= TagColours::Count)
        return true;
    m_entries[i].tag.colour = colour;
    dropIfDuplicate(i);
    return save();
}

bool TagRegistry::remove(const Tag &t)
{
    const int i = indexOf(t);
    if (i < 0)
        return true;
    m_entries.removeAt(i);
    return save();
}

void TagRegistry::load()
{
    QFile f(m_file);
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QJsonArray arr = QJsonDocument::fromJson(f.readAll()).object().value(u"tags").toArray();
    for (const QJsonValue &v : arr) {
        const QJsonObject o = v.toObject();
        const int c = TagColours::indexOfName(o.value(u"colour").toString());
        if (c < 0)
            continue;
        m_entries.append({Tag{c, o.value(u"word").toString()}, QDateTime::fromString(o.value(u"used").toString(), Qt::ISODate)});
    }
}

// ponytail: whole file rewritten on each change; the list is tens of entries
bool TagRegistry::save()
{
    QJsonArray arr;
    for (const Entry &e : m_entries)
        arr.append(QJsonObject{{u"colour"_s, TagColours::name(e.tag.colour)},
                               {u"word"_s, e.tag.word},
                               {u"used"_s, e.used.toString(Qt::ISODate)}});
    QDir().mkpath(QFileInfo(m_file).absolutePath());
    QSaveFile f(m_file);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(QJsonDocument(QJsonObject{{u"version"_s, 1}, {u"tags"_s, arr}}).toJson());
    if (!f.commit())
        return false;
    Q_EMIT changed();
    return true;
}

QString TagRegistry::saveErrorText() const
{
    return i18n("The tag list could not be saved to %1.", m_file);
}

#include "rulelibrary.h"
#include <KLocalizedString>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <algorithm>

using namespace Qt::StringLiterals;

static QString kindName(SavedRule::Kind k)
{
    switch (k) {
    case SavedRule::Kind::Select:
        return u"select"_s;
    case SavedRule::Kind::Rename:
        return u"rename"_s;
    case SavedRule::Kind::Script:
        return u"script"_s;
    }
    return {};
}

QString RuleLibrary::path()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + u"/win12/rules.json"_s;
}

void RuleLibrary::ensureLoaded() const
{
    if (m_loaded)
        return;
    m_loaded = true;
    m_rules.clear();
    QFile f(path());
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QJsonArray arr = QJsonDocument::fromJson(f.readAll()).object().value(u"rules"_s).toArray();
    for (const QJsonValue &v : arr) {
        const QJsonObject o = v.toObject();
        const QString k = o.value(u"kind"_s).toString();
        const QString name = o.value(u"name"_s).toString();
        if (name.isEmpty())
            continue;
        for (SavedRule::Kind kind : {SavedRule::Kind::Select, SavedRule::Kind::Rename, SavedRule::Kind::Script})
            if (kindName(kind) == k)
                m_rules.append({name, o.value(u"text"_s).toString(), kind});
    }
    std::sort(m_rules.begin(), m_rules.end(), [](const SavedRule &a, const SavedRule &b) {
        return a.kind != b.kind ? a.kind < b.kind : a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });
}

QList<SavedRule> RuleLibrary::all() const
{
    ensureLoaded();
    return m_rules;
}

QList<SavedRule> RuleLibrary::ofKind(SavedRule::Kind k) const
{
    QList<SavedRule> out;
    for (const SavedRule &r : all())
        if (r.kind == k)
            out << r;
    return out;
}

void RuleLibrary::reload()
{
    m_loaded = false;
    ensureLoaded();
    Q_EMIT changed();
}

// Keeps m_rules sorted; the list is tens of entries, so the whole file is rewritten each time.
bool RuleLibrary::write(QString *error)
{
    std::sort(m_rules.begin(), m_rules.end(), [](const SavedRule &a, const SavedRule &b) {
        return a.kind != b.kind ? a.kind < b.kind : a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });
    QJsonArray arr;
    for (const SavedRule &r : std::as_const(m_rules))
        arr.append(QJsonObject{{u"name"_s, r.name}, {u"kind"_s, kindName(r.kind)}, {u"text"_s, r.text}});
    QDir().mkpath(QFileInfo(path()).absolutePath());
    QSaveFile f(path());
    if (!f.open(QIODevice::WriteOnly) || f.write(QJsonDocument(QJsonObject{{u"version"_s, 1}, {u"rules"_s, arr}}).toJson()) < 0 || !f.commit()) {
        if (error)
            *error = i18n("The saved rules could not be written to %1: %2", path(), f.errorString());
        return false;
    }
    Q_EMIT changed();
    return true;
}

bool RuleLibrary::save(const SavedRule &rule, QString *error)
{
    if (rule.name.trimmed().isEmpty()) {
        if (error)
            *error = i18n("A name is needed");
        return false;
    }
    ensureLoaded();
    const QList<SavedRule> before = m_rules;
    m_rules.removeIf([&](const SavedRule &r) { return r.kind == rule.kind && r.name == rule.name; });
    m_rules.append(rule);
    if (write(error))
        return true;
    m_rules = before; // not written: the list stays as it was on disk
    return false;
}

bool RuleLibrary::remove(const QString &name, SavedRule::Kind kind, QString *error)
{
    ensureLoaded();
    const QList<SavedRule> before = m_rules;
    if (!m_rules.removeIf([&](const SavedRule &r) { return r.kind == kind && r.name == name; }))
        return true;
    if (write(error))
        return true;
    m_rules = before;
    return false;
}

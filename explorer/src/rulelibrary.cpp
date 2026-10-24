#include "rulelibrary.h"
#include <KLocalizedString>
#include <KMessageBox>
#include <KStandardGuiItem>
#include <QInputDialog>
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

static void sortRules(QList<SavedRule> &rules)
{
    std::sort(rules.begin(), rules.end(), [](const SavedRule &a, const SavedRule &b) {
        return a.kind != b.kind ? a.kind < b.kind : a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });
}

QString RuleLibrary::path()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + u"/win12/rules.json"_s;
}

static void writeServiceMenu(const RuleLibrary &l, bool empty);

void RuleLibrary::ensureLoaded() const
{
    if (m_loaded)
        return;
    m_loaded = true;
    m_rules.clear();
    QFile f(path());
    if (!QFileInfo(f).isFile())
        return; // missing, or not a file (save() reports that)
    QJsonParseError pe;
    QJsonDocument doc;
    if (f.open(QIODevice::ReadOnly))
        doc = QJsonDocument::fromJson(f.readAll(), &pe);
    if (!doc.isObject()) {
        // keep what the user had: the next save() must not overwrite it
        f.close();
        QFile::remove(path() + u".broken"_s);
        f.rename(path() + u".broken"_s);
        return;
    }
    const QJsonArray arr = doc.object().value(u"rules"_s).toArray();
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
    sortRules(m_rules);
    // an existing library gets the current menu format without a save
    QFile mf(serviceMenuPath());
    const QString cur = mf.open(QIODevice::ReadOnly) ? QString::fromUtf8(mf.readAll()) : QString();
    if (cur != serviceMenuText())
        writeServiceMenu(*this, m_rules.isEmpty());
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

QString RuleLibrary::serviceMenuPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + u"/kio/servicemenus/win12-my-rules.desktop"_s;
}

// Exec argument: the desktop-entry spec unescapes the value first (\\ -> \), then applies quoting, so inside the double
// quotes " ` $ \ are backslash-escaped and % is %%; the file text then doubles every backslash.
static QString execQuoted(QString s)
{
    s.remove(u'\n').remove(u'\r');
    QString q;
    for (const QChar c : std::as_const(s)) {
        if (c == u'"' || c == u'`' || c == u'$' || c == u'\\')
            q += u'\\';
        if (c == u'%')
            q += u'%';
        q += c;
    }
    q.replace(u'\\', u"\\\\"_s);
    return u'"' + q + u'"';
}

QString RuleLibrary::serviceMenuText() const
{
    ensureLoaded();
    QString actions;
    QString body;
    for (qsizetype i = 0; i < m_rules.size(); ++i) {
        const SavedRule &r = m_rules[i];
        actions += u"rule%1;"_s.arg(i, 3, 10, QLatin1Char('0'));
        QString name = r.name;
        name.remove(u'\n').remove(u'\r');
        name.replace(u'\\', u"\\\\"_s);
        body += u"[Desktop Action rule%1]\nName=%2\nIcon=%3\nExec=win12-explorer --rule %4 %U\n"_s.arg(i, 3, 10, QLatin1Char('0')).arg(
            name, r.kind == SavedRule::Kind::Script ? u"text-x-script"_s : u"accessories-calculator"_s, execQuoted(r.name));
    }
    return u"[Desktop Entry]\nType=Service\nMimeType=inode/directory;all/allfiles;\nX-KDE-Submenu=%2\nActions=%1\n"_s.arg(actions, QString(RuleLibrary::kSubmenu)) + body;
}

// Best effort: a failed menu file never fails a save. KF6 ignores non-executable local service menus, hence 0755.
static void writeServiceMenu(const RuleLibrary &l, bool empty)
{
    const QString p = RuleLibrary::serviceMenuPath();
    if (empty) {
        QFile::remove(p);
        return;
    }
    QDir().mkpath(QFileInfo(p).absolutePath());
    QSaveFile f(p);
    if (f.open(QIODevice::WriteOnly) && f.write(l.serviceMenuText().toUtf8()) >= 0 && f.commit())
        QFile::setPermissions(p, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner | QFileDevice::ReadGroup
                                  | QFileDevice::ExeGroup | QFileDevice::ReadOther | QFileDevice::ExeOther);
}

// Keeps m_rules sorted; the list is tens of entries, so the whole file is rewritten each time.
bool RuleLibrary::write(QString *error)
{
    sortRules(m_rules);
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
    writeServiceMenu(*this, m_rules.isEmpty());
    Q_EMIT changed();
    return true;
}

std::optional<SavedRule> RuleLibrary::find(const QString &name) const
{
    ensureLoaded();
    for (const SavedRule &r : m_rules)
        if (r.name == name)
            return r;
    return std::nullopt;
}

bool RuleLibrary::save(const SavedRule &rule, QString *error)
{
    if (rule.name.trimmed().isEmpty()) {
        if (error)
            *error = i18n("A name is needed");
        return false;
    }
    ensureLoaded();
    if (const auto other = find(rule.name); other && other->kind != rule.kind) {
        if (error) {
            const QString kt = other->kind == SavedRule::Kind::Select ? i18n("Select") : other->kind == SavedRule::Kind::Rename ? i18n("Rename") : i18n("Script");
            *error = i18n("A %1 rule named %2 already exists; choose another name", kt, rule.name);
        }
        return false;
    }
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

bool saveRuleInteractively(RuleLibrary &lib, SavedRule::Kind kind, const QString &text, QWidget *parent)
{
    bool ok = false;
    const QString name = QInputDialog::getText(parent, i18n("Save as"), i18n("Name"), QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok || name.isEmpty())
        return false;
    const auto old = lib.find(name);
    if (old && old->kind == kind
        && KMessageBox::questionTwoActions(parent, i18n("Replace the saved rule %1?", name), i18n("Save as"), KStandardGuiItem::overwrite(), KStandardGuiItem::cancel())
            != KMessageBox::PrimaryAction)
        return false;
    QString err;
    if (lib.save({name, text, kind}, &err))
        return true;
    KMessageBox::error(parent, err);
    return false;
}

#include "rulematcher.h"
#include "tagstore.h"
#include <KLocalizedString>
#include <QFileInfo>
#include <QRegularExpression>

QList<Test> testsFor(Field f)
{
    switch (f) {
    case Field::Name:
    case Field::Extension:
        return {Test::StartsWith, Test::Contains, Test::EndsWith, Test::Equals, Test::Regex};
    case Field::Size:
        return {Test::BiggerThan, Test::SmallerThan};
    case Field::Modified:
    case Field::Created:
        return {Test::OlderThan, Test::NewerThan};
    case Field::Kind:
        return {Test::Equals};
    case Field::Tag:
        return {Test::HasTag};
    }
    return {};
}

QString fieldText(Field f)
{
    switch (f) {
    case Field::Name: return i18n("name");
    case Field::Extension: return i18n("extension");
    case Field::Size: return i18n("size");
    case Field::Modified: return i18n("modified");
    case Field::Created: return i18n("created");
    case Field::Kind: return i18n("kind");
    case Field::Tag: return i18n("Tag");
    }
    return {};
}

QString testText(Test t)
{
    switch (t) {
    case Test::StartsWith: return i18n("starts with");
    case Test::Contains: return i18n("contains");
    case Test::EndsWith: return i18n("ends with");
    case Test::Equals: return i18n("is");
    case Test::Regex: return i18n("matches regex");
    case Test::OlderThan: return i18n("older than");
    case Test::NewerThan: return i18n("newer than");
    case Test::BiggerThan: return i18n("bigger than");
    case Test::SmallerThan: return i18n("smaller than");
    case Test::HasTag: return i18n("has tag");
    }
    return {};
}

qint64 parseSize(const QString &text, bool *ok)
{
    static const QRegularExpression re(QStringLiteral("^(\\d+)\\s*(B|KB|MB|GB|TB)?$"), QRegularExpression::CaseInsensitiveOption);
    const auto m = re.match(text.trimmed());
    bool good = m.hasMatch();
    qint64 n = 0;
    if (good) {
        const int shift = QStringLiteral("B KB MB GB TB").split(QLatin1Char(' ')).indexOf(m.captured(2).toUpper().isEmpty() ? QStringLiteral("B") : m.captured(2).toUpper());
        n = m.captured(1).toLongLong(&good) << (10 * shift);
    }
    if (ok)
        *ok = good;
    return good ? n : 0;
}

QDateTime parseDateValue(const QString &text, const QDateTime &now, bool *ok)
{
    if (ok)
        *ok = true;
    static const QRegularExpression re(QStringLiteral("^(\\d+)\\s*(day|week|month|year)s?$"), QRegularExpression::CaseInsensitiveOption);
    const QString t = text.trimmed();
    if (const auto m = re.match(t); m.hasMatch()) {
        const int n = m.captured(1).toInt();
        const QString u = m.captured(2).toLower();
        return u == QLatin1String("day") ? now.addDays(-n) : u == QLatin1String("week") ? now.addDays(-7 * n)
            : u == QLatin1String("month") ? now.addMonths(-n) : now.addYears(-n);
    }
    const QDate d = QDate::fromString(t, Qt::ISODate);
    if (d.isValid())
        return QDateTime(d, QTime(0, 0));
    if (ok)
        *ok = false;
    return {};
}

std::optional<RuleMatcher> RuleMatcher::compile(const QList<Rule> &rules, QString *error, const QDateTime &now)
{
    RuleMatcher rm;
    auto fail = [&](const QString &e) {
        if (error)
            *error = e;
        return std::optional<RuleMatcher>();
    };
    for (const Rule &r : rules) {
        const QString v = r.value;
        std::function<bool(const KFileItem &)> fn;
        if (r.field == Field::Kind) {
            const bool wantDir = v.trimmed().compare(QLatin1String("folder"), Qt::CaseInsensitive) == 0;
            if (!wantDir && v.trimmed().compare(QLatin1String("file"), Qt::CaseInsensitive) != 0)
                return fail(i18n("Kind must be \"folder\" or \"file\""));
            fn = [wantDir](const KFileItem &i) { return i.isDir() == wantDir; };
        } else if (r.field == Field::Tag) {
            if (v.isEmpty())
                return fail(i18n("Enter a value for the %1 rule", fieldText(r.field)));
            fn = [v](const KFileItem &i) {
                for (const Tag &t : TagStore::read(i.url()))
                    if (t.display().compare(v, Qt::CaseInsensitive) == 0 || t.encode() == v)
                        return true;
                return false;
            };
        } else if (r.field == Field::Size) {
            bool ok;
            const qint64 n = parseSize(v, &ok);
            if (!ok)
                return fail(i18n("Not a size: %1 (try 10 MB, 500 KB or 4096)", v));
            const bool bigger = r.test == Test::BiggerThan;
            fn = [n, bigger](const KFileItem &i) { return !i.isDir() && (bigger ? i.size() > n : i.size() < n); };
        } else if (r.field == Field::Modified || r.field == Field::Created) {
            bool ok;
            const QDateTime when = parseDateValue(v, now, &ok);
            if (!ok)
                return fail(i18n("Not a date: %1 (try 30 days, 6 months or 2026-01-01)", v));
            const auto which = r.field == Field::Modified ? KFileItem::ModificationTime : KFileItem::CreationTime;
            const bool older = r.test == Test::OlderThan;
            fn = [when, which, older](const KFileItem &i) {
                const QDateTime t = i.time(which);
                return t.isValid() && (older ? t < when : t > when);
            };
        } else { // Name, Extension
            if (v.isEmpty())
                return fail(i18n("Enter a value for the %1 rule", fieldText(r.field)));
            const bool ext = r.field == Field::Extension;
            auto text = [ext](const KFileItem &i) { return ext ? (i.isDir() ? QString() : QFileInfo(i.name()).suffix()) : i.name(); };
            const auto cs = Qt::CaseInsensitive;
            switch (r.test) {
            case Test::StartsWith: fn = [=](const KFileItem &i) { return text(i).startsWith(v, cs); }; break;
            case Test::Contains: fn = [=](const KFileItem &i) { return text(i).contains(v, cs); }; break;
            case Test::EndsWith: fn = [=](const KFileItem &i) { return text(i).endsWith(v, cs); }; break;
            case Test::Equals: fn = [=](const KFileItem &i) { return text(i).compare(v, cs) == 0; }; break;
            case Test::Regex: {
                const QRegularExpression re(v, QRegularExpression::CaseInsensitiveOption);
                if (!re.isValid())
                    return fail(re.errorString());
                fn = [=](const KFileItem &i) { return re.match(text(i)).hasMatch(); };
                break;
            }
            default:
                return fail(i18n("The test \"%1\" does not apply to %2", testText(r.test), fieldText(r.field)));
            }
        }
        rm.m_tests << fn;
    }
    return rm;
}

bool RuleMatcher::matches(const KFileItem &item) const
{
    for (const auto &t : m_tests)
        if (!t(item))
            return false;
    return true;
}

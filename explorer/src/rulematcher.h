#pragma once
#include <KFileItem>
#include <QDateTime>
#include <QList>
#include <QString>
#include <functional>
#include <optional>

enum class Field { Name, Extension, Size, Modified, Created, Kind }; // phase 2 adds Tag
enum class Test { StartsWith, Contains, EndsWith, Equals, Regex, OlderThan, NewerThan, BiggerThan, SmallerThan }; // phase 2 adds HasTag

struct Rule {
    Field field;
    Test test;
    QString value;
};

QList<Test> testsFor(Field);
QString fieldText(Field);
QString testText(Test);
qint64 parseSize(const QString &, bool *ok);
QDateTime parseDateValue(const QString &, const QDateTime &now, bool *ok);

class RuleMatcher
{
public:
    static std::optional<RuleMatcher> compile(const QList<Rule> &, QString *error, const QDateTime &now = QDateTime::currentDateTime());
    bool matches(const KFileItem &) const; // AND over all rules; empty list matches everything

private:
    QList<std::function<bool(const KFileItem &)>> m_tests;
};

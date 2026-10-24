#pragma once
#include <QList>
#include <QObject>
#include <QString>

// A saved formula (Select / Rename) or script, kept in win12/rules.json.
struct SavedRule {
    enum class Kind { Select, Rename, Script };
    QString name;
    QString text;
    Kind kind = Kind::Select;
    bool operator==(const SavedRule &) const = default;
};

class RuleLibrary : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    static QString path(); // GenericDataLocation/win12/rules.json (next to tags.json)
    QList<SavedRule> all() const; // sorted by kind then name (case-insensitive); loaded lazily
    QList<SavedRule> ofKind(SavedRule::Kind) const;
    bool save(const SavedRule &, QString *error); // replaces same name+kind
    bool remove(const QString &name, SavedRule::Kind, QString *error);
    void reload();

Q_SIGNALS:
    void changed();

private:
    void ensureLoaded() const;
    bool write(QString *error); // the one write path
    mutable QList<SavedRule> m_rules;
    mutable bool m_loaded = false;
};

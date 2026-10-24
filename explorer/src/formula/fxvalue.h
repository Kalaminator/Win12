#pragma once
#include <QDateTime>
#include <QString>
#include <memory>

struct FxFile;
enum class FxError { Value, Name, Div0, NA, Num }; // "#VALUE!" "#NAME?" "#DIV/0!" "#N/A" "#NUM!"

struct FxValue {
    enum class Type { Empty, Bool, Number, Text, Date, File, Error };
    Type type = Type::Empty;
    bool b = false;
    double n = 0;
    QString s;
    QDateTime d;
    std::shared_ptr<FxFile> file;
    FxError err = FxError::Value;

    static FxValue empty();
    static FxValue boolean(bool);
    static FxValue number(double);
    static FxValue text(const QString &);
    static FxValue date(const QDateTime &);
    static FxValue fileRef(std::shared_ptr<FxFile>);
    static FxValue error(FxError);

    bool isError() const;
    bool truthy(bool *ok) const; // Bool as is; Number != 0; anything else *ok = false
    QString toText() const; // '&' and TEXT(): Empty "", TRUE/FALSE, "12" / "1.5", Date "yyyy-MM-dd hh:mm", File full name, errors "#VALUE!"...
    QString display() const; // toText(), but File -> path
    bool operator==(const FxValue &) const; // exact (type and payload); tests only
};

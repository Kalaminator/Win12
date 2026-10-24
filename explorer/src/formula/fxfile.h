#pragma once
#include <QFileInfo>
#include <QString>

// ponytail: minimal row so FxValue can print File values; Task 2 grows it (sizes, dates, tags, photo).
struct FxFile {
    QString path; // local absolute path
    QString fullName() const
    {
        return QFileInfo(path).fileName();
    }
};

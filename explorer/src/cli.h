#pragma once
#include <QList>
#include <QStringList>
#include <QUrl>

struct CliOptions {
    bool daemon = false;
    bool select = false;
    bool newWindow = false;
    bool selectByRule = false; // --select-by-rule <folder>: open it and the Select by rule dialog
    bool renameMany = false; // --rename-many <files>: open their folder and the Rename many dialog
    QString rule; // --rule <name>: run a saved rule on urls
    QList<QUrl> urls;
};

// args without argv[0]; relative paths resolve against cwd; unknown "--x" flags are ignored.
CliOptions parseCli(const QStringList &args, const QString &cwd);

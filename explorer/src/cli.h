#pragma once
#include <QList>
#include <QStringList>
#include <QUrl>

struct CliOptions {
    bool daemon = false;
    bool select = false;
    bool newWindow = false;
    QList<QUrl> urls;
};

// args without argv[0]; relative paths resolve against cwd; unknown "--x" flags are ignored.
CliOptions parseCli(const QStringList &args, const QString &cwd);

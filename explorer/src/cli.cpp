#include "cli.h"

CliOptions parseCli(const QStringList &args, const QString &cwd)
{
    CliOptions o;
    for (const QString &a : args) {
        if (a == QLatin1String("--daemon"))
            o.daemon = true;
        else if (a == QLatin1String("--select"))
            o.select = true;
        else if (a == QLatin1String("--new-window"))
            o.newWindow = true;
        else if (!a.startsWith(QLatin1String("--")))
            o.urls << QUrl::fromUserInput(a, cwd, QUrl::AssumeLocalFile);
    }
    return o;
}

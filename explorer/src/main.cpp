#include "explorerwindow.h"
#include <KConfig>
#include <KDBusService>
#include <KLocalizedString>
#include <QApplication>
#include <QDir>

// ponytail: raw positional urls plus --select until Task 10's parseCli replaces this.
static void openWindow(const QStringList &args, const QString &cwd)
{
    QList<QUrl> urls;
    bool select = false;
    for (const QString &arg : args.mid(1)) {
        if (arg == QLatin1String("--select"))
            select = true;
        else if (!arg.startsWith(QLatin1Char('-')))
            urls << QUrl::fromUserInput(arg, cwd, QUrl::AssumeLocalFile);
    }
    if (select && !urls.isEmpty()) {
        auto *w = new ExplorerWindow({urls.first().adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash)});
        w->show();
        w->selectItems(urls);
        return;
    }
    (new ExplorerWindow(urls))->show(); // KMainWindow deletes itself on close; no urls = the saved session
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    KLocalizedString::setApplicationDomain("win12-explorer");
    QCoreApplication::setOrganizationDomain(QStringLiteral("win12.org"));
    QCoreApplication::setApplicationName(QStringLiteral("explorer"));
    // The D-Bus name stays org.win12.explorer (organization domain + "explorer"); every KSharedConfig::openConfig(),
    // KMainWindow's autosave included, reads win12-explorerrc.
    KConfig::setMainConfigName(QStringLiteral("win12-explorerrc"));
    QGuiApplication::setApplicationDisplayName(i18n("Win12 Explorer")); // else titles end in "— explorer"
    QGuiApplication::setDesktopFileName(QStringLiteral("org.win12.explorer"));
    KDBusService service(KDBusService::Unique);
    QObject::connect(&service, &KDBusService::activateRequested, &app, openWindow);
    openWindow(QCoreApplication::arguments(), QDir::currentPath());
    return app.exec();
}

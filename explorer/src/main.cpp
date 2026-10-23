#include "cli.h"
#include "explorerwindow.h"
#include "filemanager1.h"
#include <QDBusConnection>
#include <KConfig>
#include <KDBusService>
#include <KLocalizedString>
#include <QApplication>
#include <QDir>

static ExplorerWindow *s_last = nullptr; // last opened window, cleared on destroy

static void openWindow(const QStringList &args, const QString &cwd)
{
    const CliOptions o = parseCli(args, cwd);
    ExplorerWindow *w = nullptr;
    if (o.select && !o.urls.isEmpty()) {
        if (s_last && !o.newWindow) {
            w = s_last;
        } else {
            w = new ExplorerWindow({o.urls.first().adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash)});
            w->show();
        }
        w->selectItems(o.urls);
    } else if (o.daemon && o.urls.isEmpty()) {
        return; // no window
    } else {
        w = new ExplorerWindow(o.urls); // no urls = the saved session
        w->show();
    }
    if (w != s_last) {
        s_last = w;
        QObject::connect(w, &QObject::destroyed, [] { s_last = nullptr; });
    }
    w->raise();
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
    const CliOptions opts = parseCli(app.arguments().mid(1), QDir::currentPath());
    // One object per D-Bus path, each exporting its adaptor.
    auto *obj = new QObject(&app);
    new FileManager1(obj);
    QDBusConnection::sessionBus().registerObject(QStringLiteral("/org/freedesktop/FileManager1"), obj, QDBusConnection::ExportAdaptors);
    // KDBusService already owns /org/win12/explorer (org.freedesktop.Application); a second registerObject there
    // fails, so the script's ReleaseFileManager1 adaptor rides on the service object instead.
    new ExplorerDBus(&service);
    const bool holdsName = (opts.daemon || FileManager1::wantsRegistration()) && FileManager1::registerName();
    app.setQuitOnLastWindowClosed(!(opts.daemon || holdsName));
    QObject::connect(&service, &KDBusService::activateRequested, &app,
                     [&app](const QStringList &args, const QString &cwd) {
                         // An instance started before the switch went on takes FileManager1 over when asked
                         // (D-Bus activation of the service file forwards its --daemon request here).
                         if ((parseCli(args.mid(1), cwd).daemon || FileManager1::wantsRegistration()) && FileManager1::registerName())
                             app.setQuitOnLastWindowClosed(false);
                         openWindow(args.mid(1), cwd);
                     });
    openWindow(app.arguments().mid(1), QDir::currentPath());
    return app.exec();
}

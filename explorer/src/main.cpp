#include "explorerwindow.h"
#include <KDBusService>
#include <KLocalizedString>
#include <QApplication>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    KLocalizedString::setApplicationDomain("win12-explorer");
    QCoreApplication::setOrganizationDomain(QStringLiteral("win12.org"));
    QCoreApplication::setApplicationName(QStringLiteral("explorer"));
    QGuiApplication::setDesktopFileName(QStringLiteral("org.win12.explorer"));
    KDBusService service(KDBusService::Unique);
    ExplorerWindow w;
    w.show();
    return app.exec();
}

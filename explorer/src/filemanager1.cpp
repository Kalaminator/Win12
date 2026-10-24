#include "filemanager1.h"
#include "explorerwindow.h"
#include <KIO/Global>
#include <QApplication>
#include <QDBusConnection>
#include <QFile>
#include <QStandardPaths>

static const QString kName = QStringLiteral("org.freedesktop.FileManager1");

FileManager1::FileManager1(QObject *parent) : QDBusAbstractAdaptor(parent) {}

QMap<QUrl, QList<QUrl>> FileManager1::foldersFor(const QStringList &uris)
{
    QMap<QUrl, QList<QUrl>> out;
    for (const QString &s : uris) {
        const QUrl u = QUrl::fromUserInput(s);
        if (s.endsWith(QLatin1Char('/')))
            out[u]; // a folder itself
        else
            out[KIO::upUrl(u).adjusted(QUrl::StripTrailingSlash)] << u;
    }
    return out;
}

bool FileManager1::wantsRegistration()
{
    return QFile::exists(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + QStringLiteral("/win12-explorer/saved-default"));
}

bool FileManager1::registerName()
{
    return QDBusConnection::sessionBus().registerService(kName);
}

void FileManager1::releaseName()
{
    QDBusConnection::sessionBus().unregisterService(kName);
}

static void openFolders(const QStringList &uris)
{
    const QList<QUrl> folders = FileManager1::foldersFor(uris).keys();
    for (const QUrl &folder : folders)
        (new ExplorerWindow(folder))->show();
    if (folders.isEmpty())
        (new ExplorerWindow)->show();
}

void FileManager1::ShowFolders(const QStringList &uris, const QString &)
{
    openFolders(uris);
}

void FileManager1::ShowItems(const QStringList &uris, const QString &)
{
    const auto m = foldersFor(uris);
    for (auto it = m.cbegin(); it != m.cend(); ++it) {
        auto *w = new ExplorerWindow(it.key());
        w->show();
        w->selectItems(it.value());
    }
}

void FileManager1::ShowItemProperties(const QStringList &uris, const QString &)
{
    QList<QUrl> urls;
    for (const QString &s : uris)
        urls << QUrl::fromUserInput(s);
    auto *w = new ExplorerWindow(KIO::upUrl(urls.value(0)));
    w->show();
    w->showProperties(urls);
}

void ExplorerDBus::ReleaseFileManager1()
{
    FileManager1::releaseName();
    qApp->setQuitOnLastWindowClosed(true);
    const auto widgets = QApplication::topLevelWidgets();
    if (std::none_of(widgets.begin(), widgets.end(), [](QWidget *w) { return w->isVisible(); }))
        QMetaObject::invokeMethod(qApp, &QCoreApplication::quit, Qt::QueuedConnection); // `off` leaves no hidden daemon
}

#pragma once
#include <QDBusAbstractAdaptor>
#include <QMap>
#include <QStringList>
#include <QUrl>

class FileManager1 : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.FileManager1")
public:
    explicit FileManager1(QObject *parent);
    static QMap<QUrl, QList<QUrl>> foldersFor(const QStringList &uris); // parent folder -> items; "dir/" maps to itself, no items
    static bool wantsRegistration(); // <ConfigLocation>/win12-explorer/saved-default exists
    static bool registerName();
    static void releaseName();
public Q_SLOTS:
    void ShowFolders(const QStringList &uris, const QString &startupId);
    void ShowItems(const QStringList &uris, const QString &startupId);
    void ShowItemProperties(const QStringList &uris, const QString &startupId);
};

class ExplorerDBus : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.win12.explorer")
public:
    explicit ExplorerDBus(QObject *parent) : QDBusAbstractAdaptor(parent) {}
public Q_SLOTS:
    void ReleaseFileManager1(); // the script's `off`
};

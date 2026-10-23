#include "explorerwindow.h"
#include <KLocalizedString>

ExplorerWindow::ExplorerWindow(const QList<QUrl> &urls, QWidget *parent)
    : KMainWindow(parent)
{
    Q_UNUSED(urls)
    setWindowTitle(i18n("Win12 Explorer"));
    setAutoSaveSettings();
}

#pragma once
#include <KMainWindow>
#include <QList>
#include <QUrl>

class ExplorerWindow : public KMainWindow
{
    Q_OBJECT
public:
    explicit ExplorerWindow(const QList<QUrl> &urls = {}, QWidget *parent = nullptr);
};

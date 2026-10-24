#pragma once
#include "tag.h"
#include "tagsearch.h"
#include <QWidget>

class QListWidget;
class QToolButton;
class TagRegistry;

// Nav pane list of the registry's tags: click filters, Ctrl+click picks more, all/any switch.
class TagList : public QWidget
{
    Q_OBJECT
public:
    explicit TagList(TagRegistry *, QWidget *parent = nullptr);
    QList<Tag> picked() const;
    TagSearch::Mode mode() const;
    void clearPicks(bool silent = false); // silent: no filterChanged (tab switch)

Q_SIGNALS:
    void filterChanged(const QList<Tag> &picked, TagSearch::Mode); // empty picked = cleared

private:
    void reload();
    TagRegistry *m_registry;
    QListWidget *m_list;
    QToolButton *m_all;
    QToolButton *m_any;
};

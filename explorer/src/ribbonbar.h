#pragma once
#include <QHash>
#include <QList>
#include <QStringList>
#include <QWidget>

class ExplorerActions;
class QAction;
class QMenu;
class QStackedWidget;
class QToolButton;
class RibbonTabStrip;

// The Windows-style ribbon: tab strip (blue File tab + Home/Share/View/Advanced) over a body built from
// Ribbon::spec(). Every colour comes from the palette, so the light scheme is the twin of the dark one.
class RibbonBar : public QWidget
{
    Q_OBJECT
public:
    explicit RibbonBar(ExplorerActions *actions, QWidget *parent = nullptr);
    bool isCollapsed() const { return m_collapsed; }
    void setCollapsed(bool); // body hidden, tab strip stays; clicking a tab while collapsed expands again
    QAction *collapseAction() const { return m_collapse; }
    QMenu *fileMenu() const { return m_fileMenu; }
    int currentTab() const { return m_current; }
    void setCurrentTab(int); // index into the non-File tabs: 0 Home, 1 Share, 2 View, 3 Advanced
    QList<QToolButton *> buttonsFor(const QString &actionId) const { return m_buttons.values(actionId); }
    QStringList visibleGroupTitles(int tab) const { return m_titles.value(tab); }

Q_SIGNALS:
    void collapsedChanged(bool);

private:
    friend class RibbonTabStrip;
    RibbonTabStrip *m_strip;
    QStackedWidget *m_body;
    QAction *m_collapse;
    QMenu *m_fileMenu;
    QMultiHash<QString, QToolButton *> m_buttons;
    QList<QStringList> m_titles;
    int m_current = 0;
    bool m_collapsed = false;
};

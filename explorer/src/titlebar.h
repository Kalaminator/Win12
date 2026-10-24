#pragma once
#include "ribbonbar.h"
#include <KConfigWatcher>
#include <KSharedConfig>
#include <QColor>
#include <QWidget>

class QToolButton;

struct WmColours {
    QColor activeBg, activeFg, inactiveBg, inactiveFg;
};

// Window title bar: app icon, the ribbon's tab strip, collapse button, drag area, min/max/close.
// Colours come from kdeglobals [WM] (KWin's), each missing key falling back to the scheme's Header colour.
class TitleBar : public QWidget
{
    Q_OBJECT
public:
    static constexpr int Height = RibbonBar::TabStripHeight;
    TitleBar(RibbonBar *ribbon, const KSharedConfig::Ptr &kdeglobals, QWidget *parent = nullptr);
    static WmColours readColours(const KSharedConfig::Ptr &kdeglobals);
    void reloadColours();
    void setActive(bool);
    void updateMaximizeIcon(); // restore or maximize glyph from the window state
    bool isActive() const { return m_active; }
    QColor background() const { return m_active ? m_colours.activeBg : m_colours.inactiveBg; }
    QColor foreground() const { return m_active ? m_colours.activeFg : m_colours.inactiveFg; }
    QToolButton *minimizeButton() const { return m_min; }
    QToolButton *maximizeButton() const { return m_max; }
    QToolButton *closeButton() const { return m_close; }
    QRect dragArea() const;

Q_SIGNALS:
    void coloursChanged();

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    bool event(QEvent *) override;

private:
    void toggleMaximized();
    void applyColours();
    KSharedConfig::Ptr m_config;
    KConfigWatcher::Ptr m_watcher;
    RibbonBar *m_ribbon;
    WmColours m_colours;
    QToolButton *m_min, *m_max, *m_close;
    QWidget *m_stretch;
    bool m_active = true;
    QPoint m_pressPos;
    bool m_dragPending = false;
};

#include "ribbonbar.h"
#include "exploreractions.h"
#include "ribbonspec.h"
#include <KColorScheme>
#include <KLocalizedString>
#include <QAction>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
constexpr int stripHeight = 28;

QColor midShade(const QPalette &p)
{
    return KColorScheme::shade(p.color(QPalette::Window), KColorScheme::MidShade);
}

// One group: buttons in a grid, label below; a 1 px MidShade line on its right edge.
class RibbonGroup : public QFrame
{
public:
    using QFrame::QFrame;

protected:
    void paintEvent(QPaintEvent *e) override
    {
        QFrame::paintEvent(e);
        QPainter p(this);
        p.fillRect(width() - 1, 4, 1, height() - 8, midShade(palette()));
    }
};

// Big button whose action text is wrapped onto two lines ("Pin to Quick / access" like the reference);
// QToolButton re-syncs text from its action on ActionChanged, so the wrap is re-applied after that.
class BigButton : public QToolButton
{
public:
    using QToolButton::QToolButton;
    void wrap()
    {
        QString t = defaultAction() ? defaultAction()->text() : text();
        const int sp = t.indexOf(QLatin1Char(' '), t.size() / 2 - 3);
        if (t.size() > 9 && sp > 0)
            setText(t.replace(sp, 1, QLatin1Char('\n')));
    }

protected:
    void actionEvent(QActionEvent *e) override
    {
        QToolButton::actionEvent(e);
        wrap();
    }
};
}

class RibbonTabStrip : public QWidget
{
public:
    RibbonTabStrip(RibbonBar *bar, const QStringList &tabs) : QWidget(bar), m_bar(bar), m_tabs(tabs)
    {
        setFixedHeight(stripHeight);
        setAutoFillBackground(false);
    }
    QRect tabRect(int i) const // i == -1 is the File tab
    {
        const QFontMetrics fm = fontMetrics();
        int x = 0;
        for (int k = -1; k < m_tabs.size(); ++k) {
            const int w = fm.horizontalAdvance(label(k)) + 28;
            if (k == i)
                return {x, 0, w, stripHeight};
            x += w;
        }
        return {};
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        const QPalette pal = palette();
        p.fillRect(rect(), pal.base());
        const QColor line = midShade(pal);
        const int cur = m_bar->isCollapsed() ? -2 : m_bar->currentTab();
        p.fillRect(0, stripHeight - 1, width(), 1, line); // the ribbon's top edge, opened under the current tab
        for (int i = -1; i < m_tabs.size(); ++i) {
            const QRect r = tabRect(i);
            if (i == -1) {
                p.fillRect(r, pal.highlight());
                p.setPen(pal.color(QPalette::HighlightedText));
            } else if (i == cur) {
                p.fillRect(r, pal.window());
                p.setPen(line);
                p.drawLine(r.topLeft(), r.bottomLeft());
                p.drawLine(r.topLeft(), r.topRight());
                p.drawLine(r.topRight(), r.bottomRight());
                p.fillRect(r.left() + 1, stripHeight - 1, r.width() - 1, 1, pal.window());
                p.setPen(pal.color(QPalette::WindowText));
            } else {
                p.setPen(pal.color(QPalette::WindowText));
            }
            p.drawText(r, Qt::AlignCenter, label(i));
        }
    }
    void mousePressEvent(QMouseEvent *e) override
    {
        if (e->button() != Qt::LeftButton)
            return;
        if (tabRect(-1).contains(e->position().toPoint())) {
            m_bar->fileMenu()->popup(mapToGlobal(tabRect(-1).bottomLeft()));
            return;
        }
        for (int i = 0; i < m_tabs.size(); ++i)
            if (tabRect(i).contains(e->position().toPoint())) {
                m_bar->setCurrentTab(i);
                if (m_bar->isCollapsed())
                    m_bar->setCollapsed(false);
                return;
            }
    }

private:
    QString label(int i) const { return i < 0 ? i18n("File") : m_tabs.at(i); }
    RibbonBar *m_bar;
    QStringList m_tabs;
};

RibbonBar::RibbonBar(ExplorerActions *actions, QWidget *parent) : QWidget(parent)
{
    const QList<Ribbon::Tab> &spec = Ribbon::spec();
    QStringList names;
    for (const Ribbon::Tab &t : spec)
        if (!t.isFileMenu)
            names << t.text;
    m_strip = new RibbonTabStrip(this, names);
    m_body = new QStackedWidget(this);
    m_body->setObjectName(QStringLiteral("ribbonBody"));
    m_body->setMinimumHeight(86);
    m_body->setAutoFillBackground(true); // palette().window()

    m_collapse = new QAction(QIcon::fromTheme(QStringLiteral("arrow-up")), i18n("Collapse the ribbon"), this);
    m_collapse->setCheckable(true);
    m_collapse->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F1));
    addAction(m_collapse);
    connect(m_collapse, &QAction::toggled, this, &RibbonBar::setCollapsed);

    m_fileMenu = new QMenu(this);
    auto *collapseButton = new QToolButton(m_strip);
    collapseButton->setDefaultAction(m_collapse);
    collapseButton->setAutoRaise(true);
    auto *stripLay = new QHBoxLayout(m_strip);
    stripLay->setContentsMargins(0, 0, 4, 1);
    stripLay->addStretch();
    stripLay->addWidget(collapseButton);

    for (const Ribbon::Tab &tab : spec) {
        if (tab.isFileMenu) {
            for (const Ribbon::Group &g : tab.groups)
                for (const Ribbon::Button &b : g.buttons)
                    m_fileMenu->addAction(actions->action(b.action));
            continue;
        }
        auto *page = new QWidget;
        auto *lay = new QHBoxLayout(page);
        lay->setContentsMargins(2, 2, 2, 2);
        lay->setSpacing(0);
        QStringList titles;
        if (tab.phase > Ribbon::currentPhase) {
            auto *l = new QLabel(i18n("Advanced tools arrive in a later Win12 version."));
            l->setEnabled(false);
            lay->addWidget(l);
        } else {
            for (const Ribbon::Group &g : tab.groups) {
                if (g.phase > Ribbon::currentPhase)
                    continue;
                titles << g.text;
                auto *frame = new RibbonGroup;
                auto *grid = new QGridLayout(frame);
                grid->setContentsMargins(4, 2, 5, 0);
                grid->setSpacing(0);
                int col = 0, row = 0;
                for (const Ribbon::Button &b : g.buttons) {
                    const bool big = b.size == Ribbon::Size::Big;
                    QToolButton *btn = big ? new BigButton : new QToolButton;
                    QAction *a = actions->action(b.action);
                    btn->setDefaultAction(a);
                    btn->setAutoRaise(true);
                    if (big) {
                        if (row) {
                            ++col;
                            row = 0;
                        }
                        btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
                        btn->setIconSize({32, 32});
                        btn->setMinimumWidth(52);
                        static_cast<BigButton *>(btn)->wrap();
                        grid->addWidget(btn, 0, col, 3, 1);
                        ++col;
                    } else {
                        btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
                        btn->setIconSize({16, 16});
                        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
                        grid->addWidget(btn, row, col);
                        if (++row == 3) {
                            row = 0;
                            ++col;
                        }
                    }
                    if (b.menu)
                        btn->setPopupMode(b.action == QLatin1String("home_delete") ? QToolButton::MenuButtonPopup
                                                                                     : QToolButton::InstantPopup);
                    m_buttons.insert(b.action, btn);
                }
                auto *title = new QLabel(g.text);
                title->setAlignment(Qt::AlignHCenter | Qt::AlignBottom);
                title->setForegroundRole(QPalette::PlaceholderText);
                QFont f = title->font();
                f.setPointSizeF(f.pointSizeF() - 1);
                title->setFont(f);
                grid->setRowStretch(3, 1);
                grid->addWidget(title, 3, 0, 1, qMax(1, col + (row ? 1 : 0)));
                lay->addWidget(frame);
            }
        }
        lay->addStretch();
        m_titles << titles;
        m_body->addWidget(page);
    }

    auto *main = new QVBoxLayout(this);
    main->setContentsMargins(0, 0, 0, 0);
    main->setSpacing(0);
    main->addWidget(m_strip);
    main->addWidget(m_body);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
}

void RibbonBar::setCollapsed(bool on)
{
    if (on == m_collapsed)
        return;
    m_collapsed = on;
    m_body->setVisible(!on);
    m_collapse->setChecked(on);
    m_collapse->setIcon(QIcon::fromTheme(on ? QStringLiteral("arrow-down") : QStringLiteral("arrow-up")));
    m_strip->update();
    Q_EMIT collapsedChanged(on);
}

void RibbonBar::setCurrentTab(int i)
{
    if (i < 0 || i >= m_body->count())
        return;
    m_current = i;
    m_body->setCurrentIndex(i);
    m_strip->update();
}

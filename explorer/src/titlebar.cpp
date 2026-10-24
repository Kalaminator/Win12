#include <QApplication>
#include "titlebar.h"
#include <KColorScheme>
#include <KConfigGroup>
#include <QAction>
#include <QEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QToolButton>
#include <QWindow>

namespace {
QColor wmColour(const KConfigGroup &g, const char *key, const QColor &fallback)
{
    const QColor c = g.readEntry(key, QColor());
    return c.isValid() ? c : fallback;
}

QToolButton *windowButton(QWidget *parent, const char *icon)
{
    auto *b = new QToolButton(parent);
    b->setIcon(QIcon::fromTheme(QString::fromLatin1(icon)));
    b->setAutoRaise(true);
    b->setFixedSize(46, TitleBar::Height);
    b->setFocusPolicy(Qt::NoFocus);
    return b;
}
}

WmColours TitleBar::readColours(const KSharedConfig::Ptr &kdeglobals)
{
    const KConfigGroup g(kdeglobals, QStringLiteral("WM"));
    const KColorScheme act(QPalette::Active, KColorScheme::Header);
    const KColorScheme ina(QPalette::Inactive, KColorScheme::Header);
    return {wmColour(g, "activeBackground", act.background().color()),
            wmColour(g, "activeForeground", act.foreground().color()),
            wmColour(g, "inactiveBackground", ina.background().color()),
            wmColour(g, "inactiveForeground", ina.foreground().color())};
}

TitleBar::TitleBar(RibbonBar *ribbon, const KSharedConfig::Ptr &kdeglobals, QWidget *parent)
    : QWidget(parent), m_config(kdeglobals), m_ribbon(ribbon)
{
    setFixedHeight(Height);
    auto *icon = new QLabel(this);
    icon->setPixmap(QIcon::fromTheme(QStringLiteral("system-file-manager")).pixmap(16, 16));
    icon->setAttribute(Qt::WA_TransparentForMouseEvents);
    auto *collapse = new QToolButton(this);
    collapse->setDefaultAction(ribbon->collapseAction());
    collapse->setAutoRaise(true);
    m_stretch = new QWidget(this);
    m_min = windowButton(this, "window-minimize");
    m_max = windowButton(this, "window-maximize");
    m_close = windowButton(this, "window-close");

    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(8, 0, 0, 0);
    lay->setSpacing(0);
    lay->addWidget(icon);
    lay->addSpacing(8);
    lay->addWidget(ribbon->takeTabStrip());
    lay->addWidget(collapse);
    lay->addWidget(m_stretch, 1);
    lay->addWidget(m_min);
    lay->addWidget(m_max);
    lay->addWidget(m_close);

    connect(m_min, &QToolButton::clicked, this, [this] { window()->showMinimized(); });
    connect(m_max, &QToolButton::clicked, this, &TitleBar::toggleMaximized);
    connect(m_close, &QToolButton::clicked, this, [this] { window()->close(); });

    m_watcher = KConfigWatcher::create(kdeglobals);
    connect(m_watcher.data(), &KConfigWatcher::configChanged, this, [this](const KConfigGroup &g, const QByteArrayList &) {
        if (g.name() == QLatin1String("WM"))
            reloadColours();
    });
    applyColours();
}

void TitleBar::reloadColours()
{
    m_config->reparseConfiguration();
    applyColours();
}

void TitleBar::applyColours()
{
    m_colours = readColours(m_config);
    updateMaximizeIcon();
    m_ribbon->setTabStripColours(background(), foreground());
    QColor hover = foreground();
    hover.setAlphaF(0.15);
    const QString rgba = QStringLiteral("rgba(%1,%2,%3,%4)").arg(hover.red()).arg(hover.green()).arg(hover.blue()).arg(hover.alpha());
    setStyleSheet(QStringLiteral("QToolButton{border:none;border-radius:0;background:transparent;color:%1;}"
                                 "QToolButton:hover{background:%2;}"
                                 "QToolButton:pressed{background:%2;}")
                      .arg(foreground().name(), rgba)
                  + QStringLiteral("QToolButton#tbClose:hover{background:#c42b1c;}"));
    m_close->setObjectName(QStringLiteral("tbClose"));
    style()->unpolish(m_close);
    style()->polish(m_close);
    update();
    Q_EMIT coloursChanged();
}

void TitleBar::updateMaximizeIcon()
{
    m_max->setIcon(QIcon::fromTheme(window()->isMaximized() ? QStringLiteral("window-restore") : QStringLiteral("window-maximize")));
}

void TitleBar::setActive(bool on)
{
    if (on == m_active)
        return;
    m_active = on;
    m_ribbon->setTabStripColours(background(), foreground());
    update();
}

QRect TitleBar::dragArea() const
{
    return m_stretch->geometry();
}

void TitleBar::toggleMaximized()
{
    window()->isMaximized() ? window()->showNormal() : window()->showMaximized();
}

void TitleBar::paintEvent(QPaintEvent *)
{
    QPainter(this).fillRect(rect(), background());
}

void TitleBar::mousePressEvent(QMouseEvent *e)
{
    m_dragPending = e->button() == Qt::LeftButton && dragArea().contains(e->position().toPoint());
    m_pressPos = e->position().toPoint();
}

void TitleBar::mouseMoveEvent(QMouseEvent *e)
{
    // the move starts only after a drag, so a double-click reaches mouseDoubleClickEvent on every compositor
    if (m_dragPending && (e->buttons() & Qt::LeftButton)
        && (e->position().toPoint() - m_pressPos).manhattanLength() >= QApplication::startDragDistance()) {
        m_dragPending = false;
        if (window()->windowHandle())
            window()->windowHandle()->startSystemMove();
    }
}

void TitleBar::mouseReleaseEvent(QMouseEvent *)
{
    m_dragPending = false;
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && dragArea().contains(e->position().toPoint()))
        toggleMaximized();
}

bool TitleBar::event(QEvent *e)
{
    const bool r = QWidget::event(e);
    if (e->type() == QEvent::ApplicationPaletteChange)
        applyColours();
    return r;
}

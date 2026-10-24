#include "windowshadow.h"
#include <KWindowShadow>
#include <QEvent>
#include <QPainter>
#include <QWidget>
#include <QWindow>

QImage WindowShadow::image(int radius, const QColor &colour)
{
    const int n = 2 * radius + 1;
    QImage img(n, n, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setCompositionMode(QPainter::CompositionMode_Source);
    for (int i = 0; i < radius; ++i) {
        const double f = (i + 1.0) / radius;
        QColor c = colour;
        c.setAlphaF(0.30 * f * f);
        p.fillRect(QRect(i, i, n - 2 * i, n - 2 * i), c);
    }
    return img;
}

namespace
{
class Installer : public QObject
{
public:
    Installer(QWidget *w, int radius)
        : QObject(w)
        , m_w(w)
        , m_radius(radius)
    {
        w->installEventFilter(this);
    }
    bool eventFilter(QObject *o, QEvent *e) override
    {
        if (o == m_w && e->type() == QEvent::Show)
            create();
        return false;
    }

private:
    void create()
    {
        QWindow *win = m_w->windowHandle();
        if (!win || win == m_win)
            return;
        m_win = win;
        delete m_shadow;
        m_shadow = new KWindowShadow(this);
        const int r = m_radius;
        const QImage img = WindowShadow::image(r, Qt::black);
        auto tile = [&](const QRect &rc) {
            auto t = KWindowShadowTile::Ptr::create();
            t->setImage(img.copy(rc));
            return t;
        };
        m_shadow->setTopLeftTile(tile({0, 0, r, r}));
        m_shadow->setTopTile(tile({r, 0, 1, r}));
        m_shadow->setTopRightTile(tile({r + 1, 0, r, r}));
        m_shadow->setRightTile(tile({r + 1, r, r, 1}));
        m_shadow->setBottomRightTile(tile({r + 1, r + 1, r, r}));
        m_shadow->setBottomTile(tile({r, r + 1, 1, r}));
        m_shadow->setBottomLeftTile(tile({0, r + 1, r, r}));
        m_shadow->setLeftTile(tile({0, r, r, 1}));
        m_shadow->setPadding(QMargins(r, r, r, r));
        m_shadow->setWindow(win);
        m_shadow->create(); // false offscreen: harmless
    }
    QWidget *m_w;
    int m_radius;
    QWindow *m_win = nullptr;
    KWindowShadow *m_shadow = nullptr;
};
}

void WindowShadow::install(QWidget *window, int radius)
{
    new Installer(window, radius);
}

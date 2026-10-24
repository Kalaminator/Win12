#include "tagribbon.h"
#include <QGuiApplication>
#include <QPainter>
#include <QPixmap>
#include <QPolygonF>

namespace TagRibbon {

QIcon icon(const Tag &t)
{
    const QFontMetrics fm(font(QGuiApplication::font()));
    QPixmap px(width(t, fm), Height);
    px.fill(Qt::transparent);
    QPainter p(&px);
    paint(&p, px.rect(), t, fm);
    p.end();
    return QIcon(px);
}

QFont font(const QFont &base)
{
    QFont f = base;
    f.setWeight(QFont::DemiBold);
    f.setPixelSize(11); // ponytail: fixed 11 px like the mockup; scale with DPI if ever needed
    return f;
}

int width(const Tag &t, const QFontMetrics &fm)
{
    return PadLeft + fm.horizontalAdvance(t.display()) + PadRight;
}

void paint(QPainter *p, const QRect &slot, const Tag &t, const QFontMetrics &fm)
{
    const int w = width(t, fm);
    const int y = slot.top() + (slot.height() - Height) / 2;
    p->save();
    p->setRenderHint(QPainter::Antialiasing, false); // hard edges keep the arrow cut exact
    p->translate(slot.left(), y);
    p->setPen(Qt::NoPen);
    p->setBrush(t.paintColour());
    p->drawPolygon(QPolygon{QPoint(0, 0), QPoint(w - Point, 0), QPoint(w, Height / 2), QPoint(w - Point, Height), QPoint(0, Height)});
    p->setFont(font(p->font()));
    p->setPen(Qt::white);
    p->drawText(QRect(PadLeft, 0, w - PadLeft - PadRight, Height), Qt::AlignVCenter | Qt::AlignLeft, t.display());
    p->restore();
}

void paintFlags(QPainter *p, const QRect &r, const QList<Tag> &tags)
{
    // Same geometry as Emblem::svg: 12x5 flags, pitch 6, x from right-12, first tag on top.
    const int n = qMin<qsizetype>(tags.size(), MaxTags);
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    p->setPen(Qt::NoPen);
    for (int k = 0; k < n; ++k) {
        const qreal x = r.right() + 1 - 12, y = r.bottom() + 1 - 5 - 6 * (n - 1 - k);
        p->setBrush(tags[k].paintColour());
        p->drawPolygon(QPolygonF{QPointF(x, y), QPointF(x + 8.4, y), QPointF(x + 12, y + 2.5), QPointF(x + 8.4, y + 5), QPointF(x, y + 5)});
    }
    p->restore();
}

}

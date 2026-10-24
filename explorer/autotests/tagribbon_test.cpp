#include "tagribbon.h"
#include <QImage>
#include <QPainter>
#include <QTest>

class TagRibbonTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void widthGrowsWithText()
    {
        const QFontMetrics fm(TagRibbon::font(QFont()));
        QVERIFY(TagRibbon::width(Tag::decode(QStringLiteral("Red/Xiaomi Pad")), fm) > TagRibbon::width(Tag::decode(QStringLiteral("Red/X")), fm));
        QVERIFY(TagRibbon::width(Tag::decode(QStringLiteral("Red/X")), fm) > TagRibbon::PadLeft + TagRibbon::PadRight);
    }
    void paintFillsSlotColour()
    {
        QImage img(200, 18, QImage::Format_ARGB32);
        img.fill(Qt::white);
        const QFontMetrics fm(TagRibbon::font(QFont()));
        const Tag t = Tag::decode(QStringLiteral("Red/Xiaomi"));
        const int w = TagRibbon::width(t, fm);
        const QRect slot(0, 0, w, TagRibbon::Height);
        {
            QPainter p(&img);
            TagRibbon::paint(&p, slot, t, fm);
        }
        QCOMPARE(QColor(img.pixel(2, 9)), t.paintColour());
        QCOMPARE(QColor(img.pixel(slot.right() - 1, 1)), QColor(Qt::white));
    }
    void paintFlagsThreeMax()
    {
        QImage img(32, 32, QImage::Format_ARGB32);
        img.fill(Qt::transparent);
        QList<Tag> tags;
        for (int i = 0; i < 5; ++i)
            tags << Tag{i, QString()};
        {
            QPainter p(&img);
            TagRibbon::paintFlags(&p, QRect(0, 0, 32, 32), tags);
        }
        QCOMPARE(QColor(img.pixel(20, 29)), tags[2].paintColour()); // bottom flag = third
        QCOMPARE(QColor(img.pixel(20, 23)), tags[1].paintColour());
        QCOMPARE(QColor(img.pixel(20, 17)), tags[0].paintColour()); // first on top
        QCOMPARE(qAlpha(img.pixel(20, 13)), 0);             // no 4th flag
    }
};
QTEST_MAIN(TagRibbonTest)
#include "tagribbon_test.moc"

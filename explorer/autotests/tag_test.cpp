#include "tag.h"
using namespace Qt::StringLiterals;
#include <QSet>
#include <QTest>
#include <QColor>

class TagTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void fortyEightUniqueNames()
    {
        QSet<QString> names;
        for (int i = 0; i < TagColours::Count; ++i) {
            const QString n = TagColours::name(i);
            QVERIFY(!n.isEmpty() && !n.contains(u'/') && !n.contains(u' '));
            names.insert(n);
        }
        QCOMPARE(names.size(), 48);
    }
    void firstAndLastHex()
    {
        QCOMPARE(TagColours::colour(0), QColor("#ffb900"));
        QCOMPARE(TagColours::colour(47), QColor("#7e735f"));
        QCOMPARE(TagColours::name(16), QStringLiteral("Blue"));
        QCOMPARE(TagColours::indexOfColour(QColor("#7e735f")), 47);
        QCOMPARE(TagColours::indexOfColour(QColor("#123456")), -1);
        QCOMPARE(TagColours::indexOfName(u"bLuE"_s), 16);
        QCOMPARE(TagColours::indexOfName(u"nope"_s), -1);
    }
    void encodeDecodeRoundTrip()
    {
        for (int i = 0; i < TagColours::Count; ++i) {
            const Tag t{i, QStringLiteral("Xiaomi")};
            QCOMPARE(Tag::decode(t.encode()), t);
        }
    }
    void colourOnlyEncodesAsName()
    {
        const Tag t{9, {}};
        QCOMPARE(t.encode(), QStringLiteral("Red"));
        QCOMPARE(Tag::decode(u"Red"_s), t);
        QCOMPARE(t.display(), QStringLiteral("Red"));
        QCOMPARE((Tag{9, QStringLiteral("  ")}).encode(), QStringLiteral("Red"));
        QCOMPARE((Tag{9, QStringLiteral(" Xiaomi ")}).encode(), QStringLiteral("Red/Xiaomi"));
    }
    void decodeIsCaseInsensitiveOnColour()
    {
        const Tag t = Tag::decode(u"red/Xiaomi"_s);
        QCOMPARE(t.colour, 9);
        QCOMPARE(t.word, QStringLiteral("Xiaomi"));
        QCOMPARE(t.display(), QStringLiteral("Xiaomi"));
    }
    void decodeForeignAndOddStrings()
    {
        const Tag f = Tag::decode(u"holiday"_s);
        QVERIFY(f.isForeign());
        QCOMPARE(f.word, QStringLiteral("holiday"));
        QCOMPARE(f.encode(), QStringLiteral("holiday"));
        QCOMPARE(f.display(), QStringLiteral("holiday"));
        QCOMPARE(f.paintColour(), TagColours::colour(TagColours::indexOfName(u"Overcast"_s)));
        QCOMPARE(Tag::decode(u"Red/"_s), (Tag{9, {}}));
        QCOMPARE(Tag::decode(u"Red/a/b"_s), (Tag{9, QStringLiteral("a/b")}));
        QCOMPARE(Tag::decode(u"Red/a/b"_s).encode(), QStringLiteral("Red/a/b"));
        const Tag e = Tag::decode(QString());
        QVERIFY(e.isForeign());
        QVERIFY(e.word.isEmpty());
    }
    void sameWordOtherColourDiffers() { QVERIFY((Tag{9, u"a"_s}) != (Tag{10, u"a"_s})); }
    void win12TagCountIgnoresForeign()
    {
        const QList<Tag> l = decodeAll({u"Red/a"_s, u"holiday"_s, u"Blue"_s});
        QCOMPARE(l.size(), 3);
        QCOMPARE(win12TagCount(l), 2);
        QCOMPARE(encodeAll(l), (QStringList{u"Red/a"_s, u"holiday"_s, u"Blue"_s}));
        QCOMPARE(MaxTags, 3);
    }
};

QTEST_MAIN(TagTest)
#include "tag_test.moc"

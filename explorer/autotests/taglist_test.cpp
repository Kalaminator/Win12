#include "tagregistry.h"
#include "taglist.h"
#include <QListWidget>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>

Q_DECLARE_METATYPE(QList<Tag>)

class TagListTest : public QObject
{
    Q_OBJECT
    QTemporaryDir m_dir;
    static Tag T(const char *s) { return Tag::decode(QString::fromLatin1(s)); }
    void click(TagList &l, int row, Qt::KeyboardModifiers mod = {})
    {
        auto *list = l.findChild<QListWidget *>();
        QTest::mouseClick(list->viewport(), Qt::LeftButton, mod, list->visualItemRect(list->item(row)).center());
    }
private Q_SLOTS:
    void initTestCase() { qRegisterMetaType<QList<Tag>>(); }
    void listsRegistryTags()
    {
        TagRegistry r(m_dir.filePath(QStringLiteral("a.json")));
        r.touch(T("Red/x"));
        r.touch(T("Blue/y"));
        TagList l(&r);
        QCOMPARE(l.findChild<QListWidget *>()->count(), 2);
        r.touch(T("Green/z"));
        QCOMPARE(l.findChild<QListWidget *>()->count(), 3);
    }
    void clickPicksOne()
    {
        TagRegistry r(m_dir.filePath(QStringLiteral("b.json")));
        r.touch(T("Red/x"));
        r.touch(T("Blue/y"));
        TagList l(&r);
        l.show();
        QSignalSpy spy(&l, &TagList::filterChanged);
        click(l, 0);
        QCOMPARE(l.picked(), QList<Tag>{T("Blue/y")}); // most recent first
        click(l, 1);
        QCOMPARE(l.picked(), QList<Tag>{T("Red/x")});
        QVERIFY(spy.count() >= 1);
    }
    void ctrlClickPicksMore()
    {
        TagRegistry r(m_dir.filePath(QStringLiteral("c.json")));
        r.touch(T("Red/x"));
        r.touch(T("Blue/y"));
        TagList l(&r);
        l.show();
        click(l, 0);
        click(l, 1, Qt::ControlModifier);
        QCOMPARE(l.picked().size(), 2);
        click(l, 1, Qt::ControlModifier);
        QCOMPARE(l.picked().size(), 1);
    }
    void modeToggleEmits()
    {
        TagRegistry r(m_dir.filePath(QStringLiteral("d.json")));
        r.touch(T("Red/x"));
        TagList l(&r);
        l.show();
        click(l, 0);
        QCOMPARE(l.mode(), TagSearch::Mode::All);
        QSignalSpy spy(&l, &TagList::filterChanged);
        const auto buttons = l.findChildren<QToolButton *>();
        buttons.last()->click();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(1).value<TagSearch::Mode>(), TagSearch::Mode::Any);
        QCOMPARE(l.mode(), TagSearch::Mode::Any);
    }
    void clearPicksEmitsEmpty()
    {
        TagRegistry r(m_dir.filePath(QStringLiteral("e.json")));
        r.touch(T("Red/x"));
        TagList l(&r);
        l.show();
        click(l, 0);
        QSignalSpy spy(&l, &TagList::filterChanged);
        l.clearPicks();
        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.at(0).at(0).value<QList<Tag>>().isEmpty());
    }
};

QTEST_MAIN(TagListTest)
#include "taglist_test.moc"

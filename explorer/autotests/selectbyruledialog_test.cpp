#include "selectbyruledialog.h"
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>
#include <QTreeWidget>

class SelectByRuleDialogTest : public QObject
{
    Q_OBJECT
    QTemporaryDir dir;

    static QStringList names(QTreeWidget *t)
    {
        QStringList l;
        for (int i = 0; i < t->topLevelItemCount(); ++i)
            l << t->topLevelItem(i)->text(0);
        l.sort();
        return l;
    }
    // sorted results, row order irrelevant to asynchronous arrival
    void runFind(SelectByRuleDialog &d)
    {
        QSignalSpy sp(&d, &SelectByRuleDialog::searchFinished);
        d.find();
        QVERIFY(sp.wait(5000) || sp.count());
    }
    SelectByRuleDialog *make(SelectByRuleDialog::Action a = SelectByRuleDialog::Action::Select)
    {
        auto *d = new SelectByRuleDialog(QUrl::fromLocalFile(dir.path()), a, nullptr);
        d->setRules({{Field::Extension, Test::Equals, QStringLiteral("txt")}});
        d->setRecursive(true);
        return d;
    }
private Q_SLOTS:
    void initTestCase()
    {
        QDir d(dir.path());
        d.mkpath(QStringLiteral("sub"));
        for (const char *f : {"a.txt", "sub/b.txt", "sub/c.log"}) {
            QFile file(d.filePath(QString::fromLatin1(f)));
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
    }
    void findListsMatchesRelativeToLookIn()
    {
        std::unique_ptr<SelectByRuleDialog> d(make());
        runFind(*d);
        auto *t = d->findChild<QTreeWidget *>(QStringLiteral("results"));
        QCOMPARE(names(t), (QStringList{QStringLiteral("a.txt"), QStringLiteral("sub/b.txt")}));
        QVERIFY(d->findChild<QLabel *>(QStringLiteral("countLabel"))->text().startsWith(QStringLiteral("Found: 2 items")));
        QCOMPARE(d->ticked().size(), 2);
    }
    void untickKeepsItem()
    {
        std::unique_ptr<SelectByRuleDialog> d(make());
        runFind(*d);
        auto *t = d->findChild<QTreeWidget *>(QStringLiteral("results"));
        t->sortItems(0, Qt::AscendingOrder);
        t->topLevelItem(0)->setCheckState(0, Qt::Unchecked);
        QCOMPARE(d->ticked().size(), 1);
        QCOMPARE(d->ticked().first().url().fileName(), QStringLiteral("b.txt"));
        QVERIFY(d->findChild<QLabel *>(QStringLiteral("countLabel"))->text().startsWith(QStringLiteral("Found: 1 items")));
    }
    void invalidRegexDisablesFind()
    {
        std::unique_ptr<SelectByRuleDialog> d(make());
        d->setRules({{Field::Name, Test::Regex, QStringLiteral("[")}});
        auto *f = d->findChild<QPushButton *>(QStringLiteral("findButton"));
        QVERIFY(!f->isEnabled());
        QVERIFY(!d->findChild<QLabel *>(QStringLiteral("errorLabel"))->text().isEmpty());
        d->setRules({{Field::Name, Test::Regex, QStringLiteral("a")}});
        QVERIFY(f->isEnabled());
    }
    void defaultActionButtonIsDefault()
    {
        std::unique_ptr<SelectByRuleDialog> d(make(SelectByRuleDialog::Action::Trash));
        QVERIFY(d->findChild<QPushButton *>(QStringLiteral("btnTrash"))->isDefault());
        QVERIFY(!d->findChild<QPushButton *>(QStringLiteral("btnSelect"))->isDefault());
    }
    void selectDisabledForSubfolderMatches()
    {
        std::unique_ptr<SelectByRuleDialog> d(make());
        auto *sel = d->findChild<QPushButton *>(QStringLiteral("btnSelect"));
        QVERIFY(!sel->isEnabled()); // nothing ticked yet
        runFind(*d);
        QVERIFY(!sel->isEnabled());
        auto *t = d->findChild<QTreeWidget *>(QStringLiteral("results"));
        for (int i = 0; i < t->topLevelItemCount(); ++i)
            if (t->topLevelItem(i)->text(0).contains(QLatin1Char('/')))
                t->topLevelItem(i)->setCheckState(0, Qt::Unchecked);
        QVERIFY(sel->isEnabled());
        QVERIFY(d->findChild<QPushButton *>(QStringLiteral("btnTrash"))->isEnabled());
    }
    void addAndRemoveRuleRows()
    {
        std::unique_ptr<SelectByRuleDialog> d(make());
        QCOMPARE(d->rules().size(), 1);
        d->findChild<QAbstractButton *>(QStringLiteral("addRule"))->click();
        QCOMPARE(d->rules().size(), 2);
        d->findChildren<QToolButton *>(QStringLiteral("removeRule")).first()->click();
        QCOMPARE(d->rules().size(), 1);
    }
};

QTEST_MAIN(SelectByRuleDialogTest)
#include "selectbyruledialog_test.moc"

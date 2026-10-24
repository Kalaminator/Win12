#include "selectbyruledialog.h"
#include "formulaedit.h"
#include "rulelibrary.h"
#include <QInputDialog>
#include <QStandardPaths>
#include <QTimer>
#include <QApplication>
#include <KLineEdit>
#include <KUrlRequester>
#include <QAction>
#include <QComboBox>
#include <QDir>
#include <QKeyEvent>
#include <QLineEdit>
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
    void fieldComboOffersTag()
    {
        std::unique_ptr<SelectByRuleDialog> d(make(SelectByRuleDialog::Action::Select));
        auto *combo = d->findChild<QComboBox *>();
        QVERIFY(combo);
        QVERIFY(combo->findText(fieldText(Field::Tag)) >= 0);
    }
    void addTagActionInMenu()
    {
        std::unique_ptr<SelectByRuleDialog> d(make(SelectByRuleDialog::Action::Select));
        auto *qa = d->findChild<QAction *>(QStringLiteral("actAddTag"));
        QVERIFY(qa);
        QVERIFY(qa->text().startsWith(QLatin1String("Add tag")));
        runFind(*d);
        qa->trigger();
        QCOMPARE(d->chosenAction(), SelectByRuleDialog::Action::AddTag);
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
    void enterNeverReachesActionButton()
    {
        std::unique_ptr<SelectByRuleDialog> d(make(SelectByRuleDialog::Action::Trash));
        d->show();
        runFind(*d);
        QVERIFY(!d->ticked().isEmpty());
        QTest::keyClick(static_cast<QWidget *>(d->findChild<KUrlRequester *>()->lineEdit()), Qt::Key_Return);
        QCOMPARE(d->result(), int(QDialog::Rejected));
        QVERIFY(d->isVisible());
        d->setRules({{Field::Name, Test::Regex, QStringLiteral("[")}});
        QTest::keyClick(d->findChildren<QLineEdit *>().last(), Qt::Key_Enter);
        QCOMPARE(d->result(), int(QDialog::Rejected));
        QVERIFY(d->isVisible());
        QVERIFY(d->ticked().isEmpty()); // stale ticks gone
    }
    void enterOnToolButtonDoesNotReachActionButton()
    {
        std::unique_ptr<SelectByRuleDialog> d(make(SelectByRuleDialog::Action::Trash));
        d->show();
        runFind(*d);
        auto *tb = d->findChildren<QToolButton *>().value(0);
        QVERIFY(tb);
        tb->setFocus();
        QTest::keyClick(tb, Qt::Key_Return);
        QCOMPARE(d->result(), int(QDialog::Rejected));
        QVERIFY(d->isVisible());
    }
    void enterInValueFieldSearchesOnce()
    {
        std::unique_ptr<SelectByRuleDialog> d(make());
        d->show();
        QSignalSpy sp(d.get(), &SelectByRuleDialog::searchFinished);
        QTest::keyClick(d->findChildren<QLineEdit *>().last(), Qt::Key_Return);
        QVERIFY(sp.wait(5000));
        QTest::qWait(300);
        QCOMPARE(sp.count(), 1);
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

    // ---- fx formula mode ----
    void formulaModeHidesRulesShowsEdit()
    {
        std::unique_ptr<SelectByRuleDialog> d(make());
        d->show();
        d->setMode(SelectByRuleDialog::Mode::Formula);
        QVERIFY(d->findChild<FormulaEdit *>()->isVisible());
        QVERIFY(!d->findChild<QAbstractButton *>(QStringLiteral("addRule"))->isVisible());
        d->setMode(SelectByRuleDialog::Mode::Rules);
        QVERIFY(!d->findChild<FormulaEdit *>()->isVisible());
        QVERIFY(d->findChild<QAbstractButton *>(QStringLiteral("addRule"))->isVisible());
    }
    void formulaFindListsAllRowsTicksTrue()
    {
        QTemporaryDir fd;
        QFile a(fd.filePath(QStringLiteral("a.txt")));
        QVERIFY(a.open(QIODevice::WriteOnly));
        a.write("abc");
        a.close();
        QFile b(fd.filePath(QStringLiteral("b.txt")));
        QVERIFY(b.open(QIODevice::WriteOnly));
        b.close();
        SelectByRuleDialog d(QUrl::fromLocalFile(fd.path()), SelectByRuleDialog::Action::Select, nullptr);
        d.setMode(SelectByRuleDialog::Mode::Formula);
        d.setFormula(QStringLiteral("SIZE>0"));
        runFind(d);
        auto *t = d.findChild<QTreeWidget *>(QStringLiteral("results"));
        QTRY_COMPARE(t->topLevelItemCount(), 2);
        QCOMPARE(d.ticked().size(), 1);
        QCOMPARE(d.ticked().first().url().fileName(), QStringLiteral("a.txt"));
        QCOMPARE(d.findChild<QLabel *>(QStringLiteral("countLabel"))->text(), QStringLiteral("1 row is TRUE"));
    }
    void formulaErrorRowShowsCode()
    {
        QTemporaryDir fd;
        QFile b(fd.filePath(QStringLiteral("b.txt")));
        QVERIFY(b.open(QIODevice::WriteOnly));
        b.close();
        SelectByRuleDialog d(QUrl::fromLocalFile(fd.path()), SelectByRuleDialog::Action::Select, nullptr);
        d.setMode(SelectByRuleDialog::Mode::Formula);
        d.setFormula(QStringLiteral("1/SIZE"));
        runFind(d);
        auto *t = d.findChild<QTreeWidget *>(QStringLiteral("results"));
        QTRY_COMPARE(t->topLevelItemCount(), 1);
        QCOMPARE(t->topLevelItem(0)->text(t->columnCount() - 1), QStringLiteral("#DIV/0!"));
        QVERIFY(d.ticked().isEmpty());
    }
    void invalidFormulaDisablesFind()
    {
        std::unique_ptr<SelectByRuleDialog> d(make());
        d->setMode(SelectByRuleDialog::Mode::Formula);
        d->setFormula(QStringLiteral("LEFT(NAME"));
        QTRY_VERIFY(!d->findChild<QPushButton *>(QStringLiteral("findButton"))->isEnabled());
        QCOMPARE(d->findChild<QLabel *>(QStringLiteral("fxError"))->text(), QStringLiteral("Expected )"));
    }
    void saveAsStoresSelectRule()
    {
        QStandardPaths::setTestModeEnabled(true);
        QFile::remove(RuleLibrary::path());
        RuleLibrary lib;
        std::unique_ptr<SelectByRuleDialog> d(make());
        d->setLibrary(&lib);
        d->setMode(SelectByRuleDialog::Mode::Formula);
        d->setFormula(QStringLiteral("SIZE>0"));
        QTimer::singleShot(200, [] {
            auto *w = qobject_cast<QInputDialog *>(QApplication::activeModalWidget());
            if (w) {
                w->setTextValue(QStringLiteral("Old"));
                w->accept();
            }
        });
        d->findChild<QAbstractButton *>(QStringLiteral("fxSave"))->click();
        QCOMPARE(lib.all().size(), 1);
        QCOMPARE(lib.all().first(), (SavedRule{QStringLiteral("Old"), QStringLiteral("SIZE>0"), SavedRule::Kind::Select}));
    }
    void setRulesInFormulaModeStaysHidden()
    {
        std::unique_ptr<SelectByRuleDialog> d(make());
        d->show();
        d->setMode(SelectByRuleDialog::Mode::Formula);
        d->setRules({{Field::Name, Test::Contains, QStringLiteral("x")}});
        for (auto *w : d->findChildren<QComboBox *>())
            QVERIFY(!w->isVisible());
    }
    void everyFunctionHasAHelpGroup()
    {
        for (const FxFunctionSpec &f : FxFunctions::all())
            QVERIFY2(!fxFunctionGroup(f.name).isEmpty(), qPrintable(f.name));
    }
    void rejectStopsFormulaRun()
    {
        std::unique_ptr<SelectByRuleDialog> d(make());
        d->setMode(SelectByRuleDialog::Mode::Formula);
        d->setFormula(QStringLiteral("SIZE>0"));
        QSignalSpy sp(d.get(), &SelectByRuleDialog::searchFinished);
        d->find();
        d->reject();
        QTest::qWait(300);
        QCOMPARE(sp.count(), 0);
    }
};

QTEST_MAIN(SelectByRuleDialogTest)
#include "selectbyruledialog_test.moc"

#include "renamemanydialog.h"
#include "formulaedit.h"
#include <KFileItem>
#include <QDialogButtonBox>
#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QTest>
#include <QToolButton>
#include <QTreeWidget>

class RenameManyDialogTest : public QObject
{
    Q_OBJECT
    static QList<RenameItem> items()
    {
        QList<RenameItem> l;
        for (const char *n : {"holiday.jpg", "beach.jpg", "notes.txt"})
            l << RenameItem{QUrl::fromLocalFile(QStringLiteral("/d/") + QLatin1String(n)), QLatin1String(n), {}};
        return l;
    }
    static QSet<QString> existing() { return {QStringLiteral("holiday.jpg"), QStringLiteral("beach.jpg"), QStringLiteral("notes.txt"), QStringLiteral("03 - notes.txt")}; }
    static QString okText(QDialog &d) { return d.findChild<QDialogButtonBox *>(QStringLiteral("buttonBox"))->button(QDialogButtonBox::Ok)->text(); }
    static bool okEnabled(QDialog &d) { return d.findChild<QDialogButtonBox *>(QStringLiteral("buttonBox"))->button(QDialogButtonBox::Ok)->isEnabled(); }

private Q_SLOTS:
    void defaultBlocksMatchMockup()
    {
        RenameManyDialog d(items(), existing(), nullptr);
        auto *t = d.findChild<QTreeWidget *>(QStringLiteral("previewTree"));
        QVERIFY(t);
        QCOMPARE(t->topLevelItemCount(), 3);
        QCOMPARE(t->topLevelItem(0)->text(1), QStringLiteral("01 - holiday.jpg"));
        QCOMPARE(t->topLevelItem(1)->text(1), QStringLiteral("02 - beach.jpg"));
        QCOMPARE(t->topLevelItem(2)->foreground(1).color(), QColor(Qt::red));
        QCOMPARE(okText(d), QStringLiteral("Rename 2"));
    }
    void reorderBlocksChangesPreview()
    {
        RenameManyDialog d(items(), existing(), nullptr);
        auto b = d.blocks();
        std::reverse(b.begin(), b.end());
        d.setBlocks(b);
        QCOMPARE(d.blocks().first().kind, RenameBlock::CurrentName);
        QCOMPARE(d.plan().first().newName, QStringLiteral("holiday - 01.jpg"));
        auto *t = d.findChild<QTreeWidget *>(QStringLiteral("previewTree"));
        QCOMPARE(t->topLevelItem(0)->text(1), QStringLiteral("holiday - 01.jpg"));
    }
    void editingBlockUpdatesPreview()
    {
        RenameManyDialog d(items(), existing(), nullptr);
        auto *l = d.findChild<QListWidget *>(QStringLiteral("blockList"));
        l->setCurrentRow(0);
        auto *s = d.findChild<QSpinBox *>(QStringLiteral("digitsSpin"));
        QVERIFY(s);
        s->setValue(3);
        QCOMPARE(d.plan().first().newName, QStringLiteral("001 - holiday.jpg"));
        QCOMPARE(d.blocks().first().digits, 3);
    }
    void okDisabledWhenNothingToRename()
    {
        RenameManyDialog d(items(), existing(), nullptr);
        RenameBlock cur;
        cur.kind = RenameBlock::CurrentName;
        d.setBlocks({cur});
        QVERIFY(!okEnabled(d));
        QCOMPARE(okText(d), QStringLiteral("Rename 0"));
    }
    void whyColumnShowsClash()
    {
        RenameManyDialog d(items(), {}, nullptr);
        RenameBlock t;
        t.text = QStringLiteral("a");
        d.setBlocks({t});
        auto *tr = d.findChild<QTreeWidget *>(QStringLiteral("previewTree"));
        QCOMPARE(tr->topLevelItem(1)->text(2), QStringLiteral("Same name planned above"));
    }
    void formulaModeMockup()
    {
        QTemporaryDir tmp;
        QList<RenameItem> l;
        for (const char *n : {"Trip/holiday.jpg", "Trip/beach.jpg", "Work/notes.txt"}) {
            const QString path = tmp.filePath(QLatin1String(n));
            QDir().mkpath(QFileInfo(path).absolutePath());
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.close();
            const QUrl u = QUrl::fromLocalFile(path);
            l << RenameItem{u, QFileInfo(path).fileName(), {}, KFileItem(u)};
        }
        RenameManyDialog d(l, {}, nullptr);
        d.setMode(RenameManyDialog::Mode::Formula);
        QCOMPARE(d.base(), QUrl::fromLocalFile(tmp.filePath(QStringLiteral(""))));
        QSignalSpy done(&d, &RenameManyDialog::evaluated);
        d.setFormula(QStringLiteral("TEXT(ROWINFOLDER(),\"00\") & \" - \" & PROPER(NAME)"));
        QVERIFY(!okEnabled(d)); // evaluating
        QVERIFY(done.wait(5000));
        QVERIFY(okEnabled(d));
        const auto p = d.plan();
        QCOMPARE(p.value(0).newName, QStringLiteral("01 - Holiday.jpg"));
        QCOMPARE(p.value(1).newName, QStringLiteral("02 - Beach.jpg"));
        QCOMPARE(p.value(2).newName, QStringLiteral("01 - Notes.txt"));

        d.setFormula(QStringLiteral("1/0"));
        QVERIFY(!okEnabled(d));
        QVERIFY(done.wait(5000));
        for (const auto &r : d.plan()) {
            QCOMPARE(r.status, PlannedRename::Invalid);
            QCOMPARE(r.note, QStringLiteral("#DIV/0!"));
        }
        auto *tr = d.findChild<QTreeWidget *>(QStringLiteral("previewTree"));
        QCOMPARE(tr->topLevelItem(0)->text(2), QStringLiteral("#DIV/0!"));
    }
    void okDisabledWhileEvaluating()
    {
        RenameManyDialog d(items(), {}, nullptr);
        d.setMode(RenameManyDialog::Mode::Formula);
        d.setFormula(QStringLiteral("NAME & \"x\""));
        QVERIFY(!okEnabled(d));
    }
    void typingMakesPlanStaleAtOnce()
    {
        RenameManyDialog d(items(), {}, nullptr);
        d.setMode(RenameManyDialog::Mode::Formula);
        QSignalSpy done(&d, &RenameManyDialog::evaluated);
        d.setFormula(QStringLiteral("NAME & \"x\""));
        QVERIFY(done.wait(5000));
        QVERIFY(okEnabled(d));
        d.findChild<QLineEdit *>(QStringLiteral("formulaEdit"))->setText(QStringLiteral("NAME & \"y\""));
        QVERIFY(!okEnabled(d)); // before the 300 ms debounce
    }
    void setFormulaSameTextTwiceIsNotStuck()
    {
        RenameManyDialog d(items(), {}, nullptr);
        QSignalSpy done(&d, &RenameManyDialog::evaluated);
        d.setFormula(QStringLiteral("NAME & \"x\""));
        QVERIFY(done.wait(5000));
        d.setFormula(QStringLiteral("NAME & \"x\""));
        QVERIFY(done.wait(5000));
        QVERIFY(okEnabled(d));
    }
    void enterInTextFieldDoesNotAccept()
    {
        RenameManyDialog d(items(), existing(), nullptr);
        d.show();
        auto *l = d.findChild<QListWidget *>(QStringLiteral("blockList"));
        l->setCurrentRow(1);
        auto *e = d.findChild<QLineEdit *>(QStringLiteral("textEdit"));
        QVERIFY(e);
        e->setFocus();
        QTest::keyClick(e, Qt::Key_Return);
        QVERIFY(d.isVisible());
    }
    void enterOnToolButtonDoesNotAccept()
    {
        RenameManyDialog d(items(), existing(), nullptr);
        d.show();
        auto *tb = d.findChildren<QToolButton *>().value(0);
        QVERIFY(tb);
        tb->setFocus();
        QTest::keyClick(tb, Qt::Key_Return);
        QCOMPARE(d.result(), int(QDialog::Rejected));
        QVERIFY(d.isVisible());
    }
};

QTEST_MAIN(RenameManyDialogTest)
#include "renamemanydialog_test.moc"

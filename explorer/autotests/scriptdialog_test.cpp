#include "planview.h"
#include "rulelibrary.h"
#include "scriptdialog.h"
#include "tagstore.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QInputDialog>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QTreeWidget>

// Like planrunner_test: the Trash is isolated through XDG_* set before QApplication exists.
using namespace Qt::Literals::StringLiterals;

static const QString realTrash = QDir::homePath() + u"/.local/share/Trash/files"_s;
static const QString kScript =
    u"For Each f In Files(\"Downloads\")\n"
    "  If f.Ext = \"pdf\" And f.Contains(\"invoice\") Then\n"
    "    f.MoveTo \"Documents/Invoices/\" & YEAR(f.Modified)\n"
    "    f.Tag \"Invoice\", \"Orange\"\n"
    "  ElseIf f.Size = 0 Then\n"
    "    f.Trash\n"
    "  End If\n"
    "Next"_s;

class ScriptDialogTest : public QObject
{
    Q_OBJECT
    QTemporaryDir dir{QDir::homePath() + u"/.cache/scriptdialog-XXXXXX"_s};
    QString p(const QString &rel) const { return dir.filePath(rel); }
    void touch(const QString &rel, const QByteArray &data = "x")
    {
        QDir().mkpath(QFileInfo(p(rel)).path());
        QFile f(p(rel));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(data);
    }
    QStringList realTrashNames() const { return QDir(realTrash).entryList(QDir::NoDotAndDotDot | QDir::AllEntries); }
    TagStore tags;
    RuleLibrary lib;
    ScriptDialog *make(const QUrl &base)
    {
        auto *d = new ScriptDialog(base, &tags, &lib, nullptr);
        d->setScript(kScript);
        return d;
    }
    template<class T> static T *child(QObject *o, const char *name) { return o->findChild<T *>(QLatin1String(name)); }
    void built(std::unique_ptr<ScriptDialog> &holder)
    {
        touch(u"Downloads/invoice_mi13.pdf"_s); // tests that ran the plan moved these
        touch(u"Downloads/empty.txt"_s, QByteArray());
        holder.reset(make(QUrl::fromLocalFile(dir.path())));
        holder->buildPlan();
        QTRY_COMPARE(child<QTreeWidget>(holder.get(), "planTree")->topLevelItemCount(), 3);
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(dir.isValid());
        touch(u"Downloads/invoice_mi13.pdf"_s);
        touch(u"Downloads/empty.txt"_s, QByteArray());
        touch(u"Downloads/keep.txt"_s);
    }
    void buildPlanFillsTable()
    {
        std::unique_ptr<ScriptDialog> d;
        built(d);
        auto *tree = child<QTreeWidget>(d.get(), "planTree");
        const QString year = QString::number(QFileInfo(p(u"Downloads/invoice_mi13.pdf"_s)).lastModified().date().year());
        QCOMPARE(tree->topLevelItem(1)->text(0), u"Move → Documents/Invoices/"_s + year);
        QCOMPARE(child<QPushButton>(d.get(), "runPlan")->text(), u"Run plan (3 steps)"_s);
        QVERIFY(child<QPushButton>(d.get(), "runPlan")->isEnabled());
        QCOMPARE(child<QLabel>(d.get(), "scriptStatus")->text(), u"3 steps"_s);
        QVERIFY(QFile::exists(p(u"Downloads/empty.txt"_s))); // building changed nothing
    }
    void parseErrorMovesCursor()
    {
        std::unique_ptr<ScriptDialog> d(make(QUrl::fromLocalFile(dir.path())));
        d->setScript(u"If x Then\nf.Trash"_s);
        d->buildPlan();
        auto *st = child<QLabel>(d.get(), "scriptStatus");
        QTRY_VERIFY2(st->text().startsWith(u"Line 1:"_s), qPrintable(st->text()));
        QCOMPARE(child<QPlainTextEdit>(d.get(), "scriptEdit")->textCursor().blockNumber(), 0);
    }
    void runPlanExecutes()
    {
        if (!TagStore::supported(QUrl::fromLocalFile(dir.path())))
            QSKIP("xattrs not supported here");
        const QStringList before = realTrashNames();
        std::unique_ptr<ScriptDialog> d;
        built(d);
        QSignalSpy fin(d->planView(), &PlanView::runFinished);
        QVERIFY(!child<QPushButton>(d.get(), "undoPlan")->isEnabled());
        QTest::qWait(100);
        QCOMPARE(fin.count(), 0); // never starts by itself
        QVERIFY(QFile::exists(p(u"Downloads/invoice_mi13.pdf"_s)));
        child<QPushButton>(d.get(), "runPlan")->click();
        QTRY_COMPARE(fin.count(), 1);
        QCOMPARE(fin.at(0), (QVariantList{3, 0}));
        QVERIFY(!QFile::exists(p(u"Downloads/invoice_mi13.pdf"_s)));
        QVERIFY(!QFile::exists(p(u"Downloads/empty.txt"_s)));
        QVERIFY(child<QPushButton>(d.get(), "undoPlan")->isEnabled());
        QCOMPARE(realTrashNames(), before);
    }
    void undoPlanRestores()
    {
        if (!TagStore::supported(QUrl::fromLocalFile(dir.path())))
            QSKIP("xattrs not supported here");
        std::unique_ptr<ScriptDialog> d;
        built(d);
        QSignalSpy fin(d->planView(), &PlanView::runFinished);
        child<QPushButton>(d.get(), "runPlan")->click();
        QTRY_COMPARE(fin.count(), 1);
        QSignalSpy und(&d->planView()->runner(), &PlanRunner::undone);
        child<QPushButton>(d.get(), "undoPlan")->click();
        QTRY_COMPARE(und.count(), 1);
        QVERIFY(QFile::exists(p(u"Downloads/invoice_mi13.pdf"_s)));
        QVERIFY(!child<QPushButton>(d.get(), "undoPlan")->isEnabled());
    }
    void nonLocalFolderDisablesBuild()
    {
        std::unique_ptr<ScriptDialog> d(make(QUrl(u"smb://h/s"_s)));
        QVERIFY(!child<QPushButton>(d.get(), "buildPlan")->isEnabled());
        QCOMPARE(child<QLabel>(d.get(), "scriptStatus")->text(), u"Scripts work on local folders only"_s);
    }
    void rejectStopsRun()
    {
        std::unique_ptr<ScriptDialog> d(make(QUrl::fromLocalFile(dir.path())));
        d->setScript(u"For Each f In Files(\"Downloads\")\nf.Trash\nNext"_s);
        d->buildPlan();
        d->reject();
        QSignalSpy spy(d.get(), &QDialog::finished);
        QTest::qWait(200);
        QCOMPARE(child<QTreeWidget>(d.get(), "planTree")->topLevelItemCount(), 0); // finished never arrived
    }
    void saveScriptStoresScriptRule()
    {
        std::unique_ptr<ScriptDialog> d(make(QUrl::fromLocalFile(dir.path())));
        QTimer::singleShot(100, [] {
            auto *in = qobject_cast<QInputDialog *>(QApplication::activeModalWidget());
            if (in) {
                in->setTextValue(u"Invoices"_s);
                in->accept();
            }
        });
        child<QPushButton>(d.get(), "saveScript")->click();
        const auto rules = lib.ofKind(SavedRule::Kind::Script);
        QCOMPARE(rules.size(), 1);
        QCOMPARE(rules[0].name, u"Invoices"_s);
        QCOMPARE(rules[0].text, kScript);
    }
};

int main(int argc, char **argv)
{
    QTemporaryDir xdg(QDir::homePath() + u"/.cache/scriptdialog-xdg-XXXXXX"_s);
    qputenv("XDG_DATA_HOME", QFile::encodeName(xdg.filePath(u"data"_s)));
    qputenv("XDG_CONFIG_HOME", QFile::encodeName(xdg.filePath(u"config"_s)));
    qputenv("XDG_CACHE_HOME", QFile::encodeName(xdg.filePath(u"cache"_s)));
    QApplication app(argc, argv);
    ScriptDialogTest t;
    return QTest::qExec(&t, argc, argv);
}

#include "scriptdialog_test.moc"

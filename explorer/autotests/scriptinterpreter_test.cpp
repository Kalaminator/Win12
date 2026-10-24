#include "fxfile.h"
#include "scriptinterpreter.h"
#include "tagstore.h"
#include <KFileMetaData/UserMetaData>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::Literals::StringLiterals;

class ScriptInterpreterTest : public QObject
{
    Q_OBJECT
    QTemporaryDir dir{QDir::homePath() + u"/.cache/scriptinterp-XXXXXX"_s};

    void touch(const QString &rel, const QByteArray &data = "x")
    {
        const QString p = dir.filePath(rel);
        QDir().mkpath(QFileInfo(p).path());
        QFile f(p);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(data);
    }
    static Script parse(const QString &text)
    {
        FxParseError err;
        const auto s = parseScript(text, &err);
        if (!s)
            qWarning() << "parse error" << err.message << err.line << err.col;
        return s ? *s : Script{};
    }
    ScriptResult run(const QString &text, qint64 max = ScriptInterpreter::DefaultMaxStatements)
    {
        ScriptInterpreter in(dir.path(), max);
        return in.run(parse(text), nullptr);
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QDir().mkpath(QDir::homePath() + u"/.cache"_s);
        QVERIFY(dir.isValid());
        touch(u"Downloads/invoice_mi13.pdf"_s);
        touch(u"Downloads/empty.txt"_s, QByteArray());
        touch(u"Downloads/keep.txt"_s);
        touch(u"Deep/inner/nested.txt"_s);
        touch(u"Deep/top.txt"_s);
        touch(u"Mv/a/x.txt"_s);
        touch(u"Mv/b/x.txt"_s);
        touch(u"Mv2/x.txt"_s);
        touch(u"Out2/x.txt"_s);
        touch(u"Rn/a.txt"_s);
        touch(u"Rn/sub/b.txt"_s);
        touch(u"Hollow/e0.txt"_s, QByteArray());
        QDir().mkpath(dir.filePath(u"Hollow/emptydir"_s));
        for (int i = 0; i < 10; ++i)
            touch(u"Ten/f%1.txt"_s.arg(i));
    }

    void mockupScriptBuildsThreeSteps()
    {
        const ScriptResult r = run(
            u"For Each f In Files(\"Downloads\")\n"
            "  If f.Ext = \"pdf\" And f.Contains(\"invoice\") Then\n"
            "    f.MoveTo \"Documents/Invoices/\" & YEAR(f.Modified)\n"
            "    f.Tag \"Invoice\", \"Orange\"\n"
            "  ElseIf f.Size = 0 Then\n"
            "    f.Trash\n"
            "  End If\n"
            "Next"_s);
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.plan.size(), 3);
        const QString year = QString::number(QFileInfo(dir.filePath(u"Downloads/invoice_mi13.pdf"_s)).lastModified().date().year());
        // rows run in name order (like FormulaRun), so empty.txt comes first
        QCOMPARE(r.plan[0].kind, PlanStep::Trash);
        QCOMPARE(r.plan[0].fileText(), u"empty.txt"_s);
        QCOMPARE(r.plan[0].actionText(), u"Trash"_s);
        QCOMPARE(r.plan[1].kind, PlanStep::Move);
        QCOMPARE(r.plan[1].fileText(), u"invoice_mi13.pdf"_s);
        QCOMPARE(r.plan[1].destFolder, QUrl::fromLocalFile(dir.filePath(u"Documents/Invoices/"_s + year)));
        QCOMPARE(r.plan[1].actionText(), u"Move → Documents/Invoices/"_s + year);
        QCOMPARE(r.plan[1].line, 3);
        QCOMPARE(r.plan[2].kind, PlanStep::Tag);
        QCOMPARE(r.plan[2].fileText(), u"invoice_mi13.pdf"_s);
        QCOMPARE(r.plan[2].tag.word, u"Invoice"_s);
        QCOMPARE(r.plan[2].tag.colour, TagColours::indexOfName(u"OrangeBright"_s));
        QCOMPARE(r.plan[2].actionText(), u"Tag OrangeBright/Invoice"_s);
    }

    void renameThenMoveUsesNewName()
    {
        const ScriptResult r = run(
            u"For Each f In Files(\"Downloads\")\n"
            "  If f.Ext = \"pdf\" Then\n"
            "    f.Rename \"a.pdf\"\n"
            "    f.MoveTo \"Out\"\n"
            "  End If\n"
            "Next"_s);
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.plan.size(), 2);
        QCOMPARE(r.plan[0].kind, PlanStep::Rename);
        QCOMPARE(r.plan[0].actionText(), u"Rename → a.pdf"_s);
        QCOMPARE(r.plan[1].kind, PlanStep::Move);
        QVERIFY2(r.plan[1].source.path().endsWith(u"/Downloads/a.pdf"_s), qPrintable(r.plan[1].source.path()));
    }

    void actionAfterTrashIsAnError()
    {
        const ScriptResult r = run(
            u"For Each f In Files(\"Downloads\")\n"
            "  f.Trash\n"
            "  f.Rename \"z.txt\"\n"
            "Next"_s);
        QVERIFY(!r.ok);
        QVERIFY2(r.error.contains(u"already trashed"_s), qPrintable(r.error));
        QVERIFY2(r.error.contains(u"line 2"_s), qPrintable(r.error));
        QCOMPARE(r.line, 3);
        QCOMPARE(r.plan.size(), 1);
    }

    void renameToExistingIsSkipped()
    {
        const ScriptResult r = run(
            u"For Each f In Files(\"Downloads\")\n"
            "  If f.FullName = \"empty.txt\" Then\n"
            "    f.Rename \"keep.txt\"\n"
            "  End If\n"
            "Next"_s);
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.plan.size(), 1);
        QVERIFY(r.plan[0].skipped);
        QCOMPARE(r.plan[0].note, u"Name exists"_s);
    }

    void samePlannedNameIsSkipped()
    {
        const ScriptResult r = run(
            u"For Each f In Files(\"Downloads\")\n"
            "  f.Rename \"same.txt\"\n"
            "Next"_s);
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.plan.size(), 3);
        QVERIFY(!r.plan[0].skipped);
        QVERIFY(r.plan[1].skipped);
        QCOMPARE(r.plan[1].note, u"Same name planned above"_s);
        QVERIFY(r.plan[2].skipped);
    }

    void invalidNameIsError()
    {
        const ScriptResult r = run(u"For Each f In Files(\"Downloads\")\n  f.Rename \"a/b\"\nNext"_s);
        QVERIFY(!r.ok);
        QCOMPARE(r.line, 2);
    }

    void fourthTagIsSkipped()
    {
        touch(u"Tagged/t.txt"_s);
        const QString p = dir.filePath(u"Tagged/t.txt"_s);
        if (!TagStore::supported(QUrl::fromLocalFile(p)))
            QSKIP("xattr tags not supported here");
        KFileMetaData::UserMetaData(p).setTags({u"Red/a"_s, u"Blue/b"_s, u"Green/c"_s});
        const ScriptResult r = run(
            u"For Each f In Files(\"Tagged\")\n"
            "  f.Tag \"x\", \"Gold\"\n"
            "  f.Tag \"a\", \"Red\"\n"
            "  f.Untag \"nothere\"\n"
            "Next"_s);
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.plan.size(), 3);
        QVERIFY(r.plan[0].skipped);
        QCOMPARE(r.plan[0].note, u"Already has 3 tags"_s);
        QVERIFY(r.plan[1].skipped);
        QVERIFY(r.plan[2].skipped);
        QCOMPARE(r.plan[2].note, u"Tag not present"_s);
    }

    void tagIsSeenByHasTag()
    {
        const ScriptResult r = run(
            u"For Each f In Files(\"Downloads\")\n"
            "  f.Tag \"Invoice\", \"Red\"\n"
            "  If HASTAG(\"Invoice\") Then\n"
            "    f.Untag \"Invoice\"\n"
            "  End If\n"
            "  f.Untag \"Invoice\"\n"
            "Next"_s);
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.plan.size(), 9);
        QCOMPARE(r.plan[1].kind, PlanStep::Untag);
        QVERIFY(!r.plan[1].skipped);
        QCOMPARE(r.plan[1].actionText(), u"Untag Red/Invoice"_s);
        QVERIFY(r.plan[2].skipped);
        QCOMPARE(r.plan[2].note, u"Tag not present"_s);
    }

    void unknownColourIsError()
    {
        const ScriptResult r = run(u"For Each f In Files(\"Downloads\")\n  f.Tag \"x\", \"Plaid\"\nNext"_s);
        QVERIFY(!r.ok);
        QCOMPARE(r.error, u"Unknown colour Plaid"_s);
        QCOMPARE(r.line, 2);
    }

    void moveOntoOwnFolderIsSkipped()
    {
        const ScriptResult r = run(u"For Each f In Files(\"Downloads\")\n  f.MoveTo \"Downloads\"\nNext"_s);
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.plan.size(), 3);
        QVERIFY(r.plan[0].skipped);
        QCOMPARE(r.plan[0].note, u"Already there"_s);
    }

    void movesReserveTheDestinationName()
    {
        const ScriptResult r = run(u"For Each f In Files(\"Mv\", Subfolders:=True)\n  f.MoveTo \"Out\"\nNext"_s);
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.plan.size(), 2);
        QVERIFY(!r.plan[0].skipped);
        QVERIFY(r.plan[1].skipped);
        QCOMPARE(r.plan[1].note, u"Same name planned above"_s);
        const ScriptResult onDisk = run(u"For Each f In Files(\"Mv2\")\n  f.MoveTo \"Out2\"\n  f.CopyTo \"Out2\"\nNext"_s);
        QVERIFY2(onDisk.ok, qPrintable(onDisk.error));
        QCOMPARE(onDisk.plan.size(), 2);
        QVERIFY(onDisk.plan[0].skipped);
        QCOMPARE(onDisk.plan[0].note, u"Name exists"_s);
        QVERIFY(onDisk.plan[1].skipped);
        QCOMPARE(onDisk.plan[1].note, u"Name exists"_s);
    }

    void renameCollidingWithMovedInFileIsSkipped()
    {
        const ScriptResult r = run(u"For Each f In Files(\"Rn/sub\")\n  f.MoveTo \"Rn\"\nNext\n"
                                   "For Each g In Files(\"Rn\")\n  g.Rename \"b.txt\"\nNext"_s);
        QVERIFY2(r.ok, qPrintable(r.error));
        QVERIFY(!r.plan[0].skipped);
        QVERIFY(r.plan.last().skipped);
        QCOMPARE(r.plan.last().note, u"Same name planned above"_s);
    }

    void untagUnderstandsColourSlashWord()
    {
        const ScriptResult r = run(u"For Each f In Files(\"Downloads\")\n  f.Tag \"Orange/Invoice\"\n  f.Untag \"Orange/Invoice\"\nNext"_s);
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.plan[1].kind, PlanStep::Untag);
        QVERIFY(!r.plan[1].skipped);
        QCOMPARE(r.plan[0].actionText(), u"Tag OrangeBright/Invoice"_s);
        QCOMPARE(r.plan[1].actionText(), u"Untag OrangeBright/Invoice"_s);
    }

    void filesYieldsFilesOnly()
    {
        const ScriptResult r = run(u"For Each f In Files(\"Hollow\", Subfolders:=True)\n  If f.Size = 0 Then\n    f.Trash\n  End If\nNext"_s);
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.plan.size(), 1);
        QCOMPARE(r.plan[0].fileText(), u"e0.txt"_s);
    }

    void listerGetsTheCancelFlag()
    {
        ScriptInterpreter in(dir.path());
        std::atomic_bool cancel{false};
        const std::atomic_bool *seen = nullptr;
        in.setLister([&](const QString &, bool, QString *, const std::atomic_bool *c) {
            seen = c;
            return QList<std::shared_ptr<FxFile>>{};
        });
        in.run(parse(u"For Each f In Files(\"Virtual\")\n  f.Trash\nNext"_s), &cancel);
        QCOMPARE(seen, &cancel);
    }

    void stepLimit()
    {
        const ScriptResult r = run(u"For Each f In Files(\"Ten\")\n  f.Trash\nNext"_s, 5);
        QVERIFY(!r.ok);
        QVERIFY2(r.error.startsWith(u"Script stopped after 5 statements"_s), qPrintable(r.error));
        QVERIFY(!r.plan.isEmpty());
        QCOMPARE(r.plan.size(), 4);
    }

    void cancelStops()
    {
        ScriptInterpreter in(dir.path());
        in.setLister([&](const QString &, bool, QString *, const std::atomic_bool *) {
            QList<std::shared_ptr<FxFile>> rows;
            for (int i = 0; i < 1000; ++i) {
                auto f = std::make_shared<FxFile>();
                f->path = dir.filePath(u"Virtual/v%1.txt"_s.arg(i));
                rows << f;
            }
            return rows;
        });
        std::atomic_bool cancel{true};
        const ScriptResult r = in.run(parse(u"For Each f In Files(\"Virtual\")\n  f.Trash\nNext"_s), &cancel);
        QVERIFY(!r.ok);
        QCOMPARE(r.error, u"Stopped"_s);
        QVERIFY(r.plan.isEmpty());

        cancel = false;
        const ScriptResult all = in.run(parse(u"For Each f In Files(\"Virtual\")\n  f.Trash\nNext"_s), &cancel);
        QVERIFY2(all.ok, qPrintable(all.error));
        QCOMPARE(all.plan.size(), 1000);
    }

    void missingFolderIsError()
    {
        const ScriptResult r = run(u"For Each f In Files(\"Nope\")\n  f.Trash\nNext"_s);
        QVERIFY(!r.ok);
        QCOMPARE(r.line, 1);
        QVERIFY(!r.error.isEmpty());
    }

    void variablesAndExitFor()
    {
        const ScriptResult r = run(
            u"n = 0\n"
            "For Each f In Files(\"Downloads\")\n"
            "  n = n + 1\n"
            "  f.Tag \"Red\"\n"
            "  If n = 2 Then\n"
            "    Exit For\n"
            "  End If\n"
            "Next"_s);
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.plan.size(), 2);
        QCOMPARE(r.plan[0].kind, PlanStep::Tag);
        QCOMPARE(r.plan[1].kind, PlanStep::Tag);
    }

    void subfoldersFlag()
    {
        auto names = [](const ScriptResult &r) {
            QStringList n;
            for (const PlanStep &s : r.plan)
                n << s.fileText();
            return n;
        };
        const ScriptResult flat = run(u"For Each f In Files(\"Deep\")\n  f.Trash\nNext"_s);
        QVERIFY2(flat.ok, qPrintable(flat.error));
        QCOMPARE(names(flat), (QStringList{u"top.txt"_s}));
        const ScriptResult deep = run(u"For Each f In Files(\"Deep\", Subfolders:=True)\n  f.CopyTo \"Out\"\nNext"_s);
        QVERIFY2(deep.ok, qPrintable(deep.error));
        QCOMPARE(names(deep), (QStringList{u"nested.txt"_s, u"top.txt"_s}));
        QCOMPARE(deep.plan[0].actionText(), u"Copy → Out"_s);
    }

    void statementsCounted()
    {
        const ScriptResult r = run(u"For Each f In Files(\"Downloads\")\n  f.Trash\nNext"_s);
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.statements, qint64(1 + 3));
    }

    void progressHookEveryN()
    {
        ScriptInterpreter in(dir.path());
        QList<qint64> seen;
        in.setProgress([&](qint64 n, int) { seen << n; }, 2);
        const ScriptResult r = in.run(parse(u"For Each f In Files(\"Downloads\")\n  f.Trash\nNext"_s), nullptr);
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(seen, (QList<qint64>{2, 4}));
    }

    void tildeAndOutsideBaseShowFullPath()
    {
        const ScriptResult r = run(u"For Each f In Files(\"Downloads\")\n  f.CopyTo \"~/elsewhere\"\nNext"_s);
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.plan[0].destFolder, QUrl::fromLocalFile(QDir::homePath() + u"/elsewhere"_s));
        QCOMPARE(r.plan[0].actionText(), u"Copy → "_s + QDir::homePath() + u"/elsewhere"_s);
    }
};

QTEST_GUILESS_MAIN(ScriptInterpreterTest)
#include "scriptinterpreter_test.moc"

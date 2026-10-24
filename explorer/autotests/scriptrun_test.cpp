#include "scriptrun.h"
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::Literals::StringLiterals;

class ScriptRunTest : public QObject
{
    Q_OBJECT
    QTemporaryDir dir{QDir::homePath() + u"/.cache/scriptrun-XXXXXX"_s};
    void touch(const QString &rel, const QByteArray &data = "x")
    {
        const QString p = dir.filePath(rel);
        QDir().mkpath(QFileInfo(p).path());
        QFile f(p);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(data);
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
        for (int i = 0; i < 5000; ++i)
            touch(u"Many/f%1.txt"_s.arg(i));
    }

    void parseErrorFinishesImmediately()
    {
        ScriptRun run;
        QSignalSpy spy(&run, &ScriptRun::finished);
        QElapsedTimer t;
        t.start();
        run.start(u"For Each"_s, dir.path());
        QVERIFY(spy.wait(300));
        QVERIFY(t.elapsed() < 300);
        const auto r = spy.at(0).at(0).value<ScriptResult>();
        QVERIFY(!r.ok);
        QCOMPARE(r.line, 1);
        QVERIFY(!run.isRunning());
    }

    void mockupScriptProducesPlan()
    {
        ScriptRun run;
        QSignalSpy spy(&run, &ScriptRun::finished);
        run.start(u"For Each f In Files(\"Downloads\")\n"
                  "  If f.Ext = \"pdf\" And f.Contains(\"invoice\") Then\n"
                  "    f.MoveTo \"Documents/Invoices/\" & YEAR(f.Modified)\n"
                  "    f.Tag \"Invoice\", \"Orange\"\n"
                  "  ElseIf f.Size = 0 Then\n"
                  "    f.Trash\n"
                  "  End If\n"
                  "Next"_s, dir.path());
        QVERIFY(run.isRunning());
        QVERIFY(spy.wait(5000));
        const auto r = spy.at(0).at(0).value<ScriptResult>();
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.plan.size(), 3);
    }

    void progressIsThrottled()
    {
        ScriptRun run;
        QSignalSpy prog(&run, &ScriptRun::progress);
        QSignalSpy spy(&run, &ScriptRun::finished);
        QElapsedTimer t;
        t.start();
        run.start(u"For Each f In Files(\"Many\")\n  f.Trash\nNext"_s, dir.path());
        QVERIFY(spy.wait(20000));
        QVERIFY(spy.at(0).at(0).value<ScriptResult>().ok);
        QVERIFY(prog.count() >= 1);
        QVERIFY2(prog.count() <= 1 + t.elapsed() / 1000.0 * 4, qPrintable(QString::number(prog.count())));
    }

    void stopEndsWithoutFinished()
    {
        ScriptRun run;
        QSignalSpy spy(&run, &ScriptRun::finished);
        run.start(u"For Each f In Files(\"Many\")\n  f.Trash\nNext"_s, dir.path());
        run.stop();
        QVERIFY(!run.isRunning());
        QVERIFY(!spy.wait(300));
    }
};

QTEST_MAIN(ScriptRunTest)
#include "scriptrun_test.moc"

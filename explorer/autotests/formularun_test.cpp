#include "formularun.h"
#include "fxeval.h"
#include "fxfile.h"
#include "fxparser.h"
#include "tagstore.h"
#include <QDir>
#include <QFile>
#include <QImage>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::Literals::StringLiterals;

class FormulaRunTest : public QObject
{
    Q_OBJECT
    // ~/.cache, not /tmp: tmpfs may lack user.* xattrs (tagsRead)
    QTemporaryDir dir{QDir::homePath() + u"/.cache/formularun-XXXXXX"_s};

    static void touch(const QString &path, const QByteArray &data = {})
    {
        QDir().mkpath(QFileInfo(path).path());
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(data);
    }
    static FxExpr fx(const QString &text)
    {
        FxParseError err;
        const auto e = FxParser::parseFormula(text, &err);
        if (!e)
            qFatal("parse error in %s: %s", qPrintable(text), qPrintable(err.message));
        return *e;
    }
    QUrl base() const { return QUrl::fromLocalFile(dir.path()); }
    QUrl url(const QString &rel) const { return QUrl::fromLocalFile(dir.filePath(rel)); }

    // Runs `go`, waits for finished; returns every result in emission order
    static QList<FxRowResult> collect(FormulaRun &run, const std::function<void()> &go, int *batches = nullptr, bool *ok = nullptr)
    {
        QList<FxRowResult> all;
        int n = 0;
        auto c = connect(&run, &FormulaRun::results, &run, [&](const QList<FxRowResult> &r) {
            all += r;
            ++n;
        });
        QSignalSpy done(&run, &FormulaRun::finished);
        go();
        if (!done.wait(10000))
            qWarning() << "no finished";
        disconnect(c);
        if (batches)
            *batches = n;
        if (ok)
            *ok = !done.isEmpty() && done.first().at(0).toBool();
        return all;
    }
    static QStringList texts(const QList<FxRowResult> &rs)
    {
        QStringList out;
        for (const FxRowResult &r : rs)
            out << r.value.toText();
        return out;
    }
    static const FxRowResult *byName(const QList<FxRowResult> &rs, const QString &name)
    {
        for (const FxRowResult &r : rs)
            if (r.item.name() == name)
                return &r;
        return nullptr;
    }

private Q_SLOTS:
    void tagsAreLoadedOnTheWorker()
    {
        QTemporaryDir own{QDir::homePath() + u"/.cache/formularun-XXXXXX"_s};
        touch(own.filePath(u"a.txt"_s));
        FormulaRun run;
        const auto rs = collect(run, [&] { run.start(QUrl::fromLocalFile(own.path()), false, fx(u"SIZE>0"_s)); });
        QCOMPARE(rs.size(), 1);
        QVERIFY(rs.first().row->tagsLoaded());
    }
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QDir().mkpath(QDir::homePath() + u"/.cache"_s);
        QVERIFY(dir.isValid());
        touch(dir.filePath(u"a.txt"_s), "abc");
        touch(dir.filePath(u"sub/b.txt"_s), "b");
        QImage img(20, 10, QImage::Format_RGB32);
        img.fill(Qt::red);
        QVERIFY(img.save(dir.filePath(u"sub/c.png"_s), "PNG"));
        touch(dir.filePath(u"Zz/d.txt"_s), "dd");
    }

    void rowsAreSortedAndNumbered()
    {
        FormulaRun run;
        const auto rs = collect(run, [&] { run.start(base(), true, fx(u"ROW() & \":\" & FULLNAME"_s)); });
        QCOMPARE(texts(rs), (QStringList{u"1:a.txt"_s, u"2:sub"_s, u"3:b.txt"_s, u"4:c.png"_s, u"5:Zz"_s, u"6:d.txt"_s}));
        QCOMPARE(byName(rs, u"b.txt"_s)->row->depth, 1);
        QCOMPARE(byName(rs, u"a.txt"_s)->row->depth, 0);
        QVERIFY(byName(rs, u"sub"_s)->row->isFolder);
    }

    void rowInFolderRestarts()
    {
        FormulaRun run;
        const auto rs = collect(run, [&] { run.start(base(), true, fx(u"ROWINFOLDER()"_s)); });
        QCOMPARE(texts(rs), (QStringList{u"1"_s, u"2"_s, u"1"_s, u"2"_s, u"3"_s, u"1"_s}));
    }

    void photoColumns()
    {
        FormulaRun run;
        const auto rs = collect(run, [&] { run.start(base(), true, fx(u"WIDTH & \"x\" & HEIGHT"_s)); });
        QCOMPARE(byName(rs, u"c.png"_s)->value.toText(), u"20x10"_s);
        QCOMPARE(byName(rs, u"a.txt"_s)->value.toText(), u"0x0"_s);
    }

    void tagsEmptyWhereUnsupported()
    {
        const auto row = fxFileFor(KFileItem(QUrl(u"smb://h/s/f.txt"_s)), QUrl(u"smb://h/s"_s), 1, 1);
        QVERIFY(row->tags().isEmpty());
        QCOMPARE(row->path, u"smb://h/s/f.txt"_s);
        QCOMPARE(row->fullName(), u"f.txt"_s);
        FxContext ctx;
        const FxValue v = fxEvaluate(fx(u"HASTAG(\"x\")"_s), row, ctx);
        QCOMPARE(v, FxValue::boolean(false));
    }

    void tagsRead()
    {
        if (!TagStore::supported(base()))
            QSKIP("xattr tags not supported here");
        TagStore s;
        QCOMPARE(s.add({url(u"a.txt"_s)}, Tag::decode(u"Red/Xiaomi"_s)), TagStore::Error::None);
        const auto row = fxFileFor(KFileItem(url(u"a.txt"_s)), base(), 1, 1);
        FxContext ctx;
        QCOMPARE(fxEvaluate(fx(u"HASTAG(\"xiaomi\")"_s), row, ctx), FxValue::boolean(true));
        s.remove({url(u"a.txt"_s)}, Tag::decode(u"Red/Xiaomi"_s));
    }

    void startOnKeepsOrder()
    {
        FormulaRun run;
        const KFileItemList items{KFileItem(url(u"Zz/d.txt"_s)), KFileItem(url(u"a.txt"_s))};
        const auto rs = collect(run, [&] { run.startOn(items, base(), fx(u"FULLNAME"_s)); });
        QCOMPARE(texts(rs), (QStringList{u"d.txt"_s, u"a.txt"_s}));
        QCOMPARE(rs.at(0).row->row, 1);
        QCOMPARE(rs.at(1).row->row, 2);
        QCOMPARE(rs.at(0).row->depth, 1);
    }

    void batchesNotPerFile()
    {
        QTemporaryDir many;
        for (int i = 0; i < 300; ++i)
            touch(many.filePath(u"f%1.txt"_s.arg(i)));
        FormulaRun run;
        int batches = 0;
        const auto rs = collect(run, [&] { run.start(QUrl::fromLocalFile(many.path()), false, fx(u"ROW()"_s)); }, &batches);
        QCOMPARE(rs.size(), 300);
        QVERIFY2(batches <= 2, qPrintable(QString::number(batches)));
        // numeric collation: f2 before f10
        QCOMPARE(rs.at(2).item.name(), u"f2.txt"_s);
        QCOMPARE(rs.at(10).item.name(), u"f10.txt"_s);
    }

    void stopEndsWithoutFinished()
    {
        FormulaRun run;
        QSignalSpy done(&run, &FormulaRun::finished);
        QSignalSpy res(&run, &FormulaRun::results);
        run.start(base(), true, fx(u"FULLNAME"_s));
        QVERIFY(run.isRunning());
        run.stop();
        QVERIFY(!run.isRunning());
        QTest::qWait(300);
        QCOMPARE(done.count(), 0);
        QCOMPARE(res.count(), 0);
    }

    void stopDuringEvaluation()
    {
        FormulaRun run;
        QSignalSpy done(&run, &FormulaRun::finished);
        run.startOn({KFileItem(url(u"a.txt"_s))}, base(), fx(u"FULLNAME"_s));
        run.stop();
        QTest::qWait(300);
        QCOMPARE(done.count(), 0);
        QVERIFY(!run.isRunning());
    }

    void missingFolderFinishesWithError()
    {
        FormulaRun run;
        QSignalSpy done(&run, &FormulaRun::finished);
        run.start(url(u"nope"_s), true, fx(u"FULLNAME"_s));
        QVERIFY(done.wait(5000));
        QCOMPARE(done.first().at(0).toBool(), false);
        QVERIFY(!done.first().at(1).toString().isEmpty());
        QVERIFY(!run.isRunning());
    }

    void errorRowsArePerRow()
    {
        FormulaRun run;
        bool ok = false;
        const auto rs = collect(run, [&] { run.start(base(), true, fx(u"1/(SIZE-3)"_s)); }, nullptr, &ok);
        QVERIFY(ok);
        QCOMPARE(byName(rs, u"a.txt"_s)->value.toText(), u"#DIV/0!"_s);
        QCOMPARE(byName(rs, u"b.txt"_s)->value.type, FxValue::Type::Number);
        QVERIFY(!run.isRunning());
    }
};

QTEST_MAIN(FormulaRunTest)
#include "formularun_test.moc"

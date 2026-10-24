#include "planrunner.h"
#include "tagstore.h"
#include <KIO/FileUndoManager>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

// KIO::trash must never reach the user's real Trash: main() points XDG_DATA_HOME/CONFIG/CACHE at a temporary
// dir before QApplication exists, so the trash worker (thread or process) inherits it. The test tree lives in
// ~/.cache (same device as that dir) so trashing uses the home trash there, not a /tmp/.Trash-uid.
using namespace Qt::StringLiterals;

static const QString realTrash = QDir::homePath() + QStringLiteral("/.local/share/Trash/files");

class PlanRunnerTest : public QObject
{
    Q_OBJECT
    QTemporaryDir dir{QDir::homePath() + QStringLiteral("/.cache/planrunner-XXXXXX")};
    QString p(const QString &rel) const { return dir.filePath(rel); }
    QUrl u(const QString &rel) const { return QUrl::fromLocalFile(p(rel)); }
    QUrl file(const QString &rel)
    {
        QDir().mkpath(QFileInfo(p(rel)).path());
        QFile f(p(rel));
        (void)f.open(QIODevice::WriteOnly);
        f.write("x");
        return u(rel);
    }
    static Tag T(const char *s) { return Tag::decode(QString::fromLatin1(s)); }
    static PlanStep step(PlanStep::Kind k, const QUrl &src)
    {
        PlanStep s;
        s.kind = k;
        s.source = src;
        return s;
    }
    static PlanStep to(PlanStep::Kind k, const QUrl &src, const QUrl &dest)
    {
        PlanStep s = step(k, src);
        s.destFolder = dest;
        return s;
    }
    static PlanStep rename(const QUrl &src, const QString &name)
    {
        PlanStep s = step(PlanStep::Rename, src);
        s.newName = name;
        return s;
    }
    static PlanStep tag(PlanStep::Kind k, const QUrl &src, const Tag &t)
    {
        PlanStep s = step(k, src);
        s.tag = t;
        return s;
    }
    bool tagsSupported() const { return TagStore::supported(QUrl::fromLocalFile(dir.path())); }
    QStringList realTrashNames() const { return QDir(realTrash).entryList(QDir::NoDotAndDotDot | QDir::AllEntries); }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(dir.isValid());
    }
    void moveCreatesMissingFolder()
    {
        TagStore tags;
        PlanRunner r(nullptr, &tags);
        QSignalSpy fin(&r, &PlanRunner::finished);
        r.run({to(PlanStep::Move, file(u"m/a.txt"_s), u(u"m/out/2026"_s))});
        QVERIFY(r.isRunning());
        QTRY_COMPARE(fin.count(), 1);
        QVERIFY(QFile::exists(p(u"m/out/2026/a.txt"_s)));
        QVERIFY(!QFile::exists(p(u"m/a.txt"_s)));
        QCOMPARE(fin.at(0), (QVariantList{1, 0, QString()}));
        QVERIFY(!r.isRunning());
        QCOMPARE(r.executedCount(), 1);
    }
    void copyRenameTrashTag()
    {
        const QStringList before = realTrashNames();
        TagStore tags;
        PlanRunner r(nullptr, &tags);
        QSignalSpy fin(&r, &PlanRunner::finished);
        QList<PlanStep> plan{to(PlanStep::Copy, file(u"c/a.txt"_s), u(u"c/copies"_s)), rename(file(u"c/b.txt"_s), u"b2.txt"_s),
                             step(PlanStep::Trash, file(u"c/planrunner-trash-me.txt"_s))};
        const bool tagging = tagsSupported();
        if (tagging)
            plan << tag(PlanStep::Tag, file(u"c/d.txt"_s), T("Red/x"));
        r.run(plan);
        QTRY_COMPARE(fin.count(), 1);
        QCOMPARE(fin.at(0), (QVariantList{int(plan.size()), 0, QString()}));
        QVERIFY(QFile::exists(p(u"c/a.txt"_s)));
        QVERIFY(QFile::exists(p(u"c/copies/a.txt"_s)));
        QVERIFY(QFile::exists(p(u"c/b2.txt"_s)));
        QVERIFY(!QFile::exists(p(u"c/b.txt"_s)));
        QVERIFY(!QFile::exists(p(u"c/planrunner-trash-me.txt"_s)));
        if (tagging)
            QCOMPARE(TagStore::read(u(u"c/d.txt"_s)), QList<Tag>{T("Red/x")});
        QTRY_VERIFY(KIO::FileUndoManager::self()->isUndoAvailable());
        QCOMPARE(realTrashNames(), before); // the isolated XDG_DATA_HOME took it
        if (!tagging)
            QSKIP("xattr tags not supported here; the other steps passed");
    }
    void skippedRowsAreNotRun()
    {
        TagStore tags;
        PlanRunner r(nullptr, &tags);
        QSignalSpy fin(&r, &PlanRunner::finished);
        QSignalSpy prog(&r, &PlanRunner::progress);
        PlanStep skip = rename(file(u"s/a.txt"_s), u"never.txt"_s);
        skip.skipped = true;
        r.run({skip, rename(file(u"s/b.txt"_s), u"b2.txt"_s)});
        QTRY_COMPARE(fin.count(), 1);
        QCOMPARE(fin.at(0).at(0).toInt(), 1);
        QCOMPARE(fin.at(0).at(1).toInt(), 0);
        QCOMPARE(prog.count(), 1);
        QCOMPARE(prog.at(0), (QVariantList{0, 1}));
        QVERIFY(QFile::exists(p(u"s/a.txt"_s)));
        QVERIFY(!QFile::exists(p(u"s/never.txt"_s)));
        QVERIFY(QFile::exists(p(u"s/b2.txt"_s)));
    }
    void failedStepContinues()
    {
        TagStore tags;
        PlanRunner r(nullptr, &tags);
        QSignalSpy fin(&r, &PlanRunner::finished);
        file(u"f/taken.txt"_s);
        r.run({rename(file(u"f/a.txt"_s), u"taken.txt"_s), to(PlanStep::Move, file(u"f/b.txt"_s), u(u"f/out"_s))});
        QTRY_COMPARE(fin.count(), 1);
        QCOMPARE(fin.at(0).at(0).toInt(), 1);
        QCOMPARE(fin.at(0).at(1).toInt(), 1);
        QVERIFY(!fin.at(0).at(2).toString().isEmpty());
        QVERIFY(QFile::exists(p(u"f/a.txt"_s)));
        QVERIFY(QFile::exists(p(u"f/out/b.txt"_s)));
        QCOMPARE(r.executedCount(), 1);
    }
    void cancelStopsAfterCurrentStep()
    {
        TagStore tags;
        PlanRunner r(nullptr, &tags);
        QSignalSpy fin(&r, &PlanRunner::finished);
        connect(&r, &PlanRunner::progress, &r, [&r](int done) {
            if (done == 0)
                r.cancel();
        });
        const QUrl out = u(u"k/out"_s);
        r.run({to(PlanStep::Move, file(u"k/1.txt"_s), out), to(PlanStep::Move, file(u"k/2.txt"_s), out), to(PlanStep::Move, file(u"k/3.txt"_s), out)});
        QTRY_COMPARE(fin.count(), 1);
        QCOMPARE(fin.at(0).at(0).toInt(), 1);
        QCOMPARE(fin.at(0).at(1).toInt(), 0);
        QVERIFY(QFile::exists(p(u"k/out/1.txt"_s)));
        QVERIFY(QFile::exists(p(u"k/2.txt"_s)));
        QVERIFY(QFile::exists(p(u"k/3.txt"_s)));
        QVERIFY(!QFile::exists(p(u"k/out/2.txt"_s)));
    }
    void undoReversesExecuted()
    {
        if (!tagsSupported())
            QSKIP("xattr tags not supported here");
        TagStore tags;
        PlanRunner r(nullptr, &tags);
        QSignalSpy fin(&r, &PlanRunner::finished);
        QSignalSpy und(&r, &PlanRunner::undone);
        file(u"u/c.txt"_s);
        r.run({to(PlanStep::Move, file(u"u/a.txt"_s), u(u"u/out/x"_s)), rename(file(u"u/b.txt"_s), u"b2.txt"_s),
               tag(PlanStep::Tag, u(u"u/c.txt"_s), T("Blue/y"))});
        QTRY_COMPARE(fin.count(), 1);
        QCOMPARE(r.executedCount(), 3);
        QVERIFY(QFile::exists(p(u"u/out/x/a.txt"_s)));
        r.undo();
        QTRY_COMPARE(und.count(), 1);
        QCOMPARE(und.at(0), (QVariantList{3, QString()}));
        QCOMPARE(r.executedCount(), 0);
        QVERIFY(!r.isRunning());
        QVERIFY(QFile::exists(p(u"u/a.txt"_s)));
        QVERIFY(!QFile::exists(p(u"u/out/x/a.txt"_s)));
        QVERIFY(QFile::exists(p(u"u/b.txt"_s)));
        QVERIFY(!QFile::exists(p(u"u/b2.txt"_s)));
        QVERIFY(TagStore::read(u(u"u/c.txt"_s)).isEmpty());
    }
    void undoNoteForTrash()
    {
        TagStore tags;
        PlanRunner r(nullptr, &tags);
        QSignalSpy fin(&r, &PlanRunner::finished);
        QSignalSpy und(&r, &PlanRunner::undone);
        QVERIFY(r.undoNote().isEmpty());
        r.run({step(PlanStep::Trash, file(u"t/gone.txt"_s))});
        QTRY_COMPARE(fin.count(), 1);
        QCOMPARE(r.undoNote(), u"1 trashed item stays in the Trash"_s);
        r.undo();
        QTRY_COMPARE(und.count(), 1);
        QCOMPARE(und.at(0).at(0).toInt(), 0);
        QVERIFY(!QFile::exists(p(u"t/gone.txt"_s)));
        QCOMPARE(r.executedCount(), 0);
    }
};

int main(int argc, char **argv)
{
    QTemporaryDir xdg(QDir::homePath() + QStringLiteral("/.cache/planrunner-xdg-XXXXXX"));
    qputenv("XDG_DATA_HOME", QFile::encodeName(xdg.filePath(QStringLiteral("data"))));
    qputenv("XDG_CONFIG_HOME", QFile::encodeName(xdg.filePath(QStringLiteral("config"))));
    qputenv("XDG_CACHE_HOME", QFile::encodeName(xdg.filePath(QStringLiteral("cache"))));
    QApplication app(argc, argv);
    PlanRunnerTest t;
    return QTest::qExec(&t, argc, argv);
}

#include "planrunner_test.moc"

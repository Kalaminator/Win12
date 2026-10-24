#include "scriptinterpreter.h"
#include "fxeval.h"
#include "fxfile.h"
#include <KFileItem>
#include <KLocalizedString>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QHash>
#include <QLocale>
#include <QSet>

using namespace Qt::Literals::StringLiterals;

static QList<std::shared_ptr<FxFile>> listFolder(const QString &abs, bool subfolders, QString *error, const std::atomic_bool *cancel)
{
    if (!QFileInfo(abs).isDir()) {
        *error = i18n("Folder not found: %1", abs);
        return {};
    }
    const QUrl base = QUrl::fromLocalFile(abs);
    QDirIterator it(abs, QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot, subfolders ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags);
    QList<QUrl> urls;
    QList<QStringList> parts;
    while (it.hasNext()) {
        if (cancel && *cancel)
            return {};
        const QString path = it.next();
        if (it.fileInfo().isDir() && !it.fileInfo().isSymLink())
            continue; // Files() yields files only; folders are walked, not listed
        urls << QUrl::fromLocalFile(path);
        parts << fxRelativePath(urls.last(), base).split(u'/');
    }
    QList<std::shared_ptr<FxFile>> rows;
    rows.reserve(urls.size());
    QHash<QString, int> inFolder; // parent relative path -> rows so far
    int row = 0;
    for (const int i : fxRowOrder(parts)) {
        const QStringList &p = parts[i];
        const int rowInFolder = ++inFolder[p.mid(0, p.size() - 1).join(u'/')];
        rows << fxFileFor(KFileItem(urls[i]), base, ++row, rowInFolder);
    }
    return rows;
}

namespace
{
enum class Flow { Next, ExitFor, Fail };

// One run's state. The plan is built here; files are never touched.
struct Run {
    const QString &base;
    const qint64 max;
    const ScriptInterpreter::Lister &lister;
    const std::atomic_bool *cancel;
    ScriptResult r;
    QHash<QString, FxValue> vars; // lower-case names
    FxContext ctx;
    std::shared_ptr<FxFile> row; // the innermost For Each file: bare columns and HASTAG read it
    QSet<QString> plannedNames; // paths that planned renames, moves and copies lead to
    QHash<QString, int> trashed; // path -> line of its Trash
    std::function<void(qint64, int)> progress;
    int progressEvery = 1000;

    bool fail(const QString &message, int line)
    {
        r.ok = false;
        r.error = message;
        r.line = line;
        return false;
    }
    bool stopped(int line)
    {
        if (!cancel || !*cancel)
            return false;
        fail(i18n("Stopped"), line);
        return true;
    }

    // Every executed statement passes here once (the place for a progress hook).
    bool step(int line)
    {
        if (stopped(line))
            return false;
        if (r.statements >= max)
            return fail(i18n("Script stopped after %1 statements (endless loop?)", QLocale().toString(max)), line);
        ++r.statements;
        if (progress && r.statements % progressEvery == 0)
            progress(r.statements, int(r.plan.size()));
        return true;
    }

    // Files() and MoveTo/CopyTo paths: "~" expands, relative ones are below the base folder
    QString resolve(QString p) const
    {
        if (p == u"~"_s || p.startsWith(u"~/"_s))
            p = QDir::homePath() + p.mid(1);
        return QDir::cleanPath(QDir(base).absoluteFilePath(p));
    }

    bool value(const FxExpr &e, int line, FxValue *out)
    {
        *out = fxEvaluate(e, row, ctx);
        return !out->isError() || fail(i18n("%1 in this statement", out->toText()), line);
    }
    bool condition(const FxExpr &e, int line, bool *out)
    {
        FxValue v;
        if (!value(e, line, &v))
            return false;
        bool ok;
        *out = v.truthy(&ok);
        return ok || fail(i18n("Expected TRUE or FALSE, got %1", v.toText()), line);
    }

    Flow block(const std::vector<ScriptStmt> &body)
    {
        for (const ScriptStmt &st : body)
            if (const Flow f = statement(st); f != Flow::Next)
                return f;
        return Flow::Next;
    }

    Flow statement(const ScriptStmt &st)
    {
        if (!step(st.line))
            return Flow::Fail;
        switch (st.kind) {
        case ScriptStmt::Assign:
            vars.insert(st.var.toLower(), st.expr ? fxEvaluate(st.expr, row, ctx) : FxValue::empty());
            return Flow::Next;
        case ScriptStmt::ExitFor:
            return Flow::ExitFor;
        case ScriptStmt::If:
            for (const auto &[cond, body] : st.branches) {
                bool t;
                if (!condition(cond, cond->line, &t))
                    return Flow::Fail;
                if (t)
                    return block(body);
            }
            return block(st.elseBody);
        case ScriptStmt::ForEach:
            return forEach(st);
        case ScriptStmt::Action:
            return action(st) ? Flow::Next : Flow::Fail;
        }
        return Flow::Next;
    }

    Flow forEach(const ScriptStmt &st)
    {
        const FxNode &call = *st.expr; // Files(folder[, subfolders])
        FxValue folder;
        bool subfolders = false;
        if (call.args.empty() && !fail(i18n("Files needs a folder"), st.line))
            return Flow::Fail;
        if (!value(call.args[0], st.line, &folder) || (call.args.size() > 1 && !condition(call.args[1], st.line, &subfolders)))
            return Flow::Fail;
        QString error;
        const QList<std::shared_ptr<FxFile>> rows = lister(resolve(folder.toText()), subfolders, &error, cancel);
        if (!error.isEmpty() && !fail(error, st.line))
            return Flow::Fail;
        const std::shared_ptr<FxFile> outer = row;
        Flow f = Flow::Next;
        for (const std::shared_ptr<FxFile> &file : rows) {
            if (stopped(st.line)) { // an empty body checks too
                f = Flow::Fail;
                break;
            }
            vars.insert(st.var.toLower(), FxValue::fileRef(file));
            row = file;
            if ((f = block(st.body)) != Flow::Next)
                break;
        }
        row = outer;
        return f == Flow::ExitFor ? Flow::Next : f;
    }

    // Tag: "Invoice", "Orange" | "Orange/Invoice" | "Orange" (colour only). Untag: "Invoice" | "Orange/Invoice"
    // (a bare word matches any colour: colour = -1). The colour goes through fxColourIndex either way (R2).
    bool tagFrom(const QStringList &args, int line, Tag *out, bool untag)
    {
        QString colour = untag ? QString() : args[0], word = untag ? args[0] : QString();
        if (!untag && args.size() > 1) {
            word = args[0];
            colour = args[1];
        } else if (const qsizetype slash = args[0].indexOf(u'/'); slash >= 0) {
            word = args[0].mid(slash + 1);
            colour = args[0].left(slash);
        }
        out->word = word;
        out->colour = colour.isEmpty() ? -1 : fxColourIndex(colour);
        return colour.isEmpty() || out->colour >= 0 || fail(i18n("Unknown colour %1", colour), line);
    }

    bool action(const ScriptStmt &st)
    {
        const int line = st.line;
        FxValue target;
        if (!value(st.target, line, &target))
            return false;
        if (target.type != FxValue::Type::File || !target.file)
            return fail(i18n("%1 needs a file from For Each", st.action), line);
        FxFile &f = *target.file;
        if (const auto it = trashed.constFind(f.path); it != trashed.cend())
            return fail(i18n("File was already trashed (line %1)", *it), line);
        QStringList args;
        for (const FxExpr &a : st.args) {
            FxValue v;
            if (!value(a, line, &v))
                return false;
            args << v.toText();
        }

        PlanStep s;
        s.source = QUrl::fromLocalFile(f.path);
        s.line = line;
        s.baseFolder = base;
        const QString folder = QDir::cleanPath(QFileInfo(f.path).path());
        auto skip = [&s](const QString &note) {
            s.skipped = true;
            s.note = note;
        };
        // dest/name must be free on disk and not planned yet; a free name is reserved
        auto claim = [this, &skip](const QString &to) {
            if (QFileInfo::exists(to)) {
                skip(i18n("Name exists"));
            } else if (plannedNames.contains(to)) {
                skip(i18n("Same name planned above"));
            } else {
                plannedNames.insert(to);
                return true;
            }
            return false;
        };
        const QString &a = st.action;
        if (a == u"Trash"_s) {
            s.kind = PlanStep::Trash;
            trashed.insert(f.path, line);
        } else if (a == u"MoveTo"_s || a == u"CopyTo"_s) {
            s.kind = a == u"MoveTo"_s ? PlanStep::Move : PlanStep::Copy;
            const QString dest = resolve(args[0]);
            s.destFolder = QUrl::fromLocalFile(dest);
            const QString to = dest + u'/' + QFileInfo(f.path).fileName();
            if (s.kind == PlanStep::Move && dest == folder) {
                skip(i18n("Already there"));
            } else if (claim(to) && s.kind == PlanStep::Move) {
                plannedNames.remove(f.path);
                f.path = to;
            }
        } else if (a == u"Rename"_s) {
            const QString &name = args[0];
            if (name.isEmpty() || name.contains(u'/') || name == u"."_s || name == u".."_s)
                return fail(i18n("Not a valid file name: \"%1\"", name), line);
            s.kind = PlanStep::Rename;
            s.newName = name;
            const QString to = folder + u'/' + name;
            if (claim(to)) {
                plannedNames.remove(f.path);
                f.path = to;
            }
        } else if (a == u"Tag"_s) {
            s.kind = PlanStep::Tag;
            if (!tagFrom(args, line, &s.tag, false))
                return false;
            QList<Tag> tags = f.tags();
            if (tags.contains(s.tag)) {
                skip(i18n("Already tagged"));
            } else if (win12TagCount(tags) >= MaxTags) {
                skip(i18n("Already has %1 tags", MaxTags));
            } else {
                tags << s.tag;
                f.setTags(tags);
            }
        } else { // Untag: every tag whose word or "Colour/Word" matches (HASTAG's rule)
            s.kind = PlanStep::Untag;
            Tag want;
            if (!tagFrom(args, line, &want, true))
                return false;
            const QList<Tag> tags = f.tags();
            QList<Tag> kept;
            for (const Tag &t : tags) {
                if (t.word.compare(want.word, Qt::CaseInsensitive) == 0 && (want.colour < 0 || t.colour == want.colour)) {
                    s.tag = t;
                    r.plan << s;
                } else {
                    kept << t;
                }
            }
            if (kept.size() < tags.size()) {
                f.setTags(kept);
                return true;
            }
            s.tag = want;
            skip(i18n("Tag not present"));
        }
        r.plan << s;
        return true;
    }
};
}

ScriptInterpreter::ScriptInterpreter(const QString &baseFolder, qint64 maxStatements)
    : m_base(baseFolder)
    , m_max(maxStatements)
    , m_lister(listFolder)
{
}

void ScriptInterpreter::setLister(Lister lister)
{
    m_lister = std::move(lister);
}

void ScriptInterpreter::setProgress(std::function<void(qint64, int)> f, int everyNStatements)
{
    m_progress = std::move(f);
    m_progressEvery = qMax(1, everyNStatements);
}

ScriptResult ScriptInterpreter::run(const Script &script, const std::atomic_bool *cancel)
{
    Run run{m_base, m_max, m_lister, cancel, {}, {}, {}, {}, {}, {}, m_progress, m_progressEvery};
    run.ctx.baseFolder = m_base;
    run.ctx.variables = &run.vars;
    run.block(script.body);
    return run.r;
}

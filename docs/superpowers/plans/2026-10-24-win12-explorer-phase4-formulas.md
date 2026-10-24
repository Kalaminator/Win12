# Win12 Explorer Phase 4 (Formulas and scripts) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** every file is a row and its properties are columns: an Excel-syntax formula (our own parser, no shell, no eval) picks rows in Select by rule ▸ fx and names rows in Rename many ▸ fx; a VBA-like script loops over folders and builds a plan of moves/copies/renames/tags/trash; nothing changes until "Run plan", which runs KIO jobs with progress, Stop, conflict dialogs and Undo; formulas and scripts are saved by name and offered in Advanced ▸ My rules / My scripts, right-click ▸ Win12 ▸ My rules and Dolphin's right-click menu.

**Architecture:** A pure engine in `explorer/src/formula/` (no Widgets): `fxLex` → `FxParser` (Pratt-style precedence, Excel operators, arity-checked built-ins) → `fxEvaluate` over an `FxFile` row (lazy tags via `TagStore::read`, lazy photo size via KFileMetaData) producing an `FxValue`; `parseScript` + `ScriptInterpreter` reuse the same lexer/expression parser and turn actions into `PlanStep`s with a statement limit and a cancel flag. Two workers follow the `TagSearch` pattern (`QtConcurrent::run`, shared `Run` with `std::atomic_bool cancel`, batched queued signals): `FormulaRun` (KIO listing through the existing `RuleSearch` with a match-all matcher, evaluation off the UI thread) and `ScriptRun`. `PlanRunner` executes `PlanStep`s one KIO job at a time (recorded in `KIO::FileUndoManager`, `KJobWidgets::setWindow` so conflict dialogs appear) and reverses them on Undo. UI: an `fx` mode in `SelectByRuleDialog` and `RenameManyDialog` (shared `FormulaEdit`), a `ScriptDialog` with a `PlanView` (dry-run table + Run plan + Stop + Undo plan), a `RuleLibrary` (`$XDG_DATA_HOME/win12/rules.json`) feeding the ribbon menus, the Win12 context submenu and a generated Dolphin service menu.

**Tech Stack:** C++20, Qt 6 (Widgets, Concurrent — already linked), KF6 KIO, KFileMetaData (already linked through `win12tags`), QtTest offscreen.

**Spec:** `docs/superpowers/specs/2026-10-23-win12-explorer-design.md` — "Phase 4: Formulas and scripts", Anti-goals, Constraints, Decided how, Left to the builder. Mockup: `docs/superpowers/specs/2026-10-23-win12-explorer-mockups/formulas.html`. Conventions: `docs/superpowers/plans/2026-10-24-win12-explorer-phase2-tags.md`; deferred items: `docs/superpowers/plans/2026-10-23-win12-explorer-phase3-followups.md` (this plan folds in "Rename many: grey Unchanged rows show no reason" — Task 6; the rest stay deferred).

## Global Constraints

- Sources: `explorer/src/*.cpp` are globbed into `win12explorer`; Task 1 adds `file(GLOB FX_SRCS CONFIGURE_DEPENDS src/formula/*.cpp)` to the same library and `target_include_directories(win12explorer PUBLIC src src/formula)`. Each new test is one `explorer_add_test(<name>)` line in `explorer/CMakeLists.txt`; test files live in `explorer/autotests/`.
- Build and test only in a scratch dir, never `install.sh`, never sudo. Use **your own session's scratchpad**:
  `B=/tmp/claude-1000/-home-kalaminator-Documents-Start12/<session-id>/scratchpad/build; cmake -S explorer -B $B -DCMAKE_BUILD_TYPE=Debug >/dev/null && cmake --build $B -j$(nproc) 2>&1 | tail -5 && ctest --test-dir $B --output-on-failure 2>&1 | tail -15`. One test: `ctest --test-dir $B -R fxparser_test --output-on-failure 2>&1 | tail -20`.
- No test lock; every test is offscreen in its own process on its own `QTemporaryDir`. Tests touching the library call `QStandardPaths::setTestModeEnabled(true)` in `initTestCase`. Tests writing xattrs `QSKIP` when `KFileMetaData::UserMetaData::isSupported()` is false.
- **Spec rulings (binding):** our own parser/interpreter, user text never reaches a shell or an eval; formulas and scripts only build a plan, files change only in "Run plan"; running uses KIO jobs (progress, cancel, conflicts, Undo); scripts run off the UI thread with a step limit and a Stop button; listing and evaluation never block the UI thread; renames never overwrite (`ExplorerActions::rename` refuses an existing target); delete means Trash; Excel names wherever Excel has one; function and column names are **not** translated (Excel's English names), every other UI string goes through `i18n()` (domain `win12-explorer`, existing `po/` flow); no new plasmoid settings key (Call 12), so `main.xml` / `tests/settings-keys.txt` are untouched.
- Commit prefix `explorer:`. One fresh agent per task, ≤ ~100k tokens / ~40 tool calls; build and test output always `| tail`; no agent leaves background jobs. Model per task is marked `[sonnet]` or `[opus]`.

## Review Focus

1. A formula that reads `TAGS`/`HASTAG` on a file system without xattrs (FAT USB stick, `smb:/`) gets empty tags, `HASTAG` is `FALSE`, no error row — `FormulaRunTest::tagsEmptyWhereUnsupported` (Task 3).
2. A rename formula that yields the same base name for two rows, an empty name, or a name with `/`: second row `ClashPlanned`, the bad ones `Invalid` with a visible note, never an overwrite — `RenamePlanTest::planNamesDuplicateIsClash`, `RenamePlanTest::planNamesBadNameGetsNote` (Task 6).
3. A script that renames a file and then moves it: the move uses the new name; a script that trashes a file and touches it again errors with the line — `ScriptInterpreterTest::renameThenMoveUsesNewName`, `ScriptInterpreterTest::actionAfterTrashIsAnError` (Task 8).
4. `.MoveTo` into a folder that does not exist yet creates it instead of failing; Stop during a run finishes the current step only and Undo reverses exactly the executed ones — `PlanRunnerTest::moveCreatesMissingFolder`, `PlanRunnerTest::cancelStopsAfterCurrentStep` (Task 9).
5. Closing a dialog mid-evaluation or mid-script stops the worker and no signal arrives afterwards — `FormulaRunTest::stopEndsWithoutFinished` (Task 3), `ScriptRunTest::stopEndsWithoutFinished` (Task 10).

## File Structure

- Create `explorer/src/formula/fxvalue.h/.cpp` — `FxValue`, `FxError`, display/text conversion.
- Create `explorer/src/formula/fxlexer.h/.cpp` — `FxToken`, `fxLex`.
- Create `explorer/src/formula/fxparser.h/.cpp` — `FxNode`, `FxExpr`, `FxParseError`, `FxParser`, `FxFunctions`, `FxColumns`.
- Create `explorer/src/formula/fxfile.h/.cpp` — `FxFile`, `FxPhoto`, `FxContext`, `fxFileFor(KFileItem…)`.
- Create `explorer/src/formula/fxeval.h/.cpp` — `fxEvaluate`.
- Create `explorer/src/formula/script.h/.cpp` — `ScriptStmt`, `Script`, `parseScript`.
- Create `explorer/src/formula/planstep.h/.cpp` — `PlanStep`.
- Create `explorer/src/formula/scriptinterpreter.h/.cpp` — `ScriptInterpreter`, `ScriptResult`.
- Create `explorer/src/formularun.h/.cpp`, `explorer/src/scriptrun.h/.cpp` — the two workers.
- Create `explorer/src/planrunner.h/.cpp` — KIO execution + undo.
- Create `explorer/src/rulelibrary.h/.cpp` — `SavedRule`, `RuleLibrary`, Dolphin service menu writer.
- Create `explorer/src/formulaedit.h/.cpp`, `explorer/src/planview.h/.cpp`, `explorer/src/scriptdialog.h/.cpp`.
- Modify `explorer/src/selectbyruledialog.h/.cpp`, `renamemanydialog.h/.cpp`, `renameplan.h/.cpp`, `ribbonspec.h/.cpp`, `exploreractions.h/.cpp`, `explorerwindow.cpp`, `cli.h/.cpp`, `main.cpp`, `explorer/CMakeLists.txt`.
- Tests: `explorer/autotests/fxlexer_test.cpp`, `fxparser_test.cpp`, `fxeval_test.cpp`, `formularun_test.cpp`, `rulelibrary_test.cpp`, `script_test.cpp`, `scriptinterpreter_test.cpp`, `planrunner_test.cpp`, `scriptrun_test.cpp`, `scriptdialog_test.cpp`; extend `selectbyruledialog_test`, `renameplan_test`, `renamemanydialog_test`, `ribbonspec_test`, `exploreractions_test`, `cli_test`.

---

### Task 1 [opus]: `FxValue`, lexer and expression parser (pure)

**Files:**
- Create: `explorer/src/formula/fxvalue.h/.cpp`, `fxlexer.h/.cpp`, `fxparser.h/.cpp`
- Modify: `explorer/CMakeLists.txt` (glob `src/formula/*.cpp` into `win12explorer`, include dir `src/formula`; `explorer_add_test(fxlexer_test)`, `explorer_add_test(fxparser_test)`)
- Test: `explorer/autotests/fxlexer_test.cpp`, `explorer/autotests/fxparser_test.cpp`

**Interfaces (produces):**
```cpp
// fxvalue.h
struct FxFile;
enum class FxError { Value, Name, Div0, NA, Num }; // "#VALUE!" "#NAME?" "#DIV/0!" "#N/A" "#NUM!"
struct FxValue {
    enum class Type { Empty, Bool, Number, Text, Date, File, Error };
    Type type = Type::Empty;
    bool b = false; double n = 0; QString s; QDateTime d; std::shared_ptr<FxFile> file; FxError err = FxError::Value;
    static FxValue empty(); static FxValue boolean(bool); static FxValue number(double); static FxValue text(const QString &);
    static FxValue date(const QDateTime &); static FxValue fileRef(std::shared_ptr<FxFile>); static FxValue error(FxError);
    bool isError() const;
    bool truthy(bool *ok) const;   // Bool as is; Number != 0; anything else *ok = false
    QString toText() const;        // for '&' and TEXT(): Empty -> "", TRUE/FALSE, numbers "12" / "1.5" (<= 2 decimals, trailing zeros dropped), Date "yyyy-MM-dd hh:mm", File -> its full name, errors -> "#VALUE!" etc.
    QString display() const;       // toText(), but Empty -> "" and File -> path
    bool operator==(const FxValue &) const; // exact (type and payload); tests only
};
// fxlexer.h
struct FxToken { enum Kind { Number, String, Ident, Op, LParen, RParen, Comma, Dot, Newline, End }; Kind kind; QString text; double number = 0; int line = 1, col = 1; };
QList<FxToken> fxLex(const QString &src, QString *error, int *errLine = nullptr, int *errCol = nullptr);
// fxparser.h
struct FxNode { enum Kind { Literal, Column, Variable, Call, Member, Method, Unary, Binary }; Kind kind; FxValue literal; QString name /*column, variable, function, member, method*/; QString op; std::vector<std::shared_ptr<FxNode>> args; int line = 1, col = 1; };
using FxExpr = std::shared_ptr<FxNode>;
struct FxParseError { QString message; int line = 1, col = 1; };
struct FxFunctionSpec { QString name; int minArgs, maxArgs; /* -1 = any */ QString signature; QString help; };
namespace FxFunctions { const QList<FxFunctionSpec> &all(); const FxFunctionSpec *find(const QString &upperName); }
namespace FxColumns { const QStringList &names(); bool isColumn(const QString &upperName); }
class FxParser {
public:
    enum class Mode { Formula, Script };
    static std::optional<FxExpr> parseFormula(const QString &text, FxParseError *err); // leading '=' optional; whole text consumed; Mode::Formula
    static std::optional<FxExpr> parseExpression(const QList<FxToken> &, int *pos, Mode, FxParseError *err); // stops before Newline/End/unmatched ')' ; Task 7 uses it
};
```
Lexer: numbers `12`, `1.5`, size literals `10MB` `1.5GB` `200KB` `3B` through `parseSize` (`rulematcher.h`) → `Number` with `text` = source text; strings in `"…"` with `""` as an escaped quote, unterminated → error "Unterminated string"; identifiers `[A-Za-z_][A-Za-z0-9_]*` (`TRUE`/`FALSE` stay Ident, the parser makes literals); ops `+ - * / ^ & = <> < > <= >= :=`; `'` comment to end of line; `\n` → `Newline` (formula parsing skips them); `_` alone at a line end joins lines (VBA continuation). Columns: `NAME EXT FULLNAME FOLDER PATH SIZE MODIFIED CREATED ISFOLDER DEPTH TAGS WIDTH HEIGHT TAKEN`. Functions (name: min–max args): LEFT 1–2, RIGHT 1–2, MID 3, LEN 1, FIND 2–3, SUBSTITUTE 3, UPPER 1, LOWER 1, PROPER 1, TRIM 1, TEXT 2, CONTAINS 2, STARTSWITH 2, ENDSWITH 2, REGEX 2, IF 2–3, IFS 2–any (even), AND 1–any, OR 1–any, NOT 1, TODAY 0, DAYS 1–2, YEAR 1, MONTH 1, DAY 1, DATE 3, DATEVALUE 1, HASTAG 1–2, COUNTTAGS 0, ROW 0, ROWINFOLDER 0, COUNTIN 1, plus script-only `FILES` 1–2 (parsed like any call; named arg `Subfolders:=` is accepted only inside FILES and stored as a trailing `Literal` Bool arg). Precedence, high → low: unary `-`/`NOT`-keyword; `^`; `* /`; `+ -`; `&`; `= <> < > <= >=`; `AND`-keyword; `OR`-keyword (keywords `And Or Not` are operators in both modes, case-insensitive — Call 4; `AND(...)`/`OR(...)`/`NOT(...)` calls keep working). Postfix `.Name` → `Member`, `.Contains(args)` → `Method`, allowed in both modes (a column name before `.` is an error in Formula mode). Unknown identifier: Formula mode → error `Unknown name FOO`; Script mode → `Variable`. Arity error text: `LEFT takes 1 to 2 arguments`; `)` missing: `Expected )`; trailing junk: `Unexpected "x"`. Names are case-insensitive; `FxNode::name` is stored upper-case for columns/functions, as written for variables/members.

- [ ] **Step 1: Lexer tests** `FxLexerTest`: `numbersAndSizes` (`"12 1.5 10MB 200KB"` → 4 Number tokens with 12, 1.5, 10485760, 204800), `stringsWithDoubledQuote` (`"\"a \"\"q\"\" b\""` → one String `a "q" b`), `operators` (`"<> <= := & ^"` → 5 Op tokens with those texts), `commentAndNewline` (`"x ' hi\ny"` → Ident x, Newline, Ident y, End), `continuationLine` (`"a _\n& b"` → Ident, Op, Ident with no Newline), `unterminatedStringIsError` (`"\"abc"` → error "Unterminated string", col 1), `positions` (`"\n  LEFT("` → Ident at line 2 col 3).
- [ ] **Step 2: Run; expect** compile failure.
- [ ] **Step 3: Implement `fxvalue` and `fxlexer`.**
- [ ] **Step 4: Build + ctest; expect** `fxlexer_test` PASS. **Commit** `explorer: FxValue and the formula/script lexer`.
- [ ] **Step 5: Parser tests** `FxParserTest`: `mockupSelectFormula` (`=AND(LEFT(NAME,3)="CED", DAYS(MODIFIED)>30, NOT(HASTAG("Xiaomi","Red")))` → Call AND with 3 args; arg 0 is Binary `=` whose left is Call LEFT(Column NAME, Literal 3)), `precedence` (`1+2*3^2` → Binary + (1, Binary * (2, Binary ^ (3,2)))), `concatBelowPlusAboveCompare` (`"a"&1+1="a2"` → Binary = (Binary & ("a", Binary + (1,1)), "a2")), `keywordsAndOrNot` (Mode::Script, `Not a = 1 And b Or c` → top Binary "OR"; its left Binary "AND"; AND's left Binary "=" whose left is Unary "NOT" over Variable a), `leadingEqualsOptional` (`LEN(NAME)` and `=LEN(NAME)` give equal trees), `caseInsensitiveNames` (`left(name,1)` → Call "LEFT", Column "NAME"), `trueFalseLiterals` (`TRUE` → Literal Bool true), `unknownNameInFormulaMode` (`FOO+1` → error "Unknown name FOO", col 1), `variableInScriptMode` (`foo+1` → Variable "foo"), `arity` (`LEFT()` → "LEFT takes 1 to 2 arguments"; `IFS(a)` → "IFS takes an even number of arguments"), `missingParen` (`LEFT(NAME` → "Expected )", col 10), `trailingJunk` (`1 2` → `Unexpected "2"`), `memberAndMethod` (Script: `f.Name & f.Contains("x")` → Binary & (Member name "Name" of Variable f, Method "Contains" with 1 arg)), `filesNamedArg` (Script: `Files("~/D", Subfolders:=True)` → Call FILES, 2 args, second Literal Bool true), `namedArgElsewhereIsError` (`LEFT(a, x:=1)` → error containing ":="), `functionTableHasAllSpecNames` (every name in the spec list is in `FxFunctions::all()`, 33 entries incl. DATE, DATEVALUE, FILES).
- [ ] **Step 6: Run; expect** compile failure. **Step 7: Implement `fxparser`** (recursive descent with a precedence table; `parseFormula` lexes, skips Newlines, calls `parseExpression`, requires `End`).
- [ ] **Step 8: Build + ctest; expect** `fxparser_test` PASS. **Commit** `explorer: formula expression parser with Excel precedence, columns and arity-checked built-ins`.

### Task 2 [opus]: `FxFile`, `FxContext` and the evaluator (pure)

**Files:**
- Create: `explorer/src/formula/fxfile.h/.cpp` (struct only; `fxFileFor` comes in Task 3), `explorer/src/formula/fxeval.h/.cpp`
- Test: `explorer/autotests/fxeval_test.cpp`; `explorer_add_test(fxeval_test)`

**Interfaces (produces):**
```cpp
struct FxPhoto { int width = 0, height = 0; QDateTime taken; };
struct FxFile {
    QString path;              // local absolute path; scripts update it after a planned Rename/MoveTo (Call 9)
    bool isFolder = false; qint64 size = 0; QDateTime modified, created; int depth = 0; int row = 1, rowInFolder = 1;
    std::function<QList<Tag>()> loadTags;  std::function<FxPhoto()> loadPhoto; // optional, called once, cached
    QList<Tag> tags(); FxPhoto photo(); void setTags(const QList<Tag> &); // setTags: scripts' .Tag/.Untag
    QString fullName() const; QString name() const; QString ext() const; QString folder() const; // folder: parent folder's name
};
// name/ext split: last '.' that is neither first nor last char, files only -> name "a.b", ext "txt"; folders and "a.", ".bashrc" -> ext ""
struct FxContext {
    QDate today = QDate::currentDate();
    QString baseFolder;                                       // COUNTIN relative paths, "~" expands to QDir::homePath()
    std::function<int(const QString &absFolder)> countIn;     // default: QDir(..).entryList(AllEntries|Hidden|NoDotAndDotDot).size(), cached per folder in the context; -1 -> #N/A
    QHash<QString, FxValue> *variables = nullptr;             // scripts (case-insensitive keys: stored lower-case)
};
FxValue fxEvaluate(const FxExpr &, const std::shared_ptr<FxFile> &row, FxContext &);
```
Semantics (Excel's where Excel has the function): text compare case-insensitive; `=`/`<>` across Bool/Number/Text/Date of the same type; mismatched types → `#VALUE!`; Empty compares as `""`/0 in Excel style (Empty = "" is TRUE, Empty = 0 is TRUE); `+ - * / ^` on numbers only, `/0` → `#DIV/0!`; `&` uses `toText()`; LEFT/RIGHT default 1; MID/FIND 1-based, FIND case-sensitive, not found → `#VALUE!`; PROPER = each word's first letter upper, rest lower; TEXT(number, fmt): fmt of `0`s pads to that many digits (`"00"`, 7 → `"07"`), with `.` → that many decimals (`"0.0"`); TEXT(date, fmt) → `QDateTime::toString(fmt)`; CONTAINS/STARTSWITH/ENDSWITH case-insensitive; REGEX(text, pattern) → Bool (`QRegularExpression`, invalid pattern → `#VALUE!`); IF(c, a[, b]) missing b → FALSE; IFS no match → `#N/A`; AND/OR short-circuit left to right, non-Bool/Number operand → `#VALUE!`; TODAY → Date at 00:00; DAYS(x) = today − date(x) in whole days, DAYS(end, start) = date(end) − date(start); YEAR/MONTH/DAY of a Date (Empty → `#VALUE!`); DATE(y,m,d), DATEVALUE("2026-01-31") (ISO only); HASTAG(word[, colour]): a tag whose `word` equals case-insensitively or whose `encode()` equals; with colour, both must match; COUNTTAGS = all tags incl. foreign; ROW/ROWINFOLDER from the row; COUNTIN(folder); Column SIZE of a folder = 0, CREATED/TAKEN unknown → Empty, TAGS → encoded tags joined `", "`, WIDTH/HEIGHT 0 when not a photo (Call 7). Members on a File value: `Name Ext FullName Folder Path Size Modified Created IsFolder Depth Tags` (same values as the columns); method `Contains(x)` = CONTAINS(FullName, x). Unknown member/variable → `#NAME?`.

- [ ] **Step 1: Tests** `FxEvalTest` with a helper `ev(text, row)`; fixture row: `path "/base/Trip/holiday.jpg"`, size 1258291, modified 2026-08-02 14:05, created empty, depth 1, row 3, rowInFolder 2, `loadTags` → `{Tag{Green idx, "Work"}}`, `loadPhoto` → 4000×3000 taken 2026-07-30; `today` fixed to 2026-10-24. Cases (exact): `columns` (NAME "holiday", EXT "jpg", FULLNAME "holiday.jpg", FOLDER "Trip", PATH full, SIZE 1258291, ISFOLDER false, DEPTH 1, TAGS "Green/Work", WIDTH 4000, CREATED Empty), `textFunctions` (`LEFT(NAME,3)`="hol", `RIGHT(NAME)`="y", `MID(NAME,2,3)`="oli", `LEN(FULLNAME)`=11, `FIND("day",NAME)`=5, `FIND("x",NAME)`=#VALUE!, `SUBSTITUTE(NAME,"day","night")`="holinight", `PROPER("hELLO wORLD")`="Hello World", `TRIM("  a  b ")`="a b" (Excel's TRIM also collapses inner runs), `UPPER/LOWER`), `textFormat` (`TEXT(7,"00")`="07", `TEXT(ROWINFOLDER(),"00")`="02", `TEXT(1.25,"0.0")`="1.3", `TEXT(MODIFIED,"yyyy")`="2026"), `predicates` (`CONTAINS(NAME,"HOLI")` true, `STARTSWITH`, `ENDSWITH`, `REGEX(NAME,"^h.*y$")` true, `REGEX(NAME,"(")` #VALUE!), `logic` (`IF(SIZE>10MB,"big","small")`="small", `IF(FALSE,1)`=FALSE, `IFS(SIZE>10MB,"a",SIZE>1MB,"b")`="b", `IFS(FALSE,1)`=#N/A, `AND(TRUE,1=2)`=FALSE, `OR(1=2,TRUE)`, `NOT(TRUE)`, `AND("x")`=#VALUE!), `dates` (`DAYS(MODIFIED)`=83, `DAYS(DATE(2026,10,24),MODIFIED)`=83, `YEAR(MODIFIED)`=2026, `MONTH`=8, `DAY`=2, `DATEVALUE("2026-01-31")` is Date, `DAYS(CREATED)`=#VALUE!), `tags` (`HASTAG("work")` true, `HASTAG("Work","Green")` true, `HASTAG("Work","Red")` false, `HASTAG("Green/Work")` true, `COUNTTAGS()`=1), `rows` (`ROW()`=3, `ROWINFOLDER()`=2), `countIn` (context `countIn` stub returns 4 for "/base/Trip" → `COUNTIN("Trip")`=4 with baseFolder "/base"; `COUNTIN("~")` resolves to home path), `arithmeticAndCompare` (`1+2*3`=7, `2^10`=1024, `1/0`=#DIV/0!, `"a"<>"A"`=FALSE, `"b">"a"`=TRUE, `SIZE>"x"`=#VALUE!, `"a"&1&TRUE`="a1TRUE"), `mockupSelectFormula` (the mockup formula on a row named "CED_report_01.docx", modified 2026-08-02, tags Green/Work → TRUE; on "CED_mi13.pdf" with Red/Xiaomi → FALSE), `mockupRenameFormula` (`TEXT(ROWINFOLDER(),"00") & " - " & PROPER(NAME)` → "02 - Holiday"), `lazyTagsCalledOnce` (counter in `loadTags`; `HASTAG("a") & TAGS` → 1 call; `LEN(NAME)` → 0 calls), `scriptMembers` (Mode::Script tree `f.Ext = "jpg" And f.Contains("HOLI")` with variables {f → fileRef(row)} → TRUE; `g.Name` → #NAME?), `folderSplit` (row isFolder "/x/a.b" → NAME "a.b", EXT ""; file ".bashrc" → NAME ".bashrc" EXT ""; file "a." → NAME "a.", EXT "").
- [ ] **Step 2: Run; expect** compile failure. **Step 3: Implement.** **Step 4: Build + ctest; expect** PASS.
- [ ] **Step 5: Commit** `explorer: formula evaluator — Excel text/logic/date functions, tag and row functions over a file row`.

### Task 3 [opus]: `fxFileFor` from a `KFileItem` and the `FormulaRun` worker

**Files:**
- Modify: `explorer/src/formula/fxfile.h/.cpp` (add `fxFileFor`)
- Create: `explorer/src/formularun.h/.cpp`
- Test: `explorer/autotests/formularun_test.cpp`; `explorer_add_test(formularun_test)`

**Interfaces (produces):**
```cpp
std::shared_ptr<FxFile> fxFileFor(const KFileItem &, const QUrl &base, int row, int rowInFolder);
// local items: loadTags = TagStore::read(url); loadPhoto for mimetype "image/*" via KFileMetaData::ExtractorCollection (Property::Width/Height/PhotoDateTimeOriginal), else empty FxPhoto; depth = count of '/' in the relative path (0 for a direct child); non-local: loadTags empty, path = url.toDisplayString()
struct FxRowResult { KFileItem item; std::shared_ptr<FxFile> row; FxValue value; };
class FormulaRun : public QObject {
    Q_OBJECT
public:
    explicit FormulaRun(QObject *parent = nullptr);
    void start(const QUrl &folder, bool recursive, const FxExpr &);                  // lists through RuleSearch (match-all RuleMatcher), sorts rows by relative path (QCollator numeric, case-insensitive — Call 8), numbers ROW/ROWINFOLDER, evaluates off the UI thread
    void startOn(const KFileItemList &items /*kept order*/, const QUrl &base, const FxExpr &); // no listing (Rename many)
    void stop(); bool isRunning() const;
Q_SIGNALS:
    void results(const QList<FxRowResult> &); // batches of <= 200, row order
    void finished(bool ok, const QString &error);
};
```
Worker: `QtConcurrent::run` with a `std::shared_ptr<Run>` holding `std::atomic_bool cancel` and the rows; one `FxContext` per run (`countIn` cache lives there); the `ExtractorCollection` is created inside the worker thread; results delivered with queued `QMetaObject::invokeMethod` (TagSearch pattern, `tagsearch.cpp`).

- [ ] **Step 1: Tests** `FormulaRunTest` on a `QTemporaryDir` tree `a.txt`(3 bytes), `sub/b.txt`, `sub/c.png` (a 20×10 `QImage` saved as PNG), `Zz/d.txt`: `rowsAreSortedAndNumbered` (`start(dir, true, ROW() & ":" & FULLNAME)` → results in order "1:a.txt", "2:b.txt", "3:c.png", "4:d.txt"; DEPTH of b.txt 1), `rowInFolderRestarts` (`ROWINFOLDER()` → 1,1,2,1), `photoColumns` (`WIDTH & "x" & HEIGHT` → "20x10" for c.png, "0x0" for a.txt), `tagsEmptyWhereUnsupported` (`fxFileFor` on `KFileItem(QUrl("smb://h/s/f.txt"))` → `tags()` empty and `HASTAG("x")` FALSE, no error), `tagsRead` (QSKIP without xattr; `TagStore` adds Red/Xiaomi to a.txt → `HASTAG("xiaomi")` TRUE), `startOnKeepsOrder` (`startOn({d, a}, base, FULLNAME)` → "d.txt","a.txt" with ROW 1,2), `batchesNotPerFile` (300 files → `results` emitted ≤ 2 times), `stopEndsWithoutFinished` (start, `stop()` immediately, no `finished` in 300 ms, `isRunning()` false), `missingFolderFinishesWithError`, `errorRowsArePerRow` (`1/ (SIZE-3)` → a.txt `#DIV/0!`, others numbers; `finished(true)`).
- [ ] **Step 2: Run; expect** compile failure. **Step 3: Implement.** **Step 4: Build + ctest; expect** PASS.
- [ ] **Step 5: Commit** `explorer: FormulaRun — KIO listing, lazy tags/photo rows, evaluation off the UI thread with cancel`.

### Task 4 [sonnet]: `RuleLibrary` (rules.json)

**Files:**
- Create: `explorer/src/rulelibrary.h/.cpp`
- Test: `explorer/autotests/rulelibrary_test.cpp`; `explorer_add_test(rulelibrary_test)`

**Interfaces (produces):**
```cpp
struct SavedRule { enum class Kind { Select, Rename, Script }; QString name; QString text; Kind kind = Kind::Select; bool operator==(const SavedRule &) const = default; };
class RuleLibrary : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    static QString path();                                  // QStandardPaths::GenericDataLocation + "/win12/rules.json" (next to tags.json)
    QList<SavedRule> all() const;                           // sorted by kind then name (case-insensitive); loaded lazily from disk
    QList<SavedRule> ofKind(SavedRule::Kind) const;
    bool save(const SavedRule &, QString *error);            // replaces same name+kind; empty name -> false "A name is needed"; writes; emits changed()
    bool remove(const QString &name, SavedRule::Kind, QString *error);
    void reload();
Q_SIGNALS: void changed();
};
```
File: `{"version":1,"rules":[{"name":"Old CED files","kind":"select","text":"=AND(...)"}]}`; kinds `select|rename|script`. Mirror `tagregistry.cpp` for the write path (mkpath, `QSaveFile`).

- [ ] **Step 1: Tests** (`setTestModeEnabled`, delete `path()` in `init`): `saveThenAllRoundTrips` (save 2 formulas + 1 script; a fresh `RuleLibrary` reads the same 3, sorted Select "Old CED files" before Rename before Script), `saveReplacesSameNameAndKind` (same name twice → 1 entry with the new text; same name, other kind → 2), `removeByNameAndKind`, `emptyNameRefused` (`save` false, error "A name is needed", no `changed`), `unreadableFileIsReported` (path is a directory → `save` false with non-empty error), `changedFiresOnlyAfterWrite`.
- [ ] **Step 2–4: Run → implement → PASS.** **Step 5: Commit** `explorer: RuleLibrary — saved formulas and scripts in win12/rules.json`.

### Task 5 [sonnet]: `FormulaEdit` and the fx mode of Select by rule

**Files:**
- Create: `explorer/src/formulaedit.h/.cpp`
- Modify: `explorer/src/selectbyruledialog.h/.cpp`
- Test: extend `explorer/autotests/selectbyruledialog_test.cpp`

**Interfaces (produces):**
```cpp
class FormulaEdit : public QWidget {   // one row: "fx" label, mono QLineEdit (objectName "formulaEdit"), "Functions…" (objectName "fxHelp"), "Save as…" (objectName "fxSave"); red error label below (objectName "fxError")
    Q_OBJECT
public:
    explicit FormulaEdit(QWidget *parent = nullptr);
    void setText(const QString &); QString text() const;
    std::optional<FxExpr> expr() const;             // parsed now; nullopt + error label when invalid or empty
Q_SIGNALS:
    void formulaChanged(bool valid);                 // 300 ms after the last edit
    void saveRequested(const QString &text);         // only when valid
    void submitted();                                // Enter
};
// SelectByRuleDialog additions
enum class Mode { Rules, Formula };
void setMode(Mode); Mode mode() const;              // two checkable QToolButtons "Simple rules" (objectName "modeRules") / "fx Formula" ("modeFormula"); Rules hides the FormulaEdit, Formula hides the rule rows and "+ Add rule"
void setFormula(const QString &); QString formula() const;
void setLibrary(RuleLibrary *);                     // Save as… -> QInputDialog::getText name -> library->save({name, text, Kind::Select}); errors through KMessageBox::error
```
Formula mode: `find()` runs `FormulaRun::start(lookIn(), recursive, expr)`; the tree gets columns `Name, Ext, Size, Modified, Tags, fx result` (Rules mode keeps `Name, Modified, Size`); every row is listed, ticked when the value is Bool TRUE, unticked otherwise; the fx result cell shows `value.display()` and red text for errors; the count label reads `i18np("%1 row is TRUE", "%1 rows are TRUE", n)` in Formula mode. `FxHelpDialog` (inside `formulaedit.cpp`): a `QTextBrowser` listing `FxColumns::names()` and each `FxFunctionSpec::signature` + `help`, grouped Text / Logic / Date / Files like the mockup footer. `reject()` and the destructor stop the FormulaRun.

- [ ] **Step 1: Tests** (`SelectByRuleDialogTest`): `formulaModeHidesRulesShowsEdit`, `formulaFindListsAllRowsTicksTrue` (tree `a.txt`(3 B), `b.txt`(0 B); formula `SIZE>0` → 2 rows; a ticked, b unticked; `ticked()` = {a}; count label "1 row is TRUE"), `formulaErrorRowShowsCode` (`1/SIZE` → b's fx cell "#DIV/0!"), `invalidFormulaDisablesFind` (`LEFT(` → Find disabled, error label "Expected )"), `saveAsStoresSelectRule` (`QInputDialog` answered through a `QTimer`-driven `activeModalWidget()` text entry "Old" → library has {Old, text, Select}), `rejectStopsFormulaRun`.
- [ ] **Step 2–4: Run → implement → PASS.** **Step 5: Commit** `explorer: Select by rule ▸ fx — formula mode with an Excel-like result grid, Save as…`.

### Task 6 [sonnet]: Rename many ▸ fx and `RenamePlan::planNames` (folds in "grey rows show no reason")

**Files:**
- Modify: `explorer/src/renameplan.h/.cpp`, `explorer/src/renamemanydialog.h/.cpp`, `explorer/src/exploreractions.cpp` (`renameMany` fills `RenameItem::item`)
- Test: extend `renameplan_test.cpp`, `renamemanydialog_test.cpp`

**Interfaces (produces):**
```cpp
struct RenameItem { QUrl url; QString name; QDateTime modified; KFileItem item; }; // item: new 4th field, existing 3-field initialisers keep compiling
struct PlannedRename { ...; enum Status { Rename, Unchanged, ClashExisting, ClashPlanned, Invalid }; QString note; }; // note: Invalid "Empty name" / "Names cannot contain /" / the fx error code; Clash* "Name exists" / "Same name planned above"
namespace RenamePlan {
QList<PlannedRename> planNames(const QList<QString> &baseNames, const QList<RenameItem> &items, const QSet<QString> &existingNames); // appends each item's extension exactly like plan(); plan(blocks, …) now calls buildName per item then planNames
}
// RenameManyDialog additions
enum class Mode { Blocks, Formula }; void setMode(Mode); Mode mode() const; void setFormula(const QString &); QString formula() const; void setLibrary(RuleLibrary *); // Save as… -> Kind::Rename
QUrl base() const; // the first item's folder; passed to FormulaRun::startOn
```
Formula mode: `FormulaRun::startOn(items, base, expr)` 300 ms after each edit (`stop()` the previous run first); while running the After column shows "…"; on `finished`, `plan()` = `planNames(values' toText(), items, existing)` with error values → `Invalid` note = the error code; the tree shows `note` in a third column `Why` (Blocks mode too — this is the folded follow-up) and keeps grey rows for anything but `Rename`. The OK button is disabled while a run is active or the formula is invalid.

- [ ] **Step 1: Tests** `RenamePlanTest`: `planNamesAppendsExtension` ({"01 - Holiday"} on `holiday.jpg` → "01 - Holiday.jpg", Rename), `planNamesDuplicateIsClash` ({"x","x"} → second `ClashPlanned`, note "Same name planned above"), `planNamesBadNameGetsNote` ({"", "a/b"} → both `Invalid`, notes "Empty name", "Names cannot contain /"), `planNamesExistingIsClash` (existing {"x.jpg"} → `ClashExisting`, note "Name exists"), `blocksPlanStillPassesOldTests` (run the existing suite). `RenameManyDialogTest`: `formulaModeMockup` (items Trip/holiday.jpg, Trip/beach.jpg, Work/notes.txt in that order; formula `TEXT(ROWINFOLDER(),"00") & " - " & PROPER(NAME)` → After "01 - Holiday.jpg", "02 - Beach.jpg", "01 - Notes.txt"), `formulaErrorRowIsInvalidWithNote` (`1/0` → all `Invalid`, Why "#DIV/0!"), `whyColumnShowsClash` (Blocks mode, two items both renamed "a" → second row Why "Same name planned above"), `okDisabledWhileEvaluating`.
- [ ] **Step 2–4: Run → implement → PASS.** **Step 5: Commit** `explorer: Rename many ▸ fx — formula names the rows; a Why column explains grey rows`.

### Task 7 [opus]: Script parser (pure)

**Files:**
- Create: `explorer/src/formula/script.h/.cpp`
- Test: `explorer/autotests/script_test.cpp`; `explorer_add_test(script_test)`

**Interfaces (produces):**
```cpp
struct ScriptStmt {
    enum Kind { Assign, ForEach, If, Action, ExitFor };
    Kind kind; int line = 1;
    QString var; FxExpr expr;                                             // Assign: var = expr | Dim var [= expr] ("As …" ignored); ForEach: var In expr (expr must be Call FILES)
    std::vector<ScriptStmt> body;                                         // ForEach
    std::vector<std::pair<FxExpr, std::vector<ScriptStmt>>> branches;     // If/ElseIf (condition, body)
    std::vector<ScriptStmt> elseBody;
    FxExpr target; QString action; std::vector<FxExpr> args;              // Action: target.MoveTo a | .CopyTo a | .Rename a | .Tag w[, c] | .Untag w | .Trash  (VBA call syntax: no parentheses; parentheses also accepted)
};
struct Script { std::vector<ScriptStmt> body; };
std::optional<Script> parseScript(const QString &text, FxParseError *err);
```
Grammar (keywords case-insensitive, one statement per line, `'` comments, `_` continuation): `For Each v In <expr>` … `Next [v]`; `If <expr> Then` … `[ElseIf <expr> Then …]* [Else …] End If` (block form only — Call 10); `Exit For`; `Dim v [As …] [= expr]`; `v = expr`; `<expr>.<Action> [args]`. Actions are exactly `MoveTo CopyTo Rename Tag Untag Trash` with arity 1 / 1 / 1 / 1–2 / 1 / 0; anything else after `.` in statement position → `Unknown action X`. Errors carry the line: `Expected Next` (line of the For), `Expected End If`, `Next without For`, `Exit For outside a loop`, `For Each needs Files(…)`.

- [ ] **Step 1: Tests** `ScriptTest`: `mockupScriptParses` (the mockup script → 1 ForEach var "f", expr Call FILES with 2 args; body = 1 If with 2 branches; branch 0 condition is Binary AND, body 2 Actions "MoveTo" (1 arg, Binary &) and "Tag" (2 args); branch 1 body 1 Action "Trash" with 0 args), `assignAndDim` (`Dim n As Integer\nn = 1\nx = n + 1` → 3 statements, Assign kinds; Dim without `=` has null expr), `elseBranch`, `exitFor`, `missingNext` (error "Expected Next", line 1), `missingEndIf` (line of the If), `nextWithoutFor`, `exitForOutsideLoop`, `unknownAction` (`f.Explode` → `Unknown action Explode`, its line), `forEachNeedsFiles` (`For Each f In x` → error), `actionArity` (`f.MoveTo` with no arg → `MoveTo takes 1 argument`), `parenthesisedActionArgs` (`f.Tag("a", "Red")` same tree as without parentheses), `blankLinesAndComments`.
- [ ] **Step 2–4: Run → implement → PASS.** **Step 5: Commit** `explorer: VBA-like script parser — For Each / If / variables / file actions`.

### Task 8 [opus]: `PlanStep` and `ScriptInterpreter` (pure, step limit, cancel)

**Files:**
- Create: `explorer/src/formula/planstep.h/.cpp`, `explorer/src/formula/scriptinterpreter.h/.cpp`
- Test: `explorer/autotests/scriptinterpreter_test.cpp`; `explorer_add_test(scriptinterpreter_test)`

**Interfaces (produces):**
```cpp
struct PlanStep {
    enum Kind { Move, Copy, Rename, Tag, Untag, Trash };
    Kind kind; QUrl source; QUrl destFolder; /* Move/Copy */ QString newName; /* Rename */ Tag tag; /* Tag/Untag */
    int line = 0; bool skipped = false; QString note;   // skipped + note: "Name exists", "Same name planned above", "Already has 3 tags", "Tag not present"
    QString actionText() const;  // i18n: "Move → Invoices/2026" (dest relative to the script's base folder when inside it, else full path), "Copy → …", "Rename → new.txt", "Tag Orange/Invoice", "Untag Invoice", "Trash"
    QString fileText() const;    // source file name
};
struct ScriptResult { bool ok = true; QString error; int line = 0; QList<PlanStep> plan; qint64 statements = 0; };
class ScriptInterpreter {
public:
    static constexpr qint64 DefaultMaxStatements = 1'000'000; // Call 11
    ScriptInterpreter(const QString &baseFolder, qint64 maxStatements = DefaultMaxStatements);
    using Lister = std::function<QList<std::shared_ptr<FxFile>>(const QString &absFolder, bool subfolders, QString *error)>;
    void setLister(Lister); // default: QDirIterator (AllEntries | Hidden | NoDotAndDotDot), rows sorted like FormulaRun, lazies from fxFileFor(KFileItem(QUrl::fromLocalFile(..)))
    ScriptResult run(const Script &, const std::atomic_bool *cancel); // cancel -> ok=false, error "Stopped"
};
```
Rules: every executed statement counts; over the limit → `ok=false`, error `Script stopped after 1,000,000 statements (endless loop?)` (`QLocale().toString` for the number), `plan` keeps what was built so far. `Files(path[, subfolders])`: path relative to `baseFolder`, `~` expands, missing folder → error with line. Variables are a flat case-insensitive map; the loop variable holds a File value. A File's `path` follows planned `Rename`/`MoveTo` (Call 9); `.Tag` adds to its `tags()` (so a later `HASTAG` sees it) and is skipped with note when the file already has `MaxTags` Win12 tags or the tag is already present; `.Untag` of an absent tag is skipped "Tag not present"; `.Tag` colour must be a `TagColours::indexOfName` hit else error `Unknown colour X`; `.Rename` to a name that exists on disk (`QFileInfo::exists` in the file's current folder) or already planned in that folder → skipped; invalid name (empty, `/`) → error; `.Trash` then any further action on the same file → error `File was already trashed (line N)`. `MoveTo`/`CopyTo` destination = text resolved like `Files` paths; `MoveTo` onto the file's own folder → skipped "Already there". Each statement checks `cancel`.

- [ ] **Step 1: Tests** `ScriptInterpreterTest` (temp tree: `Downloads/invoice_mi13.pdf`, `Downloads/empty.txt` 0 B, `Downloads/keep.txt`, base = temp root, all paths relative; `today`-independent): `mockupScriptBuildsThreeSteps` (the mockup script with `"Downloads"` and `"Documents/Invoices/" & YEAR(f.Modified)` → plan [Move invoice_mi13.pdf → `Documents/Invoices/<year>`, Tag Orange/Invoice on it, Trash empty.txt]; `actionText()` of step 0 == "Move → Documents/Invoices/<year>"), `renameThenMoveUsesNewName` (`f.Rename "a.pdf"` then `f.MoveTo "Out"` → Move step source ends with `/Downloads/a.pdf`), `actionAfterTrashIsAnError` (error text contains "already trashed", `line` = the second action's line), `renameToExistingIsSkipped` (`f.Rename "keep.txt"` on empty.txt → skipped, note "Name exists"), `samePlannedNameIsSkipped`, `fourthTagIsSkipped` (QSKIP without xattr; file with 3 tags → skipped "Already has 3 tags"), `unknownColourIsError`, `stepLimit` (`maxStatements=5`, 10 files, `For Each f In Files("Downloads")` + one action → `ok=false`, error starts with "Script stopped after 5 statements", plan non-empty), `cancelStops` (lister seam returning 1000 rows; cancel flag set before run → error "Stopped"), `missingFolderIsError` (`Files("Nope")` → error with line 1), `variablesAndExitFor` (`n = 0` … `n = n + 1` … `If n = 2 Then Exit For` → 2 Tag steps), `subfoldersFlag` (`Files("Downloads", Subfolders:=True)` lists a nested file; without it, not), `statementsCounted` (3 files, body of 1 statement → `statements` == 1 + 3).
- [ ] **Step 2–4: Run → implement → PASS.** **Step 5: Commit** `explorer: ScriptInterpreter — builds a PlanStep list, 1,000,000-statement limit, cancel flag, virtual paths`.

### Task 9 [opus]: `PlanRunner` — KIO execution, progress, cancel, undo

**Files:**
- Create: `explorer/src/planrunner.h/.cpp`
- Test: `explorer/autotests/planrunner_test.cpp`; `explorer_add_test(planrunner_test)`

**Interfaces (produces):**
```cpp
class PlanRunner : public QObject {
    Q_OBJECT
public:
    PlanRunner(QWidget *window, TagStore *tags, QObject *parent = nullptr);
    void run(const QList<PlanStep> &);       // skipped rows ignored; one step at a time; emits progress(i, total) before each; stops at cancel, never at an error (a failed step is counted and the run continues — Call 13)
    void cancel();                            // after the current step
    bool isRunning() const; int executedCount() const;
    void undo();                              // reverses executed steps back to front; afterwards executedCount() == 0
    QString undoNote() const;                 // i18np("%1 trashed item stays in the Trash", …) when the executed steps include Trash, else empty
Q_SIGNALS:
    void progress(int done, int total);
    void finished(int done, int failed, const QString &firstError);
    void undone(int reversed, const QString &firstError);
};
```
Per step: **Move/Copy** — if `destFolder` does not exist, `KIO::mkpath(destFolder)` first (chained on `result`), then `KIO::move({source}, destFolder)` / `KIO::copy` with `KJobWidgets::setWindow(job, window)` (conflict dialog appears) and `FileUndoManager::recordCopyJob`; **Rename** — `ExplorerActions::rename(KFileItem(source), newName, window)` (refuses existing targets); **Tag/Untag** — `tags->add/remove({source}, tag)`, `TagStore::errorText` as the error; **Trash** — `KIO::trash({source})` + `recordJob(Trash, …)`. Undo per executed step: Move → `KIO::move({destFolder/name}, sourceFolder)`; Copy → `KIO::trash(copy)`; Rename → `rename(newUrl, oldName)`; Tag ↔ Untag via `TagStore`; Trash → not reversed (the note; the user's Ctrl+Z still works through FileUndoManager). Jobs run with `HideProgressInfo`; the dialog shows progress (Task 11).

- [ ] **Step 1: Tests** `PlanRunnerTest` (temp tree, `QSignalSpy` + `QTRY_VERIFY` on `finished`): `moveCreatesMissingFolder` (Move a.txt → `out/2026` which does not exist → file at `out/2026/a.txt`, `finished(1, 0, "")`), `copyRenameTrashTag` (one of each; Tag QSKIP without xattr; afterwards: copy exists, renamed name, trashed file gone, `KIO::FileUndoManager::self()->isUndoAvailable()`), `skippedRowsAreNotRun` (2 steps, one skipped → `finished(1, 0, …)`, `progress` emitted once with total 1), `failedStepContinues` (Rename to an existing name → `finished(1, 1, error non-empty)` with the other step done), `cancelStopsAfterCurrentStep` (3 moves; `cancel()` from the first `progress` → `finished(1, 0, …)`, files 2 and 3 untouched), `undoReversesExecuted` (Move + Rename + Tag run, `undo()` → original paths/names/tags back, `undone(3, "")`, `executedCount()` 0), `undoNoteForTrash` (after a Trash step `undoNote()` == "1 trashed item stays in the Trash"; `undo()` leaves it in the Trash).
- [ ] **Step 2–4: Run → implement → PASS.** **Step 5: Commit** `explorer: PlanRunner — plan steps as sequential KIO jobs with progress, cancel and undo`.

### Task 10 [sonnet]: `ScriptRun` worker

**Files:**
- Create: `explorer/src/scriptrun.h/.cpp`
- Test: `explorer/autotests/scriptrun_test.cpp`; `explorer_add_test(scriptrun_test)`

**Interfaces (produces):**
```cpp
class ScriptRun : public QObject {
    Q_OBJECT
public:
    explicit ScriptRun(QObject *parent = nullptr);
    void start(const QString &text, const QString &baseFolder); // parseScript on the caller's thread: a parse error -> finished() queued at once; else ScriptInterpreter::run in QtConcurrent::run with the shared cancel flag
    void stop(); bool isRunning() const;
Q_SIGNALS:
    void progress(qint64 statements, int planSteps); // at most 4 per second (the interpreter gets a std::function<void(qint64,int)> hook; add `setProgress(std::function<…>, int everyNStatements)` to ScriptInterpreter: called every 1000 statements)
    void finished(const ScriptResult &);             // never after stop()
};
```
Pattern: `tagsearch.cpp` (`shared_ptr<Run>`, queued `invokeMethod`, the worker never touches `this` after `stop()`).

- [ ] **Step 1: Tests** `ScriptRunTest`: `parseErrorFinishesImmediately` (`"For Each"` → `finished` with `ok=false`, `line` 1, within 300 ms), `mockupScriptProducesPlan` (same tree as Task 8 → `plan.size()` 3), `progressIsThrottled` (lister seam not available here — use 5000 small files via `QFile` touch; `progress` count ≤ 1 + elapsed_seconds*4), `stopEndsWithoutFinished` (start, `stop()`, no `finished` in 300 ms).
- [ ] **Step 2–4: Run → implement → PASS.** **Step 5: Commit** `explorer: ScriptRun — the interpreter off the UI thread with throttled progress and Stop`.

### Task 11 [sonnet]: `PlanView` and `ScriptDialog`

**Files:**
- Create: `explorer/src/planview.h/.cpp`, `explorer/src/scriptdialog.h/.cpp`
- Test: `explorer/autotests/scriptdialog_test.cpp`; `explorer_add_test(scriptdialog_test)` (covers PlanView)

**Interfaces (produces):**
```cpp
class PlanView : public QWidget {   // QTreeWidget (objectName "planTree", columns Action | File | Note; skipped rows grey, note in Note), status label ("planStatus"), QProgressBar ("planProgress", hidden when idle), buttons "Run plan (%1 steps)" ("runPlan"), "Stop" ("stopPlan"), "Undo plan" ("undoPlan")
    Q_OBJECT
public:
    PlanView(QWidget *window, TagStore *tags, QWidget *parent = nullptr);
    void setPlan(const QList<PlanStep> &);  // enables Run plan when any row is not skipped; button text uses the non-skipped count
    QList<PlanStep> plan() const;
    PlanRunner &runner();
Q_SIGNALS: void runStarted(); void runFinished(int done, int failed);
};
class ScriptDialog : public QDialog {  // "Script editor": QPlainTextEdit mono ("scriptEdit"), buttons "Build plan" ("buildPlan"), "Stop" ("stopScript"), "Save script…" ("saveScript"), status label ("scriptStatus"), then the PlanView; Close button
    Q_OBJECT
public:
    ScriptDialog(const QUrl &baseFolder, TagStore *tags, RuleLibrary *library, QWidget *parent);
    void setScript(const QString &); QString script() const;
    void buildPlan();                  // ScriptRun::start; status "Building plan… %1 statements, %2 steps" while running; on error "Line %1: %2" and the cursor moves to that line; on success the PlanView gets the plan and status "%1 steps" (i18np)
    PlanView *planView() const;
    void reject() override;            // stops the ScriptRun; refuses to close (message) while the PlanRunner runs
};
```
Build plan is disabled while a run is active or the base folder is not local (status "Scripts work on local folders only" — Call 6). Save script… → `QInputDialog::getText` → `library->save({name, text, Kind::Script})`. `Run plan` never starts by itself (anti-goal).

- [ ] **Step 1: Tests** `ScriptDialogTest`: `buildPlanFillsTable` (Task 8's tree; after `buildPlan()` `QTRY_COMPARE(planTree->topLevelItemCount(), 3)`, row 0 column 0 "Move → Documents/Invoices/<year>", `runPlan` text "Run plan (3 steps)"), `parseErrorMovesCursor` (`"If x Then\nf.Trash"` → status starts with "Line 1:", `textCursor().blockNumber()` 0), `runPlanExecutes` (click `runPlan` → `runFinished(3, 0)`, file moved, `undoPlan` enabled), `undoPlanRestores`, `nonLocalFolderDisablesBuild` (`smb://h/s` → `buildPlan` disabled, status text), `rejectStopsRun`, `saveScriptStoresScriptRule`.
- [ ] **Step 2–4: Run → implement → PASS.** **Step 5: Commit** `explorer: Script editor dialog with dry-run plan table, Run plan, Stop and Undo plan`.

### Task 12 [sonnet]: Ribbon ▸ Automate, actions, My rules / My scripts menus, right-click ▸ Win12 ▸ My rules

**Files:**
- Modify: `explorer/src/ribbonspec.h/.cpp` (`phaseBuilt(4)` true; the empty `Automate` group gets `B("adv_formula", ki18n("Formula"), "accessories-calculator", S::Big)`, `B("adv_script", ki18n("Script editor"), "text-x-script", S::Big)`, `B("adv_my_rules", ki18n("My rules"), "bookmarks", S::Small, true)`, `B("adv_my_scripts", ki18n("My scripts"), "bookmarks", S::Small, true)`), `explorer/src/exploreractions.h/.cpp`, `explorer/src/explorerwindow.cpp` (menu buttons get their menus like `home_delete`)
- Test: extend `ribbonspec_test.cpp`, `exploreractions_test.cpp`

**Interfaces (produces):**
```cpp
// ExplorerActions additions
void selectByFormula(const QString &formula = QString());            // Select by rule in Mode::Formula, Action::Trash default; non-empty formula: setFormula + find()
void renameMany(const KFileItemList &items, const QString &formula);  // overload: Mode::Formula preloaded
void openScript(const QString &text = QString());                    // ScriptDialog on folder(); non-empty text: setScript + buildPlan()
QMenu *myRulesMenu(const KFileItemList &items);                      // caller owns: Select rules -> selectByFormula(text); Rename rules -> renameMany(items.isEmpty() ? selection() : items, text); separator; Script rules -> openScript(text); empty library -> one disabled "(nothing saved yet)"
QMenu *myScriptsMenu();                                              // scripts only, same empty entry
RuleLibrary &library();
void setExecScriptDialog(std::function<void(ScriptDialog &)>);       // test seam, default exec()
```
Menus rebuild on `RuleLibrary::changed` (`QMenu::aboutToShow`). `contextMenu`: inside `addWin12`, after Rename many…, `win12->addMenu(myRulesMenu(items))` titled "My rules" when `library().all()` is non-empty. `updateEnabled`: the four actions follow the other `adv_*` ids (need a view). Dialogs get `setLibrary(&m_library)`.

- [ ] **Step 1: Tests** `RibbonSpecTest::automateGroupHasFourButtons` (ids in order, `adv_my_rules` and `adv_my_scripts` have `menu == true`, `phaseBuilt(4)`); `ExplorerActionsTest`: `selectByFormulaOpensFormulaMode` (seam: dialog `mode() == Mode::Formula`, `formula()` == given text), `myRulesMenuListsByKind` (library with Select "A", Rename "B", Script "C" → actions texts in order "A", "B", separator, "C"), `myRulesMenuEmptyEntry`, `contextMenuHasMyRulesWhenSaved` (Win12 submenu contains "My rules" only after a save), `openScriptLoadsText` (seam: `script()` == text).
- [ ] **Step 2–4: Run → implement → PASS.** **Step 5: Commit** `explorer: Advanced ▸ Automate — Formula, Script editor, My rules, My scripts; right-click ▸ Win12 ▸ My rules`.

### Task 13 [sonnet]: Dolphin right-click ▸ Win12 ▸ My rules (generated service menu) and `--rule`

**Files:**
- Modify: `explorer/src/rulelibrary.h/.cpp`, `explorer/src/cli.h/.cpp`, `explorer/src/main.cpp`, `explorer/src/explorerwindow.h/.cpp`
- Test: extend `rulelibrary_test.cpp`, `cli_test.cpp`

**Interfaces (produces):**
```cpp
// RuleLibrary
static QString serviceMenuPath();                 // GenericDataLocation + "/kio/servicemenus/win12-my-rules.desktop"
QString serviceMenuText() const;                  // the file's content; written (0755) after every successful save/remove; removed when the library is empty
// cli.h
struct CliOptions { ...; QString rule; };         // "--rule <name>": urls = the clicked folder (Select/Script) or files (Rename)
// ExplorerWindow
void runSavedRule(const QString &name, const QList<QUrl> &urls); // Select/Script kinds: open the folder (urls[0] when it is a folder, else its parent), then selectByFormula/openScript; Rename kind: renameMany(KFileItems of urls, text); unknown name -> KMessageBox::error "No saved rule named %1"
```
Service menu text (exact): `[Desktop Entry]\nType=Service\nMimeType=inode/directory;all/allfiles;\nX-KDE-Submenu=Win12 ▸ My rules\nActions=rule0;rule1;…\n` then per rule `[Desktop Action ruleN]\nName=<name>\nIcon=accessories-calculator|text-x-script\nExec=win12-explorer --rule "<name with " escaped as \">" %F\n`. `--rule` always opens in this process (a plain new window; no forwarding to a running instance — `// ponytail`).

- [ ] **Step 1: Tests** `RuleLibraryTest::serviceMenuWrittenOnSave` (save "Old CED files" → file exists, executable, text contains `X-KDE-Submenu=Win12 ▸ My rules`, `Actions=rule0;`, `Exec=win12-explorer --rule "Old CED files" %F`), `serviceMenuRemovedWhenEmpty`, `serviceMenuEscapesQuotes` (name `a"b` → `\"`); `CliTest::ruleOption` (`{"--rule", "Old", "/tmp/x"}` → `rule == "Old"`, one url).
- [ ] **Step 2–4: Run → implement → PASS** (the window hook is wired in `main.cpp` after the window shows; manual check: right-click a folder in Dolphin after a save).
- [ ] **Step 5: Commit** `explorer: Dolphin right-click ▸ Win12 ▸ My rules through a generated service menu and --rule`.

---

## Calls made

1. **File layout**: the engine lives in `explorer/src/formula/` (lexer, parser, evaluator, script parser, interpreter, `PlanStep`) and is globbed into `win12explorer`, no extra library — nothing outside the app needs it. Workers, runner, library and widgets stay flat in `explorer/src/` like phase 3.
2. **One value type `FxValue`** (tagged struct, not `std::variant`) with Excel's error codes as values (`#VALUE!`, `#NAME?`, `#DIV/0!`, `#N/A`, `#NUM!`); an error in one row never aborts the run, it shows red in that row.
3. **Numbers are doubles**; size literals (`10MB`) parse with the same table as the rule matcher's `parseSize`, so `SIZE > 10MB` and the Simple-rules "bigger than 10MB" agree. The leading `=` is optional.
4. **`And`/`Or`/`Not` keywords** are operators in both formula and script mode (lowest precedence, VBA-style), beside the Excel calls `AND()`/`OR()`/`NOT()` — one grammar for both languages. Text comparisons are case-insensitive like Excel's.
5. **Two Excel extras**: `DATE(y,m,d)` and `DATEVALUE("2026-01-31")`, so `MODIFIED < DATE(2026,1,1)` is writable; the spec says "Excel names wherever Excel has one". `DAYS(x)` with one argument is "days since x" (the mockup's `DAYS(MODIFIED)>30`); two arguments are Excel's `DAYS(end, start)`.
6. **Scripts work on local folders only** (`Files()` lists with `QDirIterator` in the worker; the dialog says so for `smb:/` etc.). Formulas list through KIO (`RuleSearch`) so they work anywhere, with empty `TAGS` where xattrs are unsupported.
7. **Columns on missing data**: `SIZE` of a folder is 0, `CREATED`/`TAKEN` unknown is Empty (date functions on it give `#VALUE!`), `WIDTH`/`HEIGHT` are 0 for non-images, `TAGS` is the encoded tags joined by `", "` so `CONTAINS(TAGS, "Work")` works. `EXT` is as on disk, no dot.
8. **Row order** for `ROW()`/`ROWINFOLDER()`: Select by rule sorts the listing by relative path with a numeric, case-insensitive `QCollator`; Rename many keeps the view order it was given (the dialog's existing contract).
9. **Virtual paths in scripts**: after `.Rename`/`.MoveTo` the file object's path is the planned one, so `f.Rename "a.pdf"` then `f.MoveTo "Out"` moves `a.pdf`; `.Tag` is visible to a later `HASTAG` in the same run. Nothing re-lists planned changes.
10. **Script grammar**: block `If … End If` only (no single-line `If x Then action`), `For Each … In Files(…)` is the only loop (the spec lists no `While`), plus `Exit For`, `Dim` and plain assignment. Keywords and names are case-insensitive.
11. **Step limit**: 1,000,000 executed statements per run (`ScriptInterpreter::DefaultMaxStatements`); the cancel flag is checked every statement; progress at most 4 times a second.
12. **No plasmoid setting** in this phase: the limit is a constant, saved rules live in `$XDG_DATA_HOME/win12/rules.json` beside `tags.json`.
13. **Run plan keeps going after a failed step** (counted and reported; a conflict dialog's "Skip" is not a failure); Stop ends after the current KIO job. **Undo plan** reverses the runner's own executed list (tag ↔ untag, rename back, move back, copy → Trash); trashed items are not restored and the dialog says so.
14. **Rename formulas yield the base name**, the extension is kept — the same contract as `RenamePlan::buildName`, so block mode and fx mode share `planNames`. Bad or duplicate results become grey rows with a Why note (folding in the phase 3 follow-up).
15. **Formula and column names stay English** (Excel's) and untranslated; everything else is `i18n`. The help dialog is generated from the function table, so it cannot drift.
16. **Dolphin's menu** is a generated `kio/servicemenus/win12-my-rules.desktop` rewritten on every save (one action per rule, `win12-explorer --rule "<name>" %F`); phase 3 had deferred the Dolphin entry, the spec asks for it, so it is the last task.
17. **Not built, follow-ups**: "Run last plan" ribbon button from the mockup; script syntax highlighting; `Do While` loops; a localized formula language.

## Self-review notes

- Spec coverage: row/column model + green result column (Tasks 1–3, 5); Excel syntax, size literals, all listed columns and functions (1, 2; `WIDTH/HEIGHT/TAKEN` in 3); formula in Select by rule and Rename many (5, 6); VBA-like script with the listed statements, members and actions (7, 8); plan only, "Run plan" with KIO, progress, cancel, conflicts, Undo (8, 9, 11); off-UI-thread evaluation and scripts with step limit and Stop (3, 8, 10, 11); renames never overwrite, delete = Trash (8, 9); saved formulas/scripts in Advanced ▸ My rules / My scripts, right-click ▸ Win12 ▸ My rules (4, 12) and Dolphin (13); our own parser, no shell (1, 7); no new settings keys (Call 12).
- Type consistency: `FxValue`, `FxExpr`, `FxParseError`, `FxFile`, `FxContext`, `FxRowResult`, `PlanStep`, `ScriptResult`, `SavedRule::Kind`, `SelectByRuleDialog::Mode`, `RenameManyDialog::Mode` are named identically across tasks; `ExplorerActions::rename` (static, phase 3) is what Tasks 9 and 8's note text rely on.
- Order: 1 → 2 → 3 (engine, then rows/worker); 4 independent; 5 needs 3+4; 6 needs 3+4; 7 needs 1; 8 needs 2+7; 9 needs 8's `PlanStep`; 10 needs 8; 11 needs 9+10+4; 12 needs 5, 6, 11; 13 needs 4+12.

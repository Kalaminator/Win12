# Win12 Explorer Phase 3 (Advanced, simple) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** the ribbon's Advanced tab gets Select by rule (rules → async result list → one action on the ticked items: Select, Copy to, Move to, Rename, Trash, Delete forever, Zip, Copy paths) and Rename many (name built from blocks, live preview, clash = skipped, batch undo), both also in Win12 Explorer's own right-click ▸ Win12 menu.

**Architecture:** Two pure, UI-free units with QtTest suites — `RuleMatcher` (compiled rule set, AND, `matches(KFileItem)`) and `RenamePlan` (blocks → planned names, clash detection, reverse plan for undo) — plus a small async `RuleSearch` (`KIO::listDir`/`KIO::listRecursive`, matching in the `entries` slot, so the UI never blocks on `/mnt/hdd`). Two QDialogs (`SelectByRuleDialog`, `RenameManyDialog`) only collect input and show the plan; the file work stays in `ExplorerActions` (existing KIO workers made public, recorded in `KIO::FileUndoManager`). The Advanced tab is five lines in `ribbonspec.cpp`; phase gating becomes `Ribbon::phaseBuilt(int)` because phase 3 ships before phase 2.

**Tech Stack:** C++20 (project default), Qt 6.12 Widgets/Test, KF6 6.31 KIO (Core/Widgets/FileWidgets), KI18n, QtTest offscreen. No new dependencies (zip goes through `ark` like `share_zip` does).

**Spec:** `docs/superpowers/specs/2026-10-23-win12-explorer-design.md` (Phase 3, Anti-goals, Constraints, Decided how). Mockups: `docs/superpowers/specs/2026-10-23-win12-explorer-mockups/batch-tools.html`, Advanced tab in `ribbon.html` (second ribbon). Phase 1 conventions: `docs/superpowers/plans/2026-10-23-win12-explorer-phase1.md`.

## Global Constraints

- C++17+, Qt 6, KDE Frameworks 6; CMake + ECM. Sources in `explorer/src/*.cpp` are globbed into `win12explorer`; each new test is one `explorer_add_test(<name>)` line in `explorer/CMakeLists.txt`.
- Build and test only in a scratch dir, never `install.sh`, never sudo:
  `B=/tmp/claude-1000/-home-kalaminator-Documents-Start12/79329225-ec69-40fe-b666-29df1031e186/scratchpad/build; cmake -S explorer -B $B -DCMAKE_BUILD_TYPE=Debug >/dev/null && cmake --build $B -j$(nproc) 2>&1 | tail -5 && ctest --test-dir $B --output-on-failure 2>&1 | tail -15`
- Anti-goals: nothing changes a file before the plan is shown (the result list / the Now → After preview); bulk delete defaults to Trash; renames skip existing names, never overwrite; listing and matching run through async KIO, never a synchronous directory walk on the UI thread.
- Rulings (given, not to be reopened): Phase 2 (Tags) is not built; the "tag" rule field, the "has tag" test and the "Add tag" action are left out — phase 2 adds them (`Field::Tag`, `Test::HasTag`, a `tag` button). The right-click ▸ Win12 entry exists only inside Win12 Explorer (via `ExplorerActions::contextMenu`); the Dolphin service-menu entry is a later task, out of scope. Phase 4 buttons in the mockup (Formula, Script editor, My rules, My scripts, Run last plan) and the Rename presets "Number files" / "Find & replace" are out of scope.
- Rulings made by this plan: (1) **Select** with "include subfolders" acts only when every ticked item is in the look-in folder itself; otherwise the Select button is disabled with the tooltip i18n("Select works on items of the look-in folder; the other actions work on all matches"). The spec's "results tab, like a search" needs a flat multi-folder model that Phase 4 (search) builds; this plan does not build a second view class for it. (2) **Undo rename** is the ribbon button `adv_rename_undo` (mockup: "Undo last rename"), enabled after a batch until the next batch or window close; it runs the reverse plan. Batch renames are not recorded in `KIO::FileUndoManager` (Ctrl+Z would otherwise undo one file of fifty). (3) "Keep subfolder structure" runs one `KIO::mkpath` + `KIO::copy`/`KIO::move` per distinct relative subfolder, each recorded for Undo. (4) Rule values: sizes accept `"10 MB"`, `"500 KB"`, `"2 GB"`, `"4096"` (bytes); dates accept a relative age `"30 days"`, `"2 weeks"`, `"6 months"`, `"1 year"` or an ISO date `"2026-01-01"`; text tests are case-insensitive (Windows behaviour). (5) Find & replace and Case blocks transform everything built left of them; Number advances per list position so a skipped row does not renumber the rest.
- Commit prefix `explorer:`. One fresh agent per task, ≤ ~100k tokens / ~40 tool calls; build and test output always `| tail`; no agent leaves background jobs.

## Review Focus

1. A regex the user is still typing (`"["`) must show one error line in the dialog, never throw or match nothing silently — `RuleMatcherTest::invalidRegexReportsError` (Task 2).
2. A rule search on a folder tree that takes seconds must keep the dialog repainting and be cancellable when the dialog closes — `RuleSearchTest::closingStopsTheJob` (Task 3).
3. Two items planned to the same new name, or a new name equal to another item's *current* name, must both be skipped, with the rest renamed — `RenamePlanTest::clashWithPlannedNameSkipsBoth` and `clashWithOtherCurrentNameIsSkipped` (Task 6).
4. Delete forever from the ⋯ menu asks with the count even when the user set "don't ask again" — reuse of `ExplorerActions::deletePermanently` (`ForceConfirmation`); pinned by `ExplorerActionsTest::deleteForeverGoesThroughForceConfirmation` (Task 5).
5. Undo after a batch where some renames failed on disk must only reverse the ones that happened, in reverse order — `RenamePlanTest::undoPlanReversesOnlyExecuted` (Task 6) and `ExplorerActionsTest::undoRenameBatchRestoresNames` (Task 8).

## File Structure

- Modify `explorer/src/ribbonspec.h/.cpp` — Advanced tab groups; `phaseBuilt(int)` replaces `currentPhase`. `explorer/src/ribbonbar.cpp:176,182` use it.
- Create `explorer/src/rulematcher.h/.cpp` — `Rule`, `Field`, `Test`, `parseSize`, `parseDateValue`, `RuleMatcher::compile/matches`. Pure.
- Create `explorer/src/rulesearch.h/.cpp` — `RuleSearch` QObject: async KIO listing, emits matches.
- Create `explorer/src/renameplan.h/.cpp` — `RenameBlock`, `RenamePlan::plan`, `RenamePlan::undoPlan`. Pure.
- Create `explorer/src/selectbyruledialog.h/.cpp` — look-in, rule rows, result list with ticks, action buttons.
- Create `explorer/src/renamemanydialog.h/.cpp` — block list (drag to reorder), Now → After preview.
- Modify `explorer/src/exploreractions.h/.cpp` — public item workers, `transferTo(items, move, keepStructureFrom)`, `zip`, `copyPaths`, `selectByRule(defaultAction)`, `renameMany(items)`, `undoRenameBatch()`, Win12 context-menu entries.
- Modify `explorer/src/folderview.h/.cpp` — `KFileItemList itemsInViewOrder(bool selectedOnly) const`.
- Tests: `explorer/autotests/rulematcher_test.cpp`, `rulesearch_test.cpp`, `renameplan_test.cpp`, `selectbyruledialog_test.cpp`, `renamemanydialog_test.cpp`; extend `ribbonspec_test.cpp`, `ribbonbar_test.cpp`, `exploreractions_test.cpp`, `folderview_test.cpp`.

---

### Task 1: Advanced tab in the ribbon spec, phase gating by built phases

**Files:**
- Modify: `explorer/src/ribbonspec.h` (replace `currentPhase`), `explorer/src/ribbonspec.cpp:99` (the Advanced tab), `explorer/src/ribbonbar.cpp:176-182`
- Test: `explorer/autotests/ribbonspec_test.cpp`, `explorer/autotests/ribbonbar_test.cpp`

**Interfaces:**
- Produces: `namespace Ribbon { constexpr bool phaseBuilt(int phase) { return phase == 1 || phase == 3; } }` (removes `currentPhase`). New action ids, all in tab `advanced`: group i18n("Select"): `adv_select_rule` (Big, "Select by rule", icon `edit-find`); group i18n("Rename"): `adv_rename_many` (Big, "Rename many", `edit-rename`), `adv_rename_undo` (Small, "Undo last rename", `edit-undo`); group i18n("Bulk actions"): `adv_select_matches` ("Select matches", `edit-select-all`), `adv_copy_matches` ("Copy matches to…", `edit-copy`), `adv_move_matches` ("Move matches to…", `edit-cut`), `adv_rename_matches` ("Rename matches…", `edit-rename`), `adv_trash_matches` (Big, "Trash matches", `user-trash`). Group i18n("Automate") stays `{}` with `phase 4`.
- `ExplorerActions` already builds one `QAction` per `Ribbon::actionIds()` entry, so the ids exist after this task (they do nothing until Tasks 5 and 8).

- [ ] **Step 1: Tests.** In `ribbonspec_test.cpp`: `advancedTabHasPhase3Groups` — `tab("advanced")->groups` titles are `{"Select", "Rename", "Bulk actions", "Automate"}`, the Automate group has `phase == 4`, `actionIds()` contains the eight `adv_*` ids above, `button("adv_trash_matches")->size == Size::Big`. In `ribbonbar_test.cpp`: rename `advancedIsPlaceholder` → `advancedShowsBuiltGroups`: `visibleGroupTitles(3) == {"Select", "Rename", "Bulk actions"}`; `tagsGroupHiddenInPhase1` keeps its assertion (phase 2 still hidden) and `everyVisibleButtonHasItsAction` filters with `!Ribbon::phaseBuilt(g.phase) || !Ribbon::phaseBuilt(t.phase)`.
- [ ] **Step 2: Run; expect compile failure** (`phaseBuilt` undefined) or `advancedTabHasPhase3Groups` FAIL.
- [ ] **Step 3: Implement** the spec rows (one `B(...)` per button) and `phaseBuilt`; `ribbonbar.cpp` uses `!Ribbon::phaseBuilt(tab.phase)` / `!Ribbon::phaseBuilt(g.phase)`; the "Advanced tools arrive…" label stays for unbuilt tabs (none now; keep the branch).
- [ ] **Step 4: Build + ctest; expect** `ribbonspec_test`, `ribbonbar_test`, `exploreractions_test` PASS.
- [ ] **Step 5: Commit** `explorer: Advanced tab groups for phase 3; ribbon gates on built phases, not a phase number`.

### Task 2: Rule matcher (pure)

**Files:**
- Create: `explorer/src/rulematcher.h`, `explorer/src/rulematcher.cpp`
- Test: `explorer/autotests/rulematcher_test.cpp`; `explorer_add_test(rulematcher_test)`

**Interfaces (produces):**
```cpp
enum class Field { Name, Extension, Size, Modified, Created, Kind }; // phase 2 adds Tag
enum class Test { StartsWith, Contains, EndsWith, Equals, Regex, OlderThan, NewerThan, BiggerThan, SmallerThan }; // phase 2 adds HasTag
struct Rule { Field field; Test test; QString value; };
QList<Test> testsFor(Field);                     // what the dialog's test combo offers per field
QString fieldText(Field); QString testText(Test); // i18n labels for the combos
qint64 parseSize(const QString &, bool *ok);     // "10 MB"→10485760, "500 KB", "2 GB", "4096"; ok=false otherwise
QDateTime parseDateValue(const QString &, const QDateTime &now, bool *ok); // "30 days" → now-30d; "2 weeks"; "6 months"; "1 year"; "2026-01-01" → that midnight
class RuleMatcher {
public:
    static std::optional<RuleMatcher> compile(const QList<Rule> &, QString *error, const QDateTime &now = QDateTime::currentDateTime());
    bool matches(const KFileItem &) const;       // AND over all rules; empty rule list matches everything
};
```
Semantics to pin: Name = `item.name()` (full name with extension), Extension = suffix without the dot (empty for folders); text tests case-insensitive, Regex uses `QRegularExpression` with `CaseInsensitiveOption`, "Equals" is whole-string; Size uses `item.size()` (folders never match size tests); Modified/Created use `item.time(ModificationTime/CreationTime)`, OlderThan = time < value; Kind: `Test::Equals` with value `"folder"` or `"file"` (`item.isDir()`). `compile` fails with the first error: invalid regex (`QRegularExpression::errorString()`), unparsable size or date, empty value for a text test.

- [ ] **Step 1: Tests** (one `QTemporaryDir` with `a.txt` 10 bytes, `B.TXT`, `big.bin` 3 MB, a folder `old/` with mtime set 60 days back via `QFile::setFileTime`; items via `KFileItem(QUrl::fromLocalFile)`): `nameStartsWithIsCaseInsensitive`, `extensionEqualsIgnoresDot`, `regexMatchesWholeOrPart`, `invalidRegexReportsError` (value `"["` → `compile` empty, error non-empty), `sizeBiggerThanParsesUnits` (`"2 MB"` matches big.bin only; folder never), `parseSizeRejectsGarbage`, `olderThanRelativeAge` (`"30 days"` matches `old/` only), `newerThanIsoDate`, `kindFolder`, `rulesAreAnded` (Name contains "t" AND Size smaller than "1 KB" → a.txt only), `emptyRulesMatchAll`.
- [ ] **Step 2: Run; expect** compile failure (header missing).
- [ ] **Step 3: Implement** `rulematcher.cpp` (compiled per-rule lambdas in a `QList<std::function<bool(const KFileItem&)>>`).
- [ ] **Step 4: Build + ctest; expect** `rulematcher_test` PASS.
- [ ] **Step 5: Commit** `explorer: rule matcher (name, extension, size, dates, kind; AND; tested)`.

### Task 3: Async rule search over KIO

**Files:**
- Create: `explorer/src/rulesearch.h`, `explorer/src/rulesearch.cpp`
- Test: `explorer/autotests/rulesearch_test.cpp`; `explorer_add_test(rulesearch_test)`

**Interfaces (produces):**
```cpp
class RuleSearch : public QObject {
public:
    explicit RuleSearch(QObject *parent = nullptr);
    void start(const QUrl &folder, bool recursive, const RuleMatcher &); // stops a running search first
    void stop();                                                           // kills the job, no finished()
    bool isRunning() const;
Q_SIGNALS:
    void found(const KFileItemList &);  // per KIO entries batch, already filtered, "." ".." dropped
    void finished(bool ok, const QString &error);
};
```
Uses `KIO::listRecursive(folder, KIO::HideProgressInfo)` or `KIO::listDir(...)`, `KIO::ListJob::entries` → `KFileItem(entry, folder, /*delayedMimeTypes*/ true, /*urlIsDirectory*/ true)` (recursive entries carry a relative `KIO::UDSEntry::UDS_NAME` like `sub/x.txt`; `KFileItem` resolves it against `folder`), `matches()` in the slot. Hidden files included (a Name rule can exclude them).

- [ ] **Step 1: Tests** (QTemporaryDir: `a.txt`, `sub/b.txt`, `sub/deep/c.log`): `nonRecursiveListsTopLevelOnly` (Extension equals `txt` → `{a.txt}`), `recursiveFindsNested` (→ `{a.txt, sub/b.txt}`, item urls are absolute), `closingStopsTheJob` (`start` then `stop()` → `isRunning()` false and no `finished` within 200 ms), `missingFolderFinishesWithError`. Wait with `QSignalSpy::wait`.
- [ ] **Step 2: Run; expect** compile failure.
- [ ] **Step 3: Implement** `rulesearch.cpp` (`QPointer<KIO::ListJob>`, `KJobWidgets` not needed — no UI delegate).
- [ ] **Step 4: Build + ctest; expect** `rulesearch_test` PASS.
- [ ] **Step 5: Commit** `explorer: RuleSearch lists through async KIO and emits matches`.

### Task 4: Select by rule dialog

**Files:**
- Create: `explorer/src/selectbyruledialog.h`, `explorer/src/selectbyruledialog.cpp`
- Test: `explorer/autotests/selectbyruledialog_test.cpp`; `explorer_add_test(selectbyruledialog_test)`

**Interfaces (produces):**
```cpp
class SelectByRuleDialog : public QDialog {
public:
    enum class Action { Select, CopyTo, MoveTo, Rename, Trash, DeleteForever, Zip, CopyPaths };
    SelectByRuleDialog(const QUrl &lookIn, Action defaultAction, QWidget *parent);
    // test / caller API
    void setRules(const QList<Rule> &); QList<Rule> rules() const;
    void setRecursive(bool);
    void find();                        // starts RuleSearch; the Find button and Enter in a value field call this
    Action chosenAction() const;        // valid after accept()
    KFileItemList ticked() const;       // in result-list order
    bool keepStructure() const;         // the "Keep subfolder structure" checkbox (enabled only when recursive)
    QUrl lookIn() const;
Q_SIGNALS:
    void searchFinished();              // after the last found() was appended
};
```
Layout (mockup `batch-tools.html` part 1): row "Look in:" `KUrlRequester` (directory mode, starts at `lookIn`) + `QCheckBox` i18n("Include subfolders"); rule rows (`QComboBox` field → `QComboBox` test from `testsFor(field)` → `QLineEdit` value (for `Kind` a `QComboBox` folder/file) → ✕ `QToolButton`), link-like button i18n("+ Add rule"), `QPushButton` i18n("Find"); an error `QLabel` (red) showing `compile`'s error, Find disabled while it is set; `QTreeWidget` columns Name (path relative to look-in, with icon), Modified, Size, every row `Qt::Checked`; label i18n("Found: %1 items · %2", ticked count, `KIO::convertSize(total)`), updated on tick changes. Button row: Select, i18n("Copy to…"), i18n("Move to…"), i18n("Rename…"), i18n("Move to Trash"), and a ⋯ `QToolButton` with menu i18n("Delete forever"), i18n("Compress to zip…"), i18n("Copy paths"). The button for `defaultAction` is `setDefault(true)`; all action buttons disabled until `ticked()` is non-empty; Select additionally disabled per ruling (1). Each action button sets `m_chosen` and calls `accept()`. `reject()`/destructor call `RuleSearch::stop()`.

- [ ] **Step 1: Tests** (QTemporaryDir as in Task 3): `findListsMatchesRelativeToLookIn` (`setRules({{Field::Extension, Test::Equals, "txt"}})`, `setRecursive(true)`, `find()`, wait `searchFinished`, result rows `{"a.txt", "sub/b.txt"}`, count label starts with "Found: 2 items"), `untickKeepsItem` (untick row 0 → `ticked()` is `{sub/b.txt}` and label "Found: 1 items"), `invalidRegexDisablesFind`, `defaultActionButtonIsDefault` (`Action::Trash` → the Trash `QPushButton::isDefault()`), `selectDisabledForSubfolderMatches` (ticked includes `sub/b.txt` → Select button disabled; untick it → enabled), `addAndRemoveRuleRows`.
- [ ] **Step 2: Run; expect** compile failure.
- [ ] **Step 3: Implement** the dialog; rule rows are a small private `struct RuleRow { QComboBox *field, *test; QLineEdit *value; QComboBox *kind; QToolButton *remove; }`.
- [ ] **Step 4: Build + ctest; expect** `selectbyruledialog_test` PASS.
- [ ] **Step 5: Commit** `explorer: Select by rule dialog (rules, async results, ticks, action buttons)`.

### Task 5: Actions on the matches, Advanced ribbon wiring, Win12 context-menu entry

**Files:**
- Modify: `explorer/src/exploreractions.h`, `explorer/src/exploreractions.cpp` (the `on("share_zip")` and `on("home_copy_path")` bodies at ~353 and ~425 move into the new public workers; `transferTo` at ~489; `addWin12` lambda at ~592; the selection-needing id list at ~46 gets the five `adv_*_matches` ids **not** added — they work on the folder, not the selection)
- Modify: `explorer/src/explorerwindow.cpp` — connect `ExplorerActions::selectUrlsRequested` to `activeView()->selectUrls(...)`
- Test: `explorer/autotests/exploreractions_test.cpp`

**Interfaces:**
- Consumes: `SelectByRuleDialog` (Task 4), `FolderView::selectUrls`.
- Produces (public on `ExplorerActions`):
```cpp
void transferTo(const KFileItemList &, bool move, const QUrl &keepStructureFrom = QUrl()); // asks for the folder; valid keepStructureFrom = ruling (3)
void trash(const KFileItemList &); void deletePermanently(const KFileItemList &);          // moved to public, unchanged
void zip(const KFileItemList &);        // ark --add --dialog --changetofirstpath <local paths>
void copyPaths(const KFileItemList &);  // one path per line, local path or display url
void selectByRule(SelectByRuleDialog::Action defaultAction); // opens the dialog on folder(); runs the chosen action on ticked()
Q_SIGNALS: void selectUrlsRequested(const QList<QUrl> &); // Action::Select → the window selects in the active view
```
`selectByRule` mapping: Select → `selectUrlsRequested(urls)`; CopyTo/MoveTo → `transferTo(items, move, dialog.keepStructure() ? dialog.lookIn() : QUrl())`; Rename → `renameMany(items)` (Task 8; until then a `// Task 8` no-op); Trash → `trash`; DeleteForever → `deletePermanently`; Zip → `zip`; CopyPaths → `copyPaths`. Ribbon: `adv_select_rule` → `selectByRule(Action::Trash)` (the spec's default button), `adv_select_matches` → `Action::Select`, `adv_copy_matches` → `CopyTo`, `adv_move_matches` → `MoveTo`, `adv_rename_matches` → `Rename`, `adv_trash_matches` → `Trash`. `updateEnabled`: the six are enabled whenever `folder().isValid()`. Context menu: inside `addWin12`, before `Q_EMIT fillWin12Menu`, add i18n("Select by rule…") → `selectByRule(Action::Trash)` (both the items menu and the folder-background menu), so the Win12 submenu now always exists.

- [ ] **Step 1: Tests** (`exploreractions_test.cpp`, offscreen, QTemporaryDir): `copyPathsJoinsWithNewline` (two items → clipboard text `"<p1>\n<p2>"`), `transferKeepStructureCreatesSubfolders` (items `sub/b.txt`, `a.txt` with `keepStructureFrom = dir`, destination chosen by a test seam: `static QUrl (*pickFolderForTest)(const QUrl &start)` hook or `QFileDialog` replaced by a `std::function<QUrl()> m_pickFolder` member defaulting to the dialog — pick the member; expect `dest/sub/b.txt` and `dest/a.txt` after `KIO::CopyJob` finishes, `FileUndoManager::undoAvailable` true), `transferFlatByDefault` (same items, no keepStructureFrom → `dest/b.txt`, `dest/a.txt`), `deleteForeverGoesThroughForceConfirmation` (the job created by `deletePermanently` is a `KIO::DeleteOrTrashJob` found via `findChild`; the test asserts its construction flag by checking that nothing is deleted before the confirmation — with offscreen the dialog is never answered: files still exist after 100 ms), `win12MenuHasSelectByRule` (both `contextMenu(items, folder)` and `contextMenu({}, folder)` contain a "Win12" submenu with action text "Select by rule…"), `advancedActionsEnabledWithFolder` (`setActiveView` on a view with a url → `action("adv_trash_matches")->isEnabled()`; without a view → disabled).
- [ ] **Step 2: Run; expect** compile failures (workers private / missing).
- [ ] **Step 3: Implement**; `keepStructureFrom`: group items by `KIO::relativePath(from, parentDir)`, for each group `KIO::mkpath(dest/rel)` then `KIO::copy|move(urls, dest/rel)` with `KJobWidgets::setWindow` and `recordCopyJob`; an empty relative path copies straight to `dest`.
- [ ] **Step 4: Build + ctest; expect** `exploreractions_test` PASS (and the rest).
- [ ] **Step 5: Commit** `explorer: actions on rule matches (select, copy/move with structure, trash, delete, zip, copy paths); Advanced ribbon and Win12 menu entry`.

### Task 6: Rename planner (pure)

**Files:**
- Create: `explorer/src/renameplan.h`, `explorer/src/renameplan.cpp`
- Test: `explorer/autotests/renameplan_test.cpp`; `explorer_add_test(renameplan_test)`

**Interfaces (produces):**
```cpp
struct RenameBlock {
    enum Kind { Number, Text, CurrentName, Date, FindReplace, Case };
    Kind kind;
    int digits = 2, start = 1, step = 1;              // Number
    QString text;                                     // Text; FindReplace: find; Date: format, default "yyyy-MM-dd"
    QString replace;                                  // FindReplace
    enum CaseMode { Upper, Lower, Title } caseMode = Title;
};
struct RenameItem { QUrl url; QString name; QDateTime modified; }; // "name" is the full current name
struct PlannedRename {
    QUrl url; QString oldName; QString newName;
    enum Status { Rename, Unchanged, ClashExisting, ClashPlanned } status;
};
namespace RenamePlan {
QString buildName(const QList<RenameBlock> &, const RenameItem &, int position); // base name only, no extension
QList<PlannedRename> plan(const QList<RenameBlock> &, const QList<RenameItem> &items /*view order*/, const QSet<QString> &existingNames);
QList<QPair<QUrl, QString>> undoPlan(const QList<PlannedRename> &executed); // (newUrl → oldName), reversed; only Status::Rename rows
}
```
Rules: extension (`QFileInfo(name).completeSuffix()` is NOT used — use `suffix()`, the last one, like `FolderView::displayName`; dot-files `.bashrc` have no extension) is re-appended with its dot after `buildName`; `CurrentName` contributes the base name; `Number` = `start + step*position` zero-padded to `digits` (never truncated); `Date` = `modified.toString(text)`; `FindReplace`/`Case` transform the accumulated string (ruling (5)), Title = first letter of each word upper, rest lower. `plan`: newName == oldName → `Unchanged`; newName ∈ existingNames minus the item's own name → `ClashExisting` (existingNames holds every name in the folder, files and folders); two rows with the same newName → both `ClashPlanned`; a newName equal to another item's *current* name → `ClashExisting` (it is in existingNames). An empty built base name is `Unchanged` (never renamed to just an extension); the dialog shows it grey.

- [ ] **Step 1: Tests**: `numberDigitsStartStep` (digits 3, start 5, step 2 → `005`, `007`), `textAndCurrentName` (`"Text - " + CurrentName` → `" - holiday"`), `extensionAlwaysKept` (`holiday.jpg` → `01 - holiday.jpg`; `archive.tar.gz` keeps `.gz` only; `.bashrc` stays `.bashrc` as whole base), `dateUsesModified`, `findReplaceAppliesToBuiltName`, `caseModes` (Upper/Lower/Title), `clashWithExistingNameIsSkipped` (existing `{"notes.txt","Pictures"}`, planned `notes.txt` → `ClashExisting`; a folder name clashes too — folds in the phase-1 follow-up "rename has no directory case"), `clashWithOtherCurrentNameIsSkipped`, `clashWithPlannedNameSkipsBoth`, `unchangedIsNotARename`, `numberFollowsListPosition` (row 1 skipped → row 2 still gets 03), `undoPlanReversesOnlyExecuted` (3 rows, middle `ClashExisting` → 2 pairs, last first).
- [ ] **Step 2: Run; expect** compile failure.
- [ ] **Step 3: Implement** `renameplan.cpp`.
- [ ] **Step 4: Build + ctest; expect** `renameplan_test` PASS.
- [ ] **Step 5: Commit** `explorer: rename planner (blocks, extension kept, clashes skipped, undo plan; tested)`.

### Task 7: Rename many dialog

**Files:**
- Create: `explorer/src/renamemanydialog.h`, `explorer/src/renamemanydialog.cpp`
- Test: `explorer/autotests/renamemanydialog_test.cpp`; `explorer_add_test(renamemanydialog_test)`

**Interfaces (produces):**
```cpp
class RenameManyDialog : public QDialog {
public:
    RenameManyDialog(const QList<RenameItem> &items /*view order*/, const QSet<QString> &existingNames, QWidget *parent);
    void setBlocks(const QList<RenameBlock> &); QList<RenameBlock> blocks() const; // in list order
    QList<PlannedRename> plan() const;   // what Rename will do; recomputed on every block edit / reorder
};
```
Layout (mockup part 2): "New name =" row is a `QListWidget` in `QListView::LeftToRight` flow, `InternalMove` drag-drop (native reorder, no custom drag code), one item per block with its summary text (e.g. `Number · 2 digits · from 1 · step 1`, `Text · " - "`); selecting a block shows its editor below (QSpinBox digits 1-6 / start / step; QLineEdit text; QLineEdit format; find + replace; case combo); "+ block" `QToolButton` menu with the six kinds; ✕ removes the selected block. Default blocks when none given: `{Number(2,1,1), Text(" - "), CurrentName}` (the mockup). Preview `QTreeWidget` columns i18n("Now") / i18n("After"); `ClashExisting`/`ClashPlanned` rows red with text i18n("%1 — a file with this name is already in the folder → skipped") / i18n("%1 — two items would get this name → skipped"); `Unchanged` grey. Label i18n("Order: as shown in the view"). OK button text i18n("Rename %1", count of `Status::Rename`), disabled at 0.

- [ ] **Step 1: Tests** (items `holiday.jpg`, `beach.jpg`, `notes.txt`; existing names `{…, "03 - notes.txt"}`): `defaultBlocksMatchMockup` (After column `01 - holiday.jpg`, `02 - beach.jpg`, third row red, OK text "Rename 2"), `reorderBlocksChangesPreview` (move CurrentName first via `setBlocks` reversed → `holiday - 01.jpg`), `editingBlockUpdatesPreview` (digits 3 → `001 - holiday.jpg`), `okDisabledWhenNothingToRename`.
- [ ] **Step 2: Run; expect** compile failure.
- [ ] **Step 3: Implement** the dialog (blocks stored in `QListWidgetItem::data(Qt::UserRole)` as a `QVariant` of a registered `RenameBlock` metatype, so a drag reorder reorders the data).
- [ ] **Step 4: Build + ctest; expect** `renamemanydialog_test` PASS.
- [ ] **Step 5: Commit** `explorer: Rename many dialog (blocks, drag reorder, live Now → After preview)`.

### Task 8: Run the rename batch, Undo last rename, ribbon and menu wiring

**Files:**
- Modify: `explorer/src/folderview.h/.cpp` — `KFileItemList itemsInViewOrder(bool selectedOnly) const` (rows of `currentView()->model()` top to bottom; selected = `m_selection->isRowSelected`)
- Modify: `explorer/src/exploreractions.h/.cpp`
- Test: `explorer/autotests/folderview_test.cpp`, `explorer/autotests/exploreractions_test.cpp`

**Interfaces:**
- Consumes: `RenameManyDialog`, `RenamePlan::undoPlan`, `ExplorerActions::rename(item, newName, parent)` (sync `KIO::moveAs`; note it records a `Rename` entry in `FileUndoManager` — acceptable per ruling (2), noted in a comment).
- Produces:
```cpp
void renameMany(const KFileItemList &items);   // opens RenameManyDialog(items as RenameItems, names in folder()), runs plan() rows with Status::Rename in order
void undoRenameBatch();                        // runs undoPlan(m_lastBatch) through rename(); clears m_lastBatch
// private: QList<PlannedRename> m_lastBatch;   // the rows that actually renamed (rename() returned empty)
QString ExplorerActions::runRenames(const QList<PlannedRename> &, QList<PlannedRename> *executed); // sequential; stops at the first error and returns it
```
Wiring: `adv_rename_many` → `renameMany(view->itemsInViewOrder(view->selectedCount() > 0))` (selection, else the whole folder); `adv_rename_undo` enabled iff `!m_lastBatch.isEmpty()`; `selectByRule`'s `Action::Rename` calls `renameMany(ticked)` (ticked items may span subfolders — `existingNames` is then the union of the names in each distinct parent folder, listed with `QDir::entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot)` for local urls, the planner is per-name so a clash check across folders is conservative, fine). Context menu: inside `addWin12`, for the items menu, i18n("Rename many…") → `renameMany(items)` (items ≥ 1). After a run with an error: `KMessageBox::error(m_window, error)`; what ran stays undoable. `// ponytail: sequential sync moveAs jobs; a progress dialog if 1000-file batches turn out slow on /mnt/hdd`.

- [ ] **Step 1: Tests**: `FolderViewTest::itemsInViewOrderFollowsSort` (3 files, sort by name descending → reversed names; `selectedOnly` with two selected → those two in view order). `ExplorerActionsTest::renameManyRunsPlanAndUndoRestores` — the dialog is driven by a test seam: `std::function<bool(RenameManyDialog &)> m_execRenameDialog` member (default `[](auto &d){ return d.exec() == QDialog::Accepted; }`); the test sets it to `setBlocks({Number(2,1,1), Text(" - "), CurrentName})` + return true; expect files `01 - a.txt`, `02 - b.txt` on disk, `action("adv_rename_undo")->isEnabled()`; then `undoRenameBatch()` → `a.txt`, `b.txt`, action disabled. `undoRenameBatchRestoresNames` with a pre-existing `02 - b.txt`: only `a.txt` renamed, undo restores only it. `win12MenuHasRenameMany` (items menu has it; folder-background menu does not).
- [ ] **Step 2: Run; expect** compile failures.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Build + ctest; expect** all tests PASS.
- [ ] **Step 5: Commit** `explorer: Rename many runs the plan, Undo last rename reverses the batch; Advanced ribbon and Win12 menu entries`.

---

## Self-review notes

- Spec coverage: Select by rule (Tasks 2-5), each action (Task 5; Rename via Task 8), "Add tag"/tag field deferred by ruling, Rename many blocks/drag/preview/clash/order/undo (Tasks 6-8), Advanced ribbon buttons incl. default-button mapping (Tasks 1, 5), right-click ▸ Win12 inside Win12 Explorer (Tasks 5, 8); Dolphin menu out of scope by ruling. Anti-goals: plan shown before any change (both dialogs), Trash default, no overwrite (planner + `rename()` refuses existing), async KIO listing (Task 3).
- Gap vs. spec: "Select" with subfolder matches → results tab, replaced by ruling (1); the controller may reopen it.
- Type consistency: `SelectByRuleDialog::Action` is used by `selectByRule`; `RenameItem`/`PlannedRename`/`RenameBlock` names are identical in Tasks 6-8; `itemsInViewOrder(bool)` in Tasks 8 only.
- Phase-1 follow-ups folded in: the rename directory-clash case (Task 6 test). Others do not touch phase-3 code.

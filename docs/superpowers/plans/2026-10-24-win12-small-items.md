# Win12 Explorer small-items wave Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Finish the pending small work before the AUR release: Dolphin's right-click ▸ Win12 ▸ Select by rule… / Rename many… (packaged, merged with the saved-rules entries), and every still-open deferred follow-up that a user can hit.

**Architecture:** A static service menu `explorer/win12-servicemenu.desktop` is installed by CMake into `kio/servicemenus`; it and the generated `win12-my-rules.desktop` share `X-KDE-Submenu=Win12`, which KIO merges into one submenu. Two new CLI flags open the dialogs on the clicked folder/files through the same "plain new window" path `--rule` uses. The follow-up fixes are small, local changes in the dialog, matcher, planner, view and formula code, each pinned by a test.

**Tech Stack:** Qt 6.6 / KF6 (KIO, KConfig/KDesktopFile), CMake/ECM, QtTest offscreen (`explorer/autotests`).

**Spec:** `docs/superpowers/specs/2026-10-23-win12-explorer-design.md` (line 141: "Both open from the Advanced tab and from right-click ▸ Win12 ▸ … in Dolphin and in Win12 Explorer"; line 164: Win12 does not change Dolphin's settings, toolbars or code). Follow-up lists triaged: `docs/superpowers/plans/2026-10-23-win12-explorer-phase1-followups.md`, `…-phase3-followups.md`, `2026-10-24-win12-explorer-phase2-followups.md`, `2026-10-24-win12-explorer-phase4-followups.md`.

## Global Constraints

- Sources: `explorer/src/*.cpp` are globbed into `win12explorer` (`src/formula/*.cpp` and `src/tags/*.cpp` likewise). Each new test is one `explorer_add_test(<name>)` line in `explorer/CMakeLists.txt`; test files live in `explorer/autotests/`. This wave adds no new test file: every test goes into the existing `*_test.cpp` named in its task.
- Build and test only in a scratch dir, never `install.sh`, never sudo, never `git stash`. Use **your own session's scratchpad**:
  `B=/tmp/claude-1000/-home-kalaminator-Documents-Start12/<session-id>/scratchpad/build; cmake -S explorer -B $B -DCMAKE_BUILD_TYPE=Debug >/dev/null && cmake --build $B -j$(nproc) 2>&1 | tail -5 && ctest --test-dir $B --output-on-failure 2>&1 | tail -15`. One test: `ctest --test-dir $B -R cli_test --output-on-failure 2>&1 | tail -20`.
- No test lock; every test is offscreen in its own process on its own `QTemporaryDir`. Tests touching the library call `QStandardPaths::setTestModeEnabled(true)` in `initTestCase`. Tests writing xattrs `QSKIP` when `KFileMetaData::UserMetaData::isSupported()` is false.
- **Spec rulings (binding):** Dolphin's settings, toolbars and code are never touched (only files under our own `kio/servicemenus`); renames never overwrite; delete means Trash; every UI string goes through `i18n()` (domain `win12-explorer`); no new plasmoid settings key; user text never reaches a shell (`Exec=` arguments are quoted and `%`-escaped, see Task 1).
- Commit prefix `explorer:`. One fresh agent per task, ≤ ~100k tokens / ~40 tool calls; build and test output always `| tail`; no agent leaves background jobs; commit at a safe point and stop with a handoff note when near the cap. Model per task is marked `[sonnet]` or `[opus]`.
- Work on a branch `small-items` off master; merge with `--no-ff` at the end (same flow as phase 4).

## Review Focus

1. A rule whose name holds `%` (`100% done`): `%` is a desktop-file field code, so the generated `Exec=` must write `%%` — test `serviceMenuEscapesPercent` (Task 1).
2. Dolphin right-click in a remote folder (`smb://nas/x`): `%U` hands URLs, `parseCli` keeps them as URLs, the dialog opens on that folder — test `dialogOptions` with an `smb://` url (Task 2).
3. `win12-explorer --select-by-rule` typed with no urls: the dialog opens on Home, nothing crashes — test `selectByRuleOnNoUrlsUsesHome` (Task 2).
4. Look-in edited after Find, then "Copy to… keep structure": the subfolder structure must be relative to the folder that was searched, not the edited text — test `lookInFrozenAtFind` (Task 4).
5. Rename many started from Dolphin's background click hands one folder url: one row, no error — test `renameManyOnSingleFolder` (Task 2).

---

## File Structure

- Create: `explorer/win12-servicemenu.desktop` — the static Dolphin menu (two actions).
- Modify: `explorer/CMakeLists.txt` — install it; `explorer/src/rulelibrary.h/.cpp` — same submenu, `%U`, padded keys, menu written on load, name uniqueness, `find`, `saveRuleInteractively`; `explorer/src/cli.h/.cpp`, `explorer/src/main.cpp`, `explorer/src/explorerwindow.h/.cpp` — the two flags and entry points, `ExplorerWindow::last()`; `explorer/src/selectbyruledialog.h/.cpp` — frozen look-in, Enter in results, Size sort; `explorer/src/rulematcher.cpp`, `explorer/src/renameplan.cpp`, `explorer/src/sortgroup.cpp`, `explorer/src/formula/fxeval.cpp`, `explorer/src/newtagdialog.cpp` — one-line fixes; `explorer/src/folderview.cpp`, `explorer/src/exploreractions.cpp` — selection order, clash names; the three dialogs' Save as… snippets (`selectbyruledialog.cpp:147`, `renamemanydialog.cpp:94`, `scriptdialog.cpp:70`).

---

### Task 1 [sonnet]: Static Dolphin service menu, merged with My rules

**Files:**
- Create: `explorer/win12-servicemenu.desktop`
- Modify: `explorer/CMakeLists.txt:41-43`, `explorer/src/rulelibrary.h`, `explorer/src/rulelibrary.cpp:96-165`
- Test: `explorer/autotests/rulelibrary_test.cpp`

**Interfaces:**
- Produces: `static constexpr QLatin1String RuleLibrary::kSubmenu{"Win12"}` (the `X-KDE-Submenu` both files use). `serviceMenuText()` keeps its signature. The static file's actions run `win12-explorer --select-by-rule %U` and `win12-explorer --rename-many %U` (flags implemented in Task 2; until then they are ignored by `parseCli` and open a plain window — harmless).

Static file content (exact):
```
[Desktop Entry]
Type=Service
MimeType=inode/directory;all/allfiles;
X-KDE-Submenu=Win12
Icon=system-file-manager
Actions=a-select-by-rule;b-rename-many;

[Desktop Action a-select-by-rule]
Name=Select by rule…
Icon=edit-select-all
Exec=win12-explorer --select-by-rule %U

[Desktop Action b-rename-many]
Name=Rename many…
Icon=edit-rename
Exec=win12-explorer --rename-many %U
```
Generated file changes in `serviceMenuText()`: `X-KDE-Submenu=Win12` (plain `kSubmenu`, no `i18n`), action keys `rule%1` zero-padded to 3 digits (`rule000`), `Exec=win12-explorer --rule "<name>" %U` with `"` → `\"` (exists) and `%` → `%%`. `ensureLoaded()` ends by calling `writeServiceMenu(*this, m_rules.isEmpty())` when the file's current content differs from `serviceMenuText()` (or is missing while rules exist), so an existing library gets the new format without a save.

- [ ] **Step 1: Tests** in `rulelibrary_test.cpp`: change `serviceMenuWrittenOnSave` to expect `X-KDE-Submenu=Win12`, `Actions=rule000;`, `Exec=win12-explorer --rule "Old CED files" %U`; add `serviceMenuEscapesPercent` (save name `100% done` → text contains `--rule "100%% done"`); add `serviceMenuWrittenOnLoad` (remove the menu file, write `RuleLibrary::path()` by hand with one Select rule as the existing tests' JSON shape, construct a fresh `RuleLibrary`, call `all()` → `QFile::exists(serviceMenuPath())`); add `staticMenuSharesSubmenu` (`KDesktopFile f(QFINDTESTDATA("../win12-servicemenu.desktop"))`; `f.desktopGroup().readEntry("X-KDE-Submenu") == RuleLibrary::kSubmenu`; `f.actions().size() == 2`; the two `exec()` strings end with `--select-by-rule %U` and `--rename-many %U`; `f.desktopGroup().readXdgListEntry("MimeType")` contains `inode/directory` and `all/allfiles`).
- [ ] **Step 2: Run** `ctest -R rulelibrary_test` → the four fail.
- [ ] **Step 3: Implement** the file, `install(FILES win12-servicemenu.desktop DESTINATION ${KDE_INSTALL_DATADIR}/kio/servicemenus)` next to the other `install()` lines, and the `rulelibrary.cpp` changes above.
- [ ] **Step 4: Run** the full ctest → all pass (no other test asserts the old submenu text; grep `My rules` in `autotests/` to be sure).
- [ ] **Step 5: Manual check** (no install): `mkdir -p ~/.local/share/kio/servicemenus && cp explorer/win12-servicemenu.desktop ~/.local/share/kio/servicemenus/ && chmod 755 ~/.local/share/kio/servicemenus/win12-servicemenu.desktop`, right-click a folder in Dolphin → one `Win12` submenu with Select by rule…, Rename many… and the saved rules; then remove the copied file again.
- [ ] **Step 6: Commit** `explorer: packaged Dolphin right-click ▸ Win12 menu, merged with My rules`.

---

### Task 2 [sonnet]: `--select-by-rule` and `--rename-many`, window entry points, last-window tracking

**Files:**
- Modify: `explorer/src/cli.h/.cpp`, `explorer/src/main.cpp:11-45`, `explorer/src/explorerwindow.h:86`, `explorer/src/explorerwindow.cpp` (`runSavedRule`, lines ~606-640)
- Test: `explorer/autotests/cli_test.cpp`, `explorer/autotests/explorerwindow_test.cpp`

**Interfaces:**
- Consumes: `ExplorerActions::selectByRule(SelectByRuleDialog::Action)`, `ExplorerActions::renameMany(const KFileItemList &)`, seams `setExecSelectDialog` / `setExecRenameDialog` (`exploreractions.h`).
- Produces:
```cpp
struct CliOptions { bool daemon, select, newWindow, selectByRule = false, renameMany = false; QString rule; QList<QUrl> urls; };
// ExplorerWindow
static QUrl folderFor(const QList<QUrl> &urls); // urls[0] when it is a local folder, else its parent; empty urls → Home (pulled out of runSavedRule)
void selectByRuleOn(const QList<QUrl> &urls);   // activeView()->openUrl(folderFor(urls)); m_actions->selectByRule(SelectByRuleDialog::Action::Select)
void renameManyOn(const QList<QUrl> &urls);     // opens folderFor(urls); m_actions->renameMany(KFileItems of urls); no urls → KMessageBox::information "Select the files to rename first" (same string runSavedRule uses)
static ExplorerWindow *last();                  // the newest living ExplorerWindow (set in the ctor, cleared in the dtor); replaces main.cpp's s_last so FileManager1-opened windows count too
ExplorerActions *actions() const;               // test accessor if m_actions is not already a findChild-able child
```
`main.cpp`: `if (!o.rule.isEmpty() || o.selectByRule || o.renameMany)` → one plain new window on `ExplorerWindow::folderFor(o.urls)`, `show()`, `raise()`, then `runSavedRule` / `selectByRuleOn` / `renameManyOn`; `--select` reuses `ExplorerWindow::last()`; delete `s_last`.

- [ ] **Step 1: Tests** `CliTest::dialogOptions` (`{"--select-by-rule", "smb://nas/x"}` → `selectByRule`, `urls == {QUrl("smb://nas/x")}`; `{"--rename-many", "/tmp/a", "/tmp/b"}` → `renameMany`, two urls). `ExplorerWindowTest::selectByRuleOnOpensFolderDialog`: `shownWindow()`, `setExecSelectDialog` records `d.lookIn()` and returns false; `w->selectByRuleOn({QUrl::fromLocalFile(m_dir.path() + "/x.txt")})` → recorded look-in == `m_dir.path()`. `selectByRuleOnNoUrlsUsesHome`: recorded look-in == `QUrl::fromLocalFile(QDir::homePath())`. `renameManyOnSingleFolder`: create `m_dir/sub`; `setExecRenameDialog` records `d.plan().size()` (or the row count accessor the dialog exposes — see `exploreractions_test.cpp:524`) and returns false; `renameManyOn({sub})` → 1 row. `runSavedRuleSelectKindOpensFormula`: save a Select rule `=ISFILE()` into the library (`w->actions()->library().save(...)`), `setExecSelectDialog` records `d.formula()`; `runSavedRule("r", {})` → `"=ISFILE()"`. `lastWindowTracksNewest`: two windows → `last() == second`; delete second → `last() == nullptr`.
- [ ] **Step 2: Run** `ctest -R 'cli_test|explorerwindow_test'` → fail (no such members).
- [ ] **Step 3: Implement** per the interfaces; keep `runSavedRule`'s body, only its folder logic moves into `folderFor`.
- [ ] **Step 4: Run** → PASS; full ctest passes.
- [ ] **Step 5: Commit** `explorer: --select-by-rule and --rename-many for Dolphin's Win12 menu; last window tracks FileManager1 windows`.

---

### Task 3 [sonnet]: Rule names unique across kinds; Save as… asks before replacing

**Files:**
- Modify: `explorer/src/rulelibrary.h/.cpp` (`save`, new `find`, new free function), `explorer/src/selectbyruledialog.cpp:140-150`, `explorer/src/renamemanydialog.cpp:88-97`, `explorer/src/scriptdialog.cpp:64-73`
- Test: `explorer/autotests/rulelibrary_test.cpp`

**Interfaces (produces):**
```cpp
std::optional<SavedRule> RuleLibrary::find(const QString &name) const;        // any kind
bool RuleLibrary::save(const SavedRule &, QString *error);                     // now also fails when the name exists under another kind: i18n("A %1 rule named %2 already exists; choose another name", kindText(kind), name) with kindText = i18n("Select") / i18n("Rename") / i18n("Script")
// rulelibrary.h, free function; the one Save as… flow the three dialogs call instead of their own QInputDialog + save + error-box snippet
bool saveRuleInteractively(RuleLibrary &lib, SavedRule::Kind kind, const QString &text, QWidget *parent);
// QInputDialog::getText(parent, i18n("Save as"), i18n("Name")); empty/cancel → false; same name+kind exists →
// KMessageBox::questionTwoActions(parent, i18n("Replace the saved rule %1?", name), …, KStandardGuiItem::overwrite(), KStandardGuiItem::cancel()) must be the primary action; save(); error → KMessageBox::error; returns save()'s result
```
Why this fixes phase 4 Task 13's "runSavedRule matches by name only": names become unique across kinds, so `--rule "<name>"` is unambiguous without a new flag.

- [ ] **Step 1: Tests** `saveRefusesNameOfOtherKind` (save `{"x", "1", Select}` then `{"x", "2", Script}` → false, error contains `x`; `all().size() == 1`), `saveReplacesSameKind` (two saves of `{"x", …, Select}` → one rule with the second text), `findByName` (`find("x")->kind == Select`, `find("nope")` empty).
- [ ] **Step 2: Run** → fail.
- [ ] **Step 3: Implement**; replace the three dialogs' snippets with `saveRuleInteractively(*m_library, Kind, text, this)`. The existing dialog Save-as tests (`saveAsStoresSelectRule`, the Rename/Script equivalents) drive a fresh name through the same `QInputDialog`, so they stay green; run them.
- [ ] **Step 4: Run** full ctest → PASS.
- [ ] **Step 5: Commit** `explorer: rule names unique across kinds; Save as asks before replacing a rule`.

---

### Task 4 [sonnet]: Select by rule dialog: frozen look-in, Enter in results, Size sort, Select opens the folder

**Files:**
- Modify: `explorer/src/selectbyruledialog.h/.cpp` (`lookIn()` 393-396, `find()` 398-425, `keyPressEvent` 221-231, result rows 156-161 / 192-198), `explorer/src/explorerwindow.cpp:467-470`
- Test: `explorer/autotests/selectbyruledialog_test.cpp`, `explorer/autotests/explorerwindow_test.cpp`

**Interfaces:** `QUrl SelectByRuleDialog::lookIn() const` now returns the folder of the last `find()` (`m_lookIn`, refreshed from the field at the top of `find()`), so `updateState()`'s gating and `ExplorerActions`' `keepStructure ? dialog.lookIn()` follow the search, not later edits. Enter with focus inside the results tree toggles the current item's check state (Qt::Checked ↔ Unchecked) and never reaches the default button. Size cells carry the byte count in `Qt::UserRole + 1`; a `SizeItem : QTreeWidgetItem` whose `operator<` compares that role when `treeWidget()->sortColumn() == 2` replaces `QTreeWidgetItem` for result rows (folders: 0). `ExplorerWindow`'s `selectUrlsRequested` handler calls `selectItems(urls)` (opens the urls' folder in the current tab first) instead of `activeView()->selectUrls(urls)`.

- [ ] **Step 1: Tests** (`selectbyruledialog_test.cpp`, using its `make()` / `runFind()` helpers and the `results` tree): `lookInFrozenAtFind` (runFind; set the `KUrlRequester` text to `/nowhere`; `d->lookIn()` still == the searched folder; `btnSelect` stays enabled for root-level ticks). `enterInResultsTogglesTick` (`make(Action::Trash)`, show, runFind, give the tree focus with `setCurrentItem(topLevelItem(0))`, `QTest::keyClick(tree, Qt::Key_Return)` → item 0 `checkState(0) == Qt::Unchecked`, `d->result() == QDialog::Rejected`, `d->isVisible()`). `sizeColumnSortsBySize` (test tree with files of 5 B, 2000 B and 300 B; runFind; `tree->sortItems(2, Qt::AscendingOrder)` → names in order 5, 300, 2000, i.e. not the text order "2000 B" < "300 B" < "5 B"). `explorerwindow_test.cpp`: `selectFromRuleOpensTheFolder` (create `m_dir/sub/f.txt`; `shownWindow()` on `m_dir`; `emit w->actions()->selectUrlsRequested({sub/f.txt})` → `w->activeView()->url()` == `sub` after `QTRY_COMPARE`).
- [ ] **Step 2: Run** → fail.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** full ctest → PASS (`enterNeverReachesActionButton` and `selectDisabledForSubfolderMatches` must stay green).
- [ ] **Step 5: Commit** `explorer: Select by rule keeps the searched look-in, Enter in results ticks, Size sorts by bytes, Select opens the folder`.

---

### Task 5 [sonnet]: One-line pure fixes (size overflow, trailing dot, future dates, fx int clamp, quote in tag words)

**Files:**
- Modify: `explorer/src/rulematcher.cpp:57-70`, `explorer/src/renameplan.cpp:8-12`, `explorer/src/sortgroup.cpp:10-16`, `explorer/src/formula/fxeval.cpp` (lines 340, 345, 353, 413), `explorer/src/newtagdialog.cpp:24`
- Test: `rulematcher_test.cpp`, `renameplan_test.cpp`, `sortgroup_test.cpp`, `fxeval_test.cpp`, `managetagsdialog_test.cpp` (or a `NewTagDialog` case there)

**Interfaces:** no signature changes. `parseSize`: `ok=false` when `value > std::numeric_limits<qint64>::max() >> (10 * shift)`. `dotPos`: a dot that is the last character is not an extension (`"a."` → base `"a."`, no extension). `modifiedGroup`: `d > n` → `{0, i18n("Today")}` (a future mtime is the newest thing, never "A long time ago"). `fxeval.cpp`: a local `static int toInt(double x)` returning 0 for NaN and clamping to `[INT_MIN, INT_MAX]` (`std::isnan`, `std::clamp`), used at the four `int(x)` sites. `NewTagDialog` validator regex `[^,/"]*` (a `"` would break the Baloo query string; `,` and `/` were already refused).

- [ ] **Step 1: Tests** `RuleMatcherTest::parseSizeRejectsOverflow` (`"99999999 TB"` → ok false; `"8191 TB"` → ok true); `RenamePlanTest::trailingDotIsNotAnExtension` (plan with one Text block `"x"` on `"a."` → new name `"x"`, the existing `extensionAlwaysKept` style); `SortGroupTest::futureDateIsToday` (`now.addDays(3)` → label `Today`, order 0); `FxEvalTest::hugeNumbersClamp` (`LEFT("abc", 1e300)` → `"abc"`; `MID("abc", 1e300, 1)` → `""`; `DATE(1e300, 1, 1)` → `#NUM!`); `ManageTagsDialogTest::newTagRefusesQuote` (`NewTagDialog d; d.findChild<QLineEdit*>()->setText("a\"b"); !hasAcceptableInput()`).
- [ ] **Step 2: Run** the five tests → fail.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** full ctest → PASS.
- [ ] **Step 5: Commit** `explorer: size overflow, trailing-dot names, future dates, fx int clamp, no quotes in tag words`.

---

### Task 6 [sonnet]: Selection in view order; Rename many clash check sees hidden and filtered names

**Files:**
- Modify: `explorer/src/folderview.cpp:713-726`, `explorer/src/exploreractions.cpp:816-835`
- Test: `explorer/autotests/folderview_test.cpp`, `explorer/autotests/exploreractions_test.cpp`

**Interfaces:** `FolderView::selectedItems()` returns items sorted by their row in `currentView()->model()` (sort `selectedRows()` by `row()` before the loop) — every caller (context menu ▸ Rename many, Cut/Copy, Trash) gets view order; the comment on `renameMany(items)` ("view order") becomes true from the context menu too. `renameManyImpl`: for every **local** folder (the active view's included) `existing` comes from `QDir(dir).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot)`; the view-model shortcut stays only for non-local folders (`!folder().isLocalFile()`), where it is the best we have.

- [ ] **Step 1: Tests** `FolderViewTest::selectedItemsInRowOrder` (in the existing 3-file dir: `selectUrls({c, a})` with the view sorted by name → `selectedItems()` names `{a, c}`); `ExplorerActionsTest::renameManyClashesWithHiddenFile` (dir with `a.txt` and a hidden `.b.txt`; a view on the dir without hidden files; `renameMany({a.txt})` with `setExecRenameDialog` setting one Text block `".b"` the way `exploreractions_test.cpp:524` builds blocks — extension kept gives `.b.txt` — then asserting that row's status is `PlannedRename::ClashExisting` and returning false).
- [ ] **Step 2: Run** → fail.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** full ctest → PASS (`itemsInViewOrder` tests untouched).
- [ ] **Step 5: Commit** `explorer: selection in view order; Rename many clash check reads the folder, hidden files included`.

---

## Calls made

1. **Static menu + shared submenu** — KIO (verified in `kfileitemactions.cpp`, KIO master: `selectList(priority, submenuName)` returns `userSubmenus[submenuName]`, a `QMap` keyed by the `X-KDE-Submenu` string; `insertServices` sorts each group by `actionsKey()`) merges every service-menu file with the same `X-KDE-Submenu` into one submenu. So both files use `X-KDE-Submenu=Win12` (untranslated brand, matching the in-app `Win12` submenu); the old `i18n("Win12 ▸ My rules")` would split the menu per locale. Static action keys `a-select-by-rule`, `b-rename-many` sort before `rule000…`, so the order is Select by rule…, Rename many…, then the rules in library order. No `_SEPARATOR_`: KIO's file order across directories is not something we pin, and the rule entries carry their own icons.
2. **`%U` not `%F`** in both files: `%F` makes KIO run `kioexec` for remote folders (phase 4 final follow-up); `parseCli` already keeps URLs.
3. **Tags… is not added to Dolphin's menu** — the spec line names Select by rule and Rename many only; there is no "pick tags for these files" dialog to open (in-app, Tags is a sibling menu of checkable actions), and Dolphin already shows the flags through the overlay plugin. Cost if wrong: one more flag later, same shape as Task 2.
4. **Background click = one folder url**: Dolphin cannot tell "the folder item" from "empty space" to a service menu; `Rename many…` on a background click shows one row for that folder. Accepted rather than two files or `X-KDE-RequiredNumberOfUrls` tricks.
5. **No `.desktop` translations**: `Name=` stays English in the static file; the `po/` flow covers `i18n()` only. Add `Name[xx]=` lines by hand when a translation exists.
6. **Name uniqueness instead of a `--rule-kind` flag** (Task 3): fixes the Select/Script same-name ambiguity with a library rule, no new CLI surface.
7. **Service menu rewritten on load** when its text differs (Task 1) — the format change must reach libraries saved under phase 4 without waiting for a save.
8. **`ExplorerWindow::last()`** replaces main.cpp's `s_last` so `--select` reuses a window that FileManager1 opened (phase 1 Task 10 follow-up) — one static in the class, no list.
9. **Enter in results toggles the tick** (Task 4) — nothing destructive can fire from the list; the default button still shows the right-click action.
10. **Future mtimes are "Today"** — Windows Explorer also files them under the newest group.
11. **F5 on tag-search results** (phase 2 Task 11): not planned — I could not confirm from the code that `refresh()` drops the results (`updateDirectory` runs behind the results source). Check by hand after this wave; if it does, the fix is `FolderView::refresh()` restarting `m_search` (needs the search's url/tags kept on the view).

## Follow-up triage

Legend: **fixed** = already in the code at aab1f6d (grep evidence), **planned** = a task above, **won't do now** = test gap / cosmetic / unverifiable / not user-visible.

### Phase 1 (`2026-10-23-win12-explorer-phase1-followups.md`)
- T1 install.sh `set -e` aborts on a failing test — won't do (install flow, plan-mandated).
- T1 build.sh `pacman -Q` guard — won't do (install.sh already guards with `command -v pacman`).
- T2 icon reuse; Big/Small test — won't do (cosmetic / test gap).
- T3 ViewSettingsStore range checks — won't do (columns go through `knownColumnIds().indexOf`, sizes through `qBound`; only a hand-edited config hits the rest).
- T4 future-dated mtimes — **planned** (Task 5). `mimeComment()` — fixed (T6 made Type sort cheap).
- T5 carried items (Type sort, `requestVisible`) — fixed (`folderview.cpp:450,456,558`). `m_now` — fixed (once per sort pass). Rest — won't do (perf/test gaps).
- T6 folder dropped onto itself — fixed (`folderview.cpp:354`). Wheel/zoom/List-mode — won't do.
- T7 shortcuts in nav pane, right-click activates the pane — fixed (`explorerwindow.cpp:436,533`). `home_edit == home_open`, dir rename test — won't do.
- T8 cosmetic — won't do.
- T9 queryClose, selectItems early-finished — fixed enough (`selectItems` checks `isFinished()`); won't do.
- T10 default-file-manager script: `off` deletes before restore — fixed; `on` writes saved file first — fixed (written last); saved value == our desktop — fixed; daemon never quits after Release — fixed (`filemanager1.cpp:72-78`).
- T10 FileManager1 windows not tracked for `--select` — **planned** (Task 2, `last()`). openFolders duplication, xdg-mime fallback — won't do.
- T11 taskbar pin off-swap at every start — fixed (`tests/test_explorer_default.sh:29`: swap only on output). Package removed while on; plasmoid po — won't do (plasmoid side).
- Final: forwarded `--daemon` opens a window — fixed (`main.cpp:33`).

### Phase 3 (`2026-10-23-win12-explorer-phase3-followups.md`)
- compile() ignores the test for Size/Date/Kind — won't do (the dialog never sends one).
- parseSize overflow — **planned** (Task 5).
- matcher / RuleSearch test gaps, default ctor — won't do.
- gating with the current look-in — **planned** (Task 4).
- Enter in the result list fires Trash — **planned** (Task 4; `keyPressEvent` only guards fields outside the tree).
- Select after changing look-in selects nothing — **planned** (Task 4).
- Size column sorts as text — **planned** (Task 4).
- delete-forever test, dialog test gaps — won't do.
- right-click Select by rule on the active pane — fixed (`page->activate(v)` before `contextMenu`).
- keep-structure mkpath not recorded for Undo — fixed (`exploreractions.cpp:576-595`, "each recorded for Undo").
- header includes, dead branch — won't do.
- trailing-dot "a." — **planned** (Task 5). Date format tests — won't do.
- Rename many clash check uses the filtered view model — **planned** (Task 6).
- grey rows show no reason — fixed (phase 4 Task 6).
- context-menu Rename many in selection order — **planned** (Task 6, root fix in `selectedItems()`).
- rename also lands in FileUndoManager — won't do: `undoRenameBatch` goes through `rename()` too, so Ctrl+Z after it undoes the undo — consistent, not stale.
- Rename many dialog test gaps — won't do.

### Phase 2 (`2026-10-24-win12-explorer-phase2-followups.md`)
- T1 decode trim — fixed (`tag.cpp:56`); paintColour lookup — won't do.
- T2 dedupe, colour range — fixed (`dropIfDuplicate`, `TagColours::Count` check); rewrite/merge timestamp — won't do.
- T3 control flow, replace-foreign MaxTags, tests — won't do (replace path keeps the 3-tag ceiling within one off-by-one; not data-losing).
- T4, T6, T7, T8, T12, T13 include order / perf / tests — won't do.
- T5 0-byte SVG, 1 px clip — won't do; live toggle in Dolphin — won't do (deliberate `// ponytail` in the plugin: F5).
- T9 check toggles when addTag fails — won't do (the menu closes; the error box is shown).
- T10 `q.exec()` before cancel — won't do; `"` in a tag word — **planned** (Task 5, refused at input).
- T11 double-connect — fixed by construction (a new `TagSearch` per search, `explorerwindow.cpp:266`); F5 on results — won't do now (Call 11); icons/emblems on results — won't do.
- T14 plasmoid stale value — won't do (plasmoid side, refreshes on next open).
- T15 AddTag default button — fixed (`setDefault(a == defaultAction)`).

### Phase 4 (`2026-10-24-win12-explorer-phase4-followups.md`)
- T1 depth limit — fixed (`fxparser.cpp:159` `Depth`); positional FILES, toText extremes, help i18n, test — won't do.
- T2 int casts on huge/NaN — **planned** (Task 5); TEXT numeric text, DATE rollover, Empty vs Date, countCache — won't do.
- T3, T10, T11 worker/test notes — won't do.
- T4 unstable sort, hand-edited duplicates, rollback tests — won't do.
- T5 Save as empty name — won't do (treated as cancel in `saveRuleInteractively`); weak test — won't do.
- T6 stale RED, inner edit name, "planned below" wording — won't do.
- T7, T8 messages, trash-by-path, symlink folders, bare Untag — won't do (documented behaviour).
- T9 empty folders after runner undo, Ctrl+Z of the runner's renames — won't do (consistent, no data loss).
- T12 openScript without a folder — won't do (action disabled).
- T13 name-only match — **planned** (Task 3); no menu until next save — **planned** (Task 1); unknown rule on an empty window — fine; `runSavedRule` untested — **planned** (Task 2 test).
- Final: `%F` via kioexec — **planned** (Task 1); fx Name on smb — fixed enough (`fxfile.cpp:131` uses `toDisplayString()` for non-local urls; only the dialog's relative-name display uses `toLocalFile()`, cosmetic); REGEX pinning a thread — won't do; Save silently replaces — **planned** (Task 3).
- Not built (Call 17 of phase 4): unchanged, still not built.

## Self-review notes

- Spec coverage: right-click ▸ Win12 ▸ Select by rule / Rename many in Dolphin (Tasks 1, 2); Dolphin untouched (only our `kio/servicemenus` files); saved rules under the same submenu (Task 1, Call 1).
- Type consistency: `CliOptions::selectByRule/renameMany` (Task 2) match the static file's flags (Task 1); `RuleLibrary::kSubmenu`, `find`, `saveRuleInteractively` are named identically in Tasks 1 and 3; `SelectByRuleDialog::lookIn()` keeps its signature (Task 4) so `ExplorerActions` needs no change there.
- Order: 1 → 2 (flags exist before the window acts on them); 3 independent of 2 but touches `rulelibrary.cpp` after 1; 4, 5, 6 independent. Six tasks, all `[sonnet]` — no engine work.

# Win12 Explorer chrome (title-bar tabs, no folder tabs, menu and tag fixes) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ribbon tabs in a client-drawn title bar on a frameless window, folder tabs removed, Share folded into Advanced, a Dolphin-complete right-click menu without doubles, tagging from a slide-out panel, and no "No tag" group header.

**Architecture:** Small pure changes first (group label, ribbon spec, context menu, tag panel), then the tab strip becomes one `FolderPage` per window with a two-key session, then the ribbon's tab strip is reparented into a new `TitleBar` widget that reads KWin's `[WM]` colours, and last the window goes frameless with its own move/resize/maximise handling and a KWin server-side shadow. Every step leaves the app running.

**Tech Stack:** Qt 6.6 / KF6 (KIO `KFileItemActions`, `KNewFileMenu`, KConfig/`KConfigWatcher`, `KColorScheme`, new: KWindowSystem `KWindowShadow`), CMake/ECM, QtTest offscreen (`explorer/autotests`).

**Spec:** `docs/superpowers/specs/2026-10-24-win12-explorer-chrome-design.md` (binding, incl. its "Calls made"); base spec anti-goals/constraints: `docs/superpowers/specs/2026-10-23-win12-explorer-design.md` lines 162-195.

## Global Constraints

- Sources: `explorer/src/*.cpp` are globbed into `win12explorer`. New source files need no CMake change; a new test is one `explorer_add_test(<name>)` line in `explorer/CMakeLists.txt`. This plan adds no new test file: every test goes into the existing `*_test.cpp` named in its task.
- Build and test only in a scratch dir, never `install.sh`, never sudo, never `git stash`. Use **your own session's scratchpad**:
  `B=/tmp/claude-1000/-home-kalaminator-Documents-Start12/<session-id>/scratchpad/build; cmake -S explorer -B $B -DCMAKE_BUILD_TYPE=Debug >/dev/null && cmake --build $B -j$(nproc) 2>&1 | tail -5 && ctest --test-dir $B --output-on-failure 2>&1 | tail -15`. One test: `ctest --test-dir $B -R exploreractions_test --output-on-failure 2>&1 | tail -20`.
- No test lock; every test is offscreen in its own process on its own `QTemporaryDir`. Tests touching the library call `QStandardPaths::setTestModeEnabled(true)` in `initTestCase`. Tests writing xattrs `QSKIP` when `KFileMetaData::UserMetaData::isSupported()` is false (existing helper `tagsUnsupported()`).
- **Spec rulings (binding):** Dolphin's settings, toolbars and code are never touched; delete means Trash; every UI string goes through `i18n()` (domain `win12-explorer`); no new plasmoid settings key; window move/resize only via `QWindow::startSystemMove()` / `startSystemResize()`; Dolphin entries come from KIO, filtered, never re-implemented when KIO has them; a right-click entry appears once; no "Open in New Tab" anywhere; tagging from the right-click menu never opens a separate window.
- Commit prefix `explorer:`. One fresh agent per task, ≤ ~100k tokens / ~40 tool calls; build and test output always `| tail`; grep then read small ranges of the big files (`explorerwindow.cpp` 859 lines, `exploreractions.cpp` 1092 lines); no agent leaves background jobs; commit at a safe point and stop with a handoff note (done / left / exact next step) when near the cap. Model per task is marked `[sonnet]` or `[opus]`.
- Work on branch `explorer-chrome` (this worktree); merge into master with `--no-ff` at the end.

## Review Focus

1. `kdeglobals [WM]` with only some keys (e.g. `activeBackground` but no `inactiveBackground`): each missing key alone falls back to the Header colour — test `readColoursMixesPerKey` (Task 7).
2. Right-click on a mixed selection (one folder + one file): no "Open in New Window" / "Open in Split View" (single folder only), "Copy Location" copies all paths — test `contextMenuMixedSelectionHasNoWindowEntries` (Task 6).
3. Saved session whose primary folder vanished but whose split `SecondUrl` exists: the window opens the survivor in one pane, no split onto Home — test `sessionDropsMissingPrimary` (Task 5).
4. Maximised window: the 6 px resize band is inert (no resize start, no resize cursor) — test `noResizeEdgesWhenMaximized` (Task 8).
5. Tag panel when every selected file already has 3 Win12 tags: colour clicks do nothing, the panel is disabled and says "max 3 tags" — test `tagsMenuGreysOutAtThree` (Task 4).

---

## File Structure

- Create: `explorer/src/tagpicker.h/.cpp` (word field + 48-colour grid, shared by the menu panel and `NewTagDialog`); `explorer/src/titlebar.h/.cpp` (client-side title bar: icon, ribbon tab strip, drag area, window buttons, `[WM]` colours); `explorer/src/windowshadow.h/.cpp` (KWin shadow tiles).
- Modify: `explorer/src/sortgroup.cpp/.h`, `explorer/src/folderview.cpp` (no-tag header); `explorer/src/ribbonspec.cpp`, `explorer/src/ribbonbar.h/.cpp` (Share group, strip hand-over, title colours); `explorer/src/exploreractions.h/.cpp`, `explorer/src/rulelibrary.h/.cpp` (context menu, filters, tag panel, Dolphin entries); `explorer/src/newtagdialog.h/.cpp`; `explorer/src/explorerwindow.h/.cpp`, `explorer/src/main.cpp`, `explorer/src/folderview.h` (tabs out, session, frameless); `explorer/CMakeLists.txt`, `explorer/PKGBUILD` (kwindowsystem, ark optdepend).
- Tests touched: `sortgroup_test`, `taggroupproxy_test`, `folderview_test`, `ribbonspec_test`, `ribbonbar_test`, `exploreractions_test`, `rulelibrary_test`, `explorerwindow_test`, `session_test`.

## Calls made (planner's calls; the design left these open)

- **Call 1:** Dolphin's "Open in New Window", "Open in Split View", "Copy Location" and "Open Terminal Here" are Dolphin's own code, not KIO services, so they are four small own entries reusing existing code (`copyPaths`, `openTerminal`, `FolderPage::setSplit`, the `ExplorerWindow` constructor). "Copy Location" *is* `home_copy_path` (same function, shown once, gets Ctrl+Alt+C). "Create New ›" is `KNewFileMenu`'s menu.
- **Call 2:** Win12's own service menus are filtered with KIO's `excludeList`, which (verified in `kfileitemactions.cpp`) matches `[Desktop Action <key>]` keys for service menus and plugin ids for plugins — KIO offers no file-name filter. `RuleLibrary::serviceActionKeys()` lists the keys of exactly the two files the design names (`win12-servicemenu.desktop`, `win12-my-rules.desktop`).
- **Call 3:** KIO *plugins* are enabled (`MenuActionSource::Plugins`) with a deny-list of the design's forbidden entries: `tagsfileitemaction` (Assign Tags), `setfoldericonitemaction` (folder colour), `kactivitymanagerd_fileitem_linking_plugin` (Activities). Compress › / Extract › are Ark's `compressfileitemaction` / `extractfileitemaction` plugins (Ark 26.08 installed here); other plugins keep showing like other apps' service menus.
- **Call 4:** Background menu: "Create New ›" replaces the separate "New folder" and "New item ›" entries (all three are the same `KNewFileMenu`); the background menu also gets "Copy Location" and "Open Terminal Here" for the folder itself.
- **Call 5:** The tag panel is a `QWidgetAction` inside the Tags submenu (design's first choice). The ribbon's Tags button reuses it (both go through `fillTagsMenu`). `NewTagDialog` stays only for Select by rule ▸ "Tag matches…" and wraps the same `TagPicker`.
- **Call 6:** "No tag" = a group with an empty label; the view delegate draws no header strip for an empty label (one guard, no new flag).
- **Call 7:** `TabPage` becomes `FolderPage` (one per window). Session = `[Session] Url` + `SecondUrl` (present ⇔ split); old `Tabs`/`Active` keys are ignored and deleted on the next save. Ctrl+W closes the window; several CLI urls open one window each.
- **Call 8:** Title bar height 32 px (= the tab strip's height); 6 px of title colour stay above the selected tab; the File tab keeps the accent fill (inset like the selected tab); the ribbon collapse button moves into the title bar left of the window buttons; window buttons are 46×32 with theme icons (`window-minimize`, `window-maximize`/`window-restore`, `window-close`); close hover `#c42b1c`, other hovers = foreground at 15 % alpha.
- **Call 9:** Colours: per-key fallback to `KColorScheme(…, KColorScheme::Header)`; live update via `KConfigWatcher` on `kdeglobals` group `WM` plus `QEvent::ApplicationPaletteChange` (covers the Header fallback).
- **Call 10:** Resize: a 6 px band at the window edges is detected with an application-level event filter (child widgets cover the edges), then `startSystemResize(edges)`; a 1 px outline in the title colour darkened (`darker(140)`) replaces KWin's frame; the band is inert while maximised.
- **Call 11:** Shadow: `KWindowShadow` (KF6 KWindowSystem) — KWin draws it server-side on X11 and Wayland without enlarging the window geometry, so snapping stays exact; on other compositors there is none. New dependency `kwindowsystem`.
- **Call 12:** The Share group keeps its `share_*` action ids (no renames, no i18n churn).

---

### Task 1 [sonnet]: No "No tag" header

**Files:**
- Modify: `explorer/src/sortgroup.cpp:70`, `explorer/src/sortgroup.h:16`, `explorer/src/folderview.cpp:239` (`stripHeight`)
- Test: `explorer/autotests/sortgroup_test.cpp:112`, `explorer/autotests/taggroupproxy_test.cpp:37,75`, `explorer/autotests/folderview_test.cpp` (uses its `groupByTag` helper at ~519)

**Interfaces:**
- Produces: `SortGroup::tagGroups({}, role)` returns `{{1000, QString()}}`; the view delegate's `stripHeight()` returns 0 whenever `index.data(FolderModel::GroupLabelRole).toString().isEmpty()`.

- [ ] **Step 1: Tests.** `sortgroup_test::noTagGroupLast`: add `QVERIFY(g.first().label.isEmpty())`. `taggroupproxy_test`: `noTag()` helper → assert in `untaggedInNoTagGroupLast` that it is empty and expect labels `{"x:u2", ":u1"}`. `folderview_test::untaggedFirstRowHasNoHeaderStrip`: two files, one tagged `Red/x` (skip when `tagsUnsupported`), `groupByTag(GroupRole::TagName)`; take the view's `currentView()->itemDelegate()`; `QStyleOptionViewItem o; o.widget = view.currentView(); o.font = view.font();` compare `sizeHint(o, firstTaggedRow).height() > sizeHint(o, firstUntaggedRow).height()` (the tagged group has a strip, the untagged one none; both rows have `GroupFirstRole` true).
- [ ] **Step 2: Run** `ctest -R "sortgroup_test|taggroupproxy_test|folderview_test"` → the three fail.
- [ ] **Step 3: Implement**: `return {{1000, QString()}};` in `tagGroups`; comment in `sortgroup.h:16` → `No tags -> {1000, ""} (drawn without a header)`; the empty-label guard as the first line of `stripHeight`.
- [ ] **Step 4: Run** full ctest → all pass (grep `"No tag"` in `src/` and `autotests/` → no hits left).
- [ ] **Step 5: Commit** `explorer: untagged files end the grouped view without a "No tag" header`.

---

### Task 2 [sonnet]: Share folds into Advanced

**Files:**
- Modify: `explorer/src/ribbonspec.cpp` (delete the `share` tab, add its group last in `advanced`), `explorer/src/ribbonbar.h:24` (comment: `0 Home, 1 View, 2 Advanced`)
- Test: `explorer/autotests/ribbonspec_test.cpp:26`, `explorer/autotests/ribbonbar_test.cpp:33-47`

**Interfaces:**
- Produces: `Ribbon::spec()` tabs `file, home, view, advanced`; `advanced` ends with group `i18n("Share")` holding `share_share, share_email, share_zip, share_print, share_terminal, share_security` (same `B(...)` lines, Share stays `S::Big` with a menu); group phase 1 (built). Action ids unchanged (Call 12).

- [ ] **Step 1: Tests.** `ribbonspec_test::tabOrder` → ids `{file, home, view, advanced}`, `spec()[3].phase == 3`; add `shareGroupLastInAdvanced`: `groupTexts("advanced").last() == "Share"`, its button ids in the order above, `Ribbon::button("share_share")->menu == true`, no tab with id `share`. `ribbonbar_test::tagsGroupShowsBuiltButtons` → `visibleGroupTitles(1)`; `advancedShowsBuiltGroups` → `visibleGroupTitles(2) == {Select, Rename, Bulk actions, Automate, Share}`.
- [ ] **Step 2: Run** `ctest -R "ribbonspec_test|ribbonbar_test"` → fail.
- [ ] **Step 3: Implement** the spec change; grep `setCurrentTab(` outside `ribbonbar.cpp` (none today) to be sure no caller uses the old indices.
- [ ] **Step 4: Run** full ctest → pass. Launch `$B/bin/win12-explorer` (or the built binary path) once: tabs File/Home/View/Advanced, Share group at the far right of Advanced.
- [ ] **Step 5: Commit** `explorer: Share tab folded into Advanced`.

---

### Task 3 [sonnet]: Right-click menu — Win12 submenu out, KIO plugins in, Win12 service menus filtered

**Files:**
- Modify: `explorer/src/exploreractions.cpp:1010-1092` (`contextMenu`), `explorer/src/exploreractions.h:87` (drop `fillWin12Menu`), `explorer/src/rulelibrary.h/.cpp` (`serviceActionKeys`, `ruleKey`), `explorer/PKGBUILD:8` (`optdepends=('ark: Compress and Extract in the right-click menu')`)
- Test: `explorer/autotests/exploreractions_test.cpp:400-520`, `explorer/autotests/rulelibrary_test.cpp`

**Interfaces:**
- Produces: `static QString RuleLibrary::ruleKey(int i)` → `rule%03d` (used by `serviceMenuText()` too); `QStringList RuleLibrary::serviceActionKeys() const` → `{"a-select-by-rule", "b-rename-many", ruleKey(0) … ruleKey(n-1)}`; `static const QStringList ExplorerActions::kHiddenPlugins = {"tagsfileitemaction", "setfoldericonitemaction", "kactivitymanagerd_fileitem_linking_plugin"}`. In `contextMenu` the items branch calls `fia->addActionsTo(menu, KFileItemActions::MenuActionSource::Services | KFileItemActions::MenuActionSource::Plugins, {}, m_library.serviceActionKeys() + kHiddenPlugins)`. The `addWin12` lambda, both calls, the `fillWin12Menu` signal and the now-unneeded separators go. `myRulesMenu()` stays (ribbon "My rules").

- [ ] **Step 1: Tests.** `exploreractions_test`: delete `contextMenuHasWin12OnlyWhenFilled`, `win12MenuHasSelectByRule`, `win12MenuHasRenameMany` and the `win12Entries` helper; rewrite `contextMenuHasMyRulesWhenSaved` → `contextMenuHasNoWin12Submenu` (save a Select rule; for items and for `{}`: no top-level action text `Win12` / `My rules`; keep the "Rename rule with nothing selected says so" block via `myRulesMenu({})`). Add a helper `QStringList allTexts(QMenu *)` (recursive through submenus). Add `contextMenuExcludesWin12ServiceMenus`: dir `QStandardPaths::writableLocation(GenericDataLocation) + "/kio/servicemenus"`; copy `QFINDTESTDATA("../win12-servicemenu.desktop")` there and write `other-app.desktop` (`Type=Service`, `MimeType=all/allfiles;inode/directory;`, `Actions=zzz-other;`, action `Name=Other app entry`, `Exec=true`), both `0755`; build `contextMenu({a.txt}, folder)`; `allTexts` contains `Other app entry`, not `Select by rule…`, not `Rename many…`; remove both files (also in `cleanupTestCase`). Add `contextMenuHidesForbiddenPlugins`: `QSKIP` unless `KPluginMetaData::findPluginById("kf6/kfileitemaction", "compressfileitemaction").isValid()`; `allTexts` contains `Compress`, none of `Assign Tags`, `Activities`, `Folder Colour`/`Change Folder Icon`; for `a.zip` written as the 22-byte empty-archive record (`"PK\x05\x06"` + 18 zero bytes) `allTexts` contains `Extract`. `rulelibrary_test::serviceActionKeysMatchFiles`: save two rules → keys `{a-select-by-rule, b-rename-many, rule000, rule001}`; every `KDesktopFile(QFINDTESTDATA("../win12-servicemenu.desktop")).actions()` name is in the keys; `serviceMenuText()` contains `[Desktop Action rule001]`.
- [ ] **Step 2: Run** `ctest -R "exploreractions_test|rulelibrary_test"` → fail.
- [ ] **Step 3: Implement** as in Interfaces; PKGBUILD optdepends.
- [ ] **Step 4: Run** full ctest → pass. Manual: right-click a file → no Win12 submenu, Compress › present, Extract › on a `.zip`; Dolphin still shows its Win12 submenu (unchanged files).
- [ ] **Step 5: Commit** `explorer: right-click menu without the Win12 submenu, with KIO plugins, Win12 service menus filtered`.

---

### Task 4 [sonnet]: Tags as a slide-out panel

**Files:**
- Create: `explorer/src/tagpicker.h`, `explorer/src/tagpicker.cpp`
- Modify: `explorer/src/newtagdialog.h/.cpp` (wrap `TagPicker`), `explorer/src/exploreractions.cpp:946-985` (`fillTagsMenu`)
- Test: `explorer/autotests/exploreractions_test.cpp:104-160,205-245`

**Interfaces:**
- Produces:
  ```cpp
  class TagPicker : public QWidget { Q_OBJECT
  public:
      explicit TagPicker(QWidget *parent = nullptr); // "Tag name (optional)" QLineEdit with validator [^,/"]* + 6x8 grid of 18 px colour buttons (TagColours order)
      Tag tag() const;                               // {checked colour or -1, trimmed word}
      void setTag(const Tag &);
      QLineEdit *wordEdit() const;
      QAbstractButton *colourButton(int i) const;
  Q_SIGNALS:
      void colourPicked(const Tag &);                // on a colour button click: {colour, current word}
  };
  ```
  `NewTagDialog` keeps `tag()` / `setTag()` / its `QDialogButtonBox` (OK enabled on `colourPicked`), body = one `TagPicker`. `fillTagsMenu(menu, items)` layout: `addSection(i18n("Your tags"))` + checkable rows (only when the registry has tags) · `addSection(i18n("New tag"))` · one `QWidgetAction` whose default widget is a `TagPicker` (disabled, with a `QLabel(i18n("max 3 tags"))` beside it, when `full`) · `Manage tags…`. `colourPicked(t)` → `addTag(items, t)` then `while (auto *p = qobject_cast<QMenu *>(QApplication::activePopupWidget())) p->hide();`. `menu->aboutToShow` → `QTimer::singleShot(0, picker->wordEdit(), qOverload<>(&QWidget::setFocus))`. No "New tag…" action anywhere.

- [ ] **Step 1: Tests.** Update `tagsMenuListsRecentFirstAndChecks`: texts of `actions()` are `[Your tags (separator), old, new, New tag (separator), <widget action>, Manage tags…]`; rows keep their checked/trigger behaviour. Update `tagsMenuGreysOutAtThree`: the widget action's `TagPicker` is `!isEnabled()`, its sibling label text `max 3 tags`, rows unchecked are disabled as before. Add `tagsMenuPanelAddsTagOnColourClick` (skip when unsupported): find the `QWidgetAction`, `picker = qobject_cast<QWidgetAction *>(a)->defaultWidget()->findChild<TagPicker *>()`; `QTest::keyClicks(picker->wordEdit(), "w")`; `picker->colourButton(3)->click()`; `TagStore::read(u) == {Tag{3, "w"}}`; `a.registry().contains(Tag{3,"w"})`. Add `tagsMenuHasNoNewTagDialogEntry`: no action text starting with `New tag…`. Move `newTagDialogRejectsCommaAndSlash` to `tagPickerRejectsCommaAndSlash` on a bare `TagPicker` (`keyClicks "a,b/c\"" → text "abc"`); keep `newTagDialogNeedsAColour` and `newTagDialogSeamAddsTag` as they are (API unchanged).
- [ ] **Step 2: Run** `ctest -R exploreractions_test` → fail (TagPicker missing).
- [ ] **Step 3: Implement** `TagPicker` (move the grid/validator code out of `newtagdialog.cpp`), rewire `NewTagDialog`, rewrite `fillTagsMenu`.
- [ ] **Step 4: Run** full ctest → pass. Manual (Wayland): right-click file ▸ Tags → panel beside the menu; type a word, click a colour → tag added, menu closes; ribbon View ▸ Add tag shows the same panel. If typing in the field is unreliable on Wayland, switch to the design's fallback (popup `QWidget` with `Qt::Popup` anchored at the Tags entry, closed with the menu) — note it in the handoff.
- [ ] **Step 5: Commit** `explorer: Tags submenu is a slide-out panel; New Tag dialog only for Tag matches…`.

---

### Task 5 [sonnet]: Folder tabs removed; one FolderPage per window; two-key session

**Files:**
- Modify: `explorer/src/explorerwindow.h` (whole class API below), `explorer/src/explorerwindow.cpp:222-337` (ctor), `:457-461` (shortcuts), `:530` (middle click), `:550-586` (open/close tab → delete), `:793-832` (eventFilter, queryClose, savedSession), `explorer/src/main.cpp:10-45`, `explorer/src/folderview.h:66` (comment)
- Test: `explorer/autotests/explorerwindow_test.cpp`, `explorer/autotests/session_test.cpp`

**Interfaces:**
- Produces: `class FolderPage` (sed-rename of `TabPage` in `src/` and `autotests/`, same API). `ExplorerWindow`:
  ```cpp
  explicit ExplorerWindow(const QUrl &url = {}, QWidget *parent = nullptr); // empty: the saved session
  FolderPage *page() const;                                                 // replaces currentPage(); openTab/closeTab/eventFilter removed
  static QList<QUrl> savedSession(const KSharedConfig::Ptr &); // [Session] Url (+ SecondUrl when present); no Url → {} (old Tabs key ignored)
  static QList<QUrl> sessionUrlsToOpen(const QList<QUrl> &saved); // unchanged: drops missing local folders; empty → Home
  ```
  Ctor: `urls = url.isValid() ? {url} : sessionUrlsToOpen(savedSession(m_config))`; `m_page = new FolderPage(&m_store, urls[0], splitter)` where `m_stack` was (members `m_tabs`, `m_stack` deleted; `paneOnly` actions go on `m_page`); `if (urls.size() == 2) { m_page->setSplit(true); m_page->secondary()->openUrl(urls[1]); }`; `wireView` both panes as `openTab` did; `activeChanged` → `syncToActiveView`. `KFilePlacesView::tabRequested` and `FolderView::folderMiddleClicked` → `(new ExplorerWindow(url))->show()`. Shortcuts: Ctrl+T, Ctrl+Tab, Ctrl+Shift+Backtab deleted; Ctrl+W → `close()`. `queryClose`: `writeEntry("Url", …)`, `SecondUrl` written when split else `deleteEntry`, `deleteEntry("Tabs")`, `deleteEntry("Active")`. `main.cpp`: `new ExplorerWindow(o.urls)` → one window per url (first is `w`); the other two sites compile as-is (`{QUrl}` → `QUrl`).

- [ ] **Step 1: Tests.** `explorerwindow_test`: delete `tabSwitchKeepsOtherTabsResults`, `ctrlTAndCtrlWOpenAndCloseTabs`; in `tagPickShowsResultsAndNavigationClears` replace `w->openTab(...)` with `w->activeView()->openUrl(QUrl::fromLocalFile(m_dir.path()))`; `splitMarksTheActivePane` → `w->page()`. Add `noTabBarAndCtrlTDoesNothing` (`!w->findChild<QTabBar *>()`, Ctrl+T → `ExplorerWindow::last() == w`), `ctrlWClosesWindow` (`QTRY_VERIFY(!w->isVisible())`), `middleClickOpensNewWindow` (`Q_EMIT w->activeView()->folderMiddleClicked(dir)` → `last() != w`, `last()->activeView()->url() == dir`; delete both), `sessionRestoresFolderAndSplit` (`w->page()->setSplit(true); w->page()->secondary()->openUrl(dir2); w->close(); delete w; auto *r = new ExplorerWindow(); primary url == dir, isSplit, secondary url == dir2`), `sessionDropsMissingPrimary` (Review Focus 3: write `[Session] Url=file:///nonexistent-xyz, SecondUrl=<dir>` into `KSharedConfig::openConfig()` → `new ExplorerWindow()`: `!isSplit()`, primary url == dir). `session_test::savedSessionRoundTrip` → write `Url`/`SecondUrl` → `{a, b}`; add `savedSessionUrlOnly` (→ size 1) and `savedSessionIgnoresOldTabs` (only `Tabs`+`Active` written → `savedSession(cfg).isEmpty()` and `sessionUrlsToOpen(...) == home`).
- [ ] **Step 2: Run** `ctest -R "explorerwindow_test|session_test"` → compile failure / fail.
- [ ] **Step 3: Implement**: `sed -i 's/\bTabPage\b/FolderPage/g' explorer/src/*.{h,cpp} explorer/autotests/*.cpp`, then the window changes; `grep -n "openTab\|currentPage\|m_tabs\|m_stack\|QTabBar\|tab" explorer/src/*.cpp` until only `tabStrip`/ribbon hits remain; `folderview.h:66` comment → `// opens a new window`.
- [ ] **Step 4: Run** full ctest → pass. Manual: start → last folder + split restored; middle-click a folder → new window; Ctrl+T does nothing; Ctrl+W closes.
- [ ] **Step 5: Commit** `explorer: folder tabs removed; one page per window; session = folder + split`.

---

### Task 6 [sonnet]: Dolphin's own entries in the right-click menu

**Files:**
- Modify: `explorer/src/exploreractions.h` (signals), `explorer/src/exploreractions.cpp` (shortcut table ~`:150-156`, `contextMenu`), `explorer/src/explorerwindow.cpp` (`connectActions`)
- Test: `explorer/autotests/exploreractions_test.cpp`, `explorer/autotests/explorerwindow_test.cpp`

**Interfaces:**
- Produces: signals `void openInNewWindowRequested(const QUrl &)`, `void openInSplitRequested(const QUrl &)`. Shortcuts: `home_copy_path` Ctrl+Alt+C, `file_terminal` Alt+Shift+F4 (check the table for clashes first). Items branch of `contextMenu`, when `items.size() == 1 && items.first().isDir()`: after `Open`: `Open in New Window` (icon `window-new`) → emit `openInNewWindowRequested(url)`; `Open in Split View` (icon `view-split-left-right`) → emit `openInSplitRequested(url)`. For every selection, after the Cut/Copy group: `Copy Location` (icon `edit-copy`, `setShortcut(Ctrl+Alt+C)`, `setShortcutVisibleInContextMenu(true)`) → `copyPaths(items)`; `Open Terminal Here` (icon `utilities-terminal`, Alt+Shift+F4 shown) → `openTerminal(single dir ? its url : folder)`, enabled only for local urls. Background branch: the `home_new_folder` and `home_new_item` entries are replaced by `QAction *create = menu->addMenu(m_newFileMenu->menu()); create->setText(i18n("Create New")); create->setIcon(document-new); create->setEnabled(inActiveFolder);` plus `Copy Location` (folder) and `Open Terminal Here` (folder) after Refresh. `ExplorerWindow::connectActions`: `openInNewWindowRequested` → `(new ExplorerWindow(url))->show()`; `openInSplitRequested` → `m_page->setSplit(true); m_page->secondary()->openUrl(url);`.

- [ ] **Step 1: Tests.** `exploreractions_test`: `contextMenuOnFolderHasDolphinEntries` (folder item: `allTexts` has each of `Open in New Window`, `Open in Split View`, `Copy Location`, `Open Terminal Here` exactly once; `QSignalSpy` on both signals, trigger each → one emission with the folder url); `contextMenuOnFileHasNoWindowEntries` (file: neither window entry; `Copy Location`/`Open Terminal Here` once); `contextMenuMixedSelectionHasNoWindowEntries` (Review Focus 2: folder + file → no window entries; trigger `Copy Location` → `QApplication::clipboard()->text()` holds both paths); `backgroundMenuHasCreateNew` (`contextMenu({}, folder)`: one `Create New` whose `menu()` is the `home_new_item` action's menu; no `New folder` / `New item` text; `Open Terminal Here` present); `dolphinShortcuts` (`action("home_copy_path")->shortcut() == Ctrl+Alt+C`, `action("file_terminal")->shortcut() == Alt+Shift+F4`); `contextMenuTextsAreUnique` (for folder, file and background menus: top-level non-separator texts have no duplicates). `explorerwindow_test::openInSplitOpensSecondPane` (`Q_EMIT w->explorerActions()->openInSplitRequested(dir2)` → `page()->isSplit()`, secondary url == dir2).
- [ ] **Step 2: Run** → fail.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** full ctest → pass. Manual: right-click folder / file / background; `Open in Split View` opens the second pane.
- [ ] **Step 5: Commit** `explorer: Dolphin's Open in New Window / Split View / Copy Location / Open Terminal Here / Create New in the right-click menu`.

---

### Task 7 [sonnet]: TitleBar widget with KWin colours (not yet in the window)

**Files:**
- Create: `explorer/src/titlebar.h`, `explorer/src/titlebar.cpp`
- Modify: `explorer/src/ribbonbar.h/.cpp` (`TabStripHeight`, `takeTabStrip`, `tabStrip`, `setTabStripColours`, strip painting)
- Test: `explorer/autotests/ribbonbar_test.cpp` (TitleBar tests go here too — it already links the ribbon)

**Interfaces:**
- Produces (RibbonBar): `static constexpr int TabStripHeight = 32;` (replaces `stripHeight` 28) · `QWidget *takeTabStrip()` (removes the strip from the ribbon's layout; the caller reparents it; the ribbon then holds only the body) · `QWidget *tabStrip() const` · `void setTabStripColours(const QColor &bg, const QColor &fg)` (default: palette window / windowText). Strip paint: fill `rect()` with `bg`; `TabInset = 6`; File tab = `pal.highlight()` from `y = TabInset`, text `HighlightedText`; selected tab = `pal.window()` from `y = TabInset`, text `WindowText`; unselected = no fill, text `fg`; no bottom line.
- Produces (TitleBar):
  ```cpp
  struct WmColours { QColor activeBg, activeFg, inactiveBg, inactiveFg; };
  class TitleBar : public QWidget { Q_OBJECT
  public:
      static constexpr int Height = RibbonBar::TabStripHeight;
      TitleBar(RibbonBar *ribbon, const KSharedConfig::Ptr &kdeglobals, QWidget *parent = nullptr); // takes ribbon->takeTabStrip()
      static WmColours readColours(const KSharedConfig::Ptr &kdeglobals); // [WM] activeBackground/activeForeground/inactiveBackground/inactiveForeground; each invalid key alone → KColorScheme(QPalette::Active|Inactive, KColorScheme::Header) background()/foreground()
      void reloadColours();            // re-read + strip colours + repaint; connected to KConfigWatcher::configChanged (group "WM") and run on QEvent::ApplicationPaletteChange
      void setActive(bool);  bool isActive() const;
      QColor background() const;  QColor foreground() const; // for the current active state
      QToolButton *minimizeButton() const;  QToolButton *maximizeButton() const;  QToolButton *closeButton() const;
      QRect dragArea() const;          // the stretch between the collapse button and the window buttons
  };
  ```
  Layout: app icon (`QIcon::fromTheme("system-file-manager")`, 16 px, 8 px margins) · the strip · the ribbon's collapse button (`QToolButton` on `ribbon->collapseAction()`) · stretch · three 46×32 autoRaise buttons (theme icons per Call 8; hover styles per Call 8 via a stylesheet set in `reloadColours`). Buttons: minimize → `window()->showMinimized()`; maximize → `window()->isMaximized() ? showNormal() : showMaximized()`; close → `window()->close()`. Mouse: press (left) on the drag area → `window()->windowHandle()->startSystemMove()`; double-click there → maximise toggle. `paintEvent` fills `background()`.

- [ ] **Step 1: Tests** (`ribbonbar_test`): `fileTabUsesAccent` keeps `(4, 14)`; add `unselectedTabShowsTitleColour` (`r.setTabStripColours(Qt::red, Qt::white)`; grab the strip; pixel at `(x of Home tab centre, 3)` == red; at `(same x, 20)` == `palette().window()`; pixel at `(x of View tab centre, 20)` == red). `clickingTabWhileCollapsedExpands` → click `r.tabStrip()` at `y = 20`. TitleBar: `readColoursFromWm` (temp `kdeglobals` with the user's values `232,17,35` / `255,255,255` / `221,126,141` / `245,216,221` → matching `QColor`s); `readColoursFallsBackToHeader` (empty config → equals `KColorScheme(QPalette::Active, KColorScheme::Header).background().color()` etc.); `readColoursMixesPerKey` (Review Focus 1: only `activeBackground` set → that one from `[WM]`, the other three from Header); `inactiveColours` (`setActive(false)` → `background() == inactiveBg`); `reloadColoursPicksUpChange` (write a new `activeBackground`, `sync()`, `reloadColours()` → `background()` changed); `titleBarHostsTheStrip` (`TitleBar tb(&ribbon, cfg)`: `tb.isAncestorOf(ribbon.tabStrip())`, `ribbon.findChild<QStackedWidget *>()` still there, `tb.height() == 32` after `show()`); `closeButtonClosesHost` (`QWidget host; auto *tb = new TitleBar(&ribbon, cfg, &host); host.show(); QTest::mouseClick(tb->closeButton(), Qt::LeftButton); QTRY_VERIFY(!host.isVisible())`).
- [ ] **Step 2: Run** `ctest -R ribbonbar_test` → fail.
- [ ] **Step 3: Implement** RibbonBar changes and `TitleBar` (`KConfigWatcher::create(kdeglobals)` kept as a member `KConfigWatcher::Ptr`).
- [ ] **Step 4: Run** full ctest → pass (the app is visually unchanged: the ribbon still owns its strip until Task 8).
- [ ] **Step 5: Commit** `explorer: TitleBar widget with KWin [WM] colours; ribbon tab strip can be handed over`.

---

### Task 8 [opus]: Frameless window with the title bar, move/resize/maximise

**Files:**
- Modify: `explorer/src/explorerwindow.h/.cpp` (ctor, new members/overrides), `explorer/src/ribbonbar.cpp` (strip's `mousePressEvent` must still reach `m_bar` after reparenting — it does, it holds the pointer)
- Test: `explorer/autotests/explorerwindow_test.cpp`

**Interfaces:**
- Produces: `static constexpr int ExplorerWindow::ResizeBorder = 6;` · `static Qt::Edges resizeEdgesAt(const QPoint &pos, const QSize &size, int border = ResizeBorder)` (pure: corners give two edges, `Qt::Edges()` inside) · `Qt::Edges edgesForPress(const QPoint &windowPos) const` (`Qt::Edges()` while `isMaximized() || isFullScreen()`, else `resizeEdgesAt`) · `TitleBar *titleBar() const`. Ctor: `setWindowFlag(Qt::FramelessWindowHint)` before anything is shown; `m_titleBar = new TitleBar(m_ribbon, KSharedConfig::openConfig(u"kdeglobals"_s), central)` inserted as the first row of the central layout (ribbon body second); `setContentsMargins(1, 1, 1, 1)`; `qApp->installEventFilter(this)`. `eventFilter`: for `MouseMove`/`MouseButtonPress` on a widget whose `window() == this`, map `globalPosition()` to this window, `edges = edgesForPress(pos)`; move → `QGuiApplication::setOverrideCursor(cursor for edges)` / `restoreOverrideCursor()` when leaving the band (track a bool); left press with edges → `windowHandle()->startSystemResize(edges); return true`. `changeEvent`: `WindowStateChange` → margins 0 when maximised else 1, maximize button icon `window-restore`/`window-maximize`; `ActivationChange` → `m_titleBar->setActive(isActiveWindow())`. `paintEvent`: 1 px rect in `m_titleBar->background().darker(140)` (skipped when maximised). `setWindowTitle` keeps running (taskbar).

- [ ] **Step 1: Tests** (`explorerwindow_test`): `windowIsFramelessWithTitleBar` (`windowFlags() & Qt::FramelessWindowHint`; `w->titleBar()`; `w->titleBar()->isAncestorOf(w->findChild<RibbonBar *>()->tabStrip())`; the ribbon body `QStackedWidget` sits directly below the title bar: `body->mapTo(w, {0,0}).y() == titleBar->mapTo(w, {0, titleBar->height()}).y()`); `resizeEdgesAtTable` (size 800×600, border 6: `(3,3)` → `Top|Left`, `(400,2)` → `Top`, `(798,300)` → `Right`, `(797,598)` → `Bottom|Right`, `(400,300)` → none, `(6,300)` → none); `noResizeEdgesWhenMaximized` (Review Focus 4: `w->showMaximized(); QTRY_VERIFY(w->isMaximized()); w->edgesForPress({2, 300}) == Qt::Edges()`); `maximizeButtonToggles` (click → `QTRY_VERIFY(w->isMaximized())`; click → `QTRY_VERIFY(!w->isMaximized())`); `closeButtonClosesWindow`; `windowTitleStillSet` (`windowTitle().endsWith("Win12 Explorer")`); `clickingViewTabInTitleBarSwitchesRibbon` (`QTest::mouseClick(strip, …, {x inside View tab, 20})` → `ribbon->currentTab() == 1`).
- [ ] **Step 2: Run** `ctest -R explorerwindow_test` → fail.
- [ ] **Step 3: Implement.** Keep `setAutoSaveSettings(u"MainWindow"_s)` (geometry restore works frameless).
- [ ] **Step 4: Run** full ctest → pass. **Manual (user, Wayland and X11 sessions):** drag the empty bar area moves the window; drag each edge and corner resizes with the right cursor; double-click the bar maximises/restores; drag to a screen edge snaps; Meta+Left tiles; the Close/Min/Max buttons work; Alt+F3 still opens KWin's window menu; focusing another window turns the bar to the inactive colour; System Settings ▸ Colours ▸ pick another title-bar colour → the bar follows live; File tab opens the backstage; ribbon collapse from the title bar.
- [ ] **Step 5: Commit** `explorer: frameless window with ribbon tabs in a client-drawn title bar`.

---

### Task 9 [sonnet]: KWin shadow for the frameless window; packaging

**Files:**
- Create: `explorer/src/windowshadow.h`, `explorer/src/windowshadow.cpp`
- Modify: `explorer/CMakeLists.txt:11,28-30` (`WindowSystem` in `find_package(KF6 …)`, link `KF6::WindowSystem`), `explorer/PKGBUILD:8` (`kwindowsystem` in depends), `explorer/src/explorerwindow.cpp` (one call in the ctor)
- Test: `explorer/autotests/explorerwindow_test.cpp`

**Interfaces:**
- Produces: `QImage WindowShadow::image(int radius, const QColor &colour)` — square `(2*radius+1)²`, transparent, then for `i = 0 … radius-1` fill the rect inset by `i` from the edges with `colour` at alpha `0.30 * ((i + 1.0) / radius)²` (soft fall-off; centre darkest) · `void WindowShadow::install(QWidget *window, int radius = 24)` — on the window's first `QEvent::Show` (event filter), slice the image into the 8 `KWindowShadowTile`s (corners `radius×radius`, edges 1 px wide/high), `KWindowShadow::setPadding(QMargins(radius, radius, radius, radius))`, `setWindow(window->windowHandle())`, `create()`; the shadow object is parented to the window; re-`create()` after `windowHandle()` changes. `ExplorerWindow` ctor ends with `WindowShadow::install(this)`.

- [ ] **Step 1: Tests** (`explorerwindow_test`): `shadowImageIsSoftAndSymmetric` (`image(8, Qt::black)`: size 17×17; `pixelColor(8,8).alpha() > pixelColor(1,1).alpha() > 0`; `pixelColor(0,8) == pixelColor(16,8)`; `pixelColor(0,0).alpha() < 20`); `shadowInstallIsHarmlessOffscreen` (`shownWindow()` with the shadow installed shows and deletes without crash — `create()` returns false offscreen).
- [ ] **Step 2: Run** → compile failure.
- [ ] **Step 3: Implement** + CMake/PKGBUILD.
- [ ] **Step 4: Run** full ctest → pass. Manual: visible soft shadow under KWin (Wayland and X11); snapping still exact against screen edges.
- [ ] **Step 5: Commit** `explorer: KWin shadow for the frameless window; kwindowsystem dependency`.

---

### Task 10 [fable]: Final review and merge

- [ ] Fresh reviewer reads the design's Goals 1-6, Anti-goals and "Calls made" against the branch diff (`git diff master...explorer-chrome -- explorer/`) and this plan's Calls; checks the user's manual-check list (Task 8 Step 4, Task 9 Step 4) was done; runs the full ctest once.
- [ ] Merge: `git checkout master && git merge --no-ff explorer-chrome -m "Merge explorer-chrome: Win12 Explorer chrome (title-bar tabs, no folder tabs, menu and tag fixes)"`.

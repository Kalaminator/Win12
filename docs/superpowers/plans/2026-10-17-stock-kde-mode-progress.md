# Stock KDE mode — progress log

Plan: `docs/superpowers/plans/2026-10-17-stock-kde-mode.md` (spec section 11). Entry format and rules: the plan's
"Driver protocol". Append one entry per run; the last entry's `next:` / `next-model:` lines drive the loop.

## 2026-10-17 08:25 Plan written
status: done
done: spec section 11 (modes, detection, the patch dependency table, engine rules, test model) + plan (6 tasks, no KDE package change) + this log.
left: nothing
notes: base 1f009d0. This PC runs the patched KDE and agents have no sudo: stock semantics are harness-only (Task 1's stock fake shell); Task 4 is the only live task and uses the marker file ~/.config/win12-force-stock (two plasmashell restarts through ./install.sh --restart). Pre-patch behaviour to reuse: `git show f70da6e:package/contents/ui/code/islands.js` (fix(), one step per pass, lost bounce) and the old syncCenterPanel in the same commit. Task 6 is the user's (stock packages, checklist in the plan, patched packages back from ~/.cache/win12-kde). Other agents edit islands.js / main.qml concurrently: re-read the lines you touch.
next: Task 1 — `STOCK` in `islands.js` + the stock fake shell + stock cases
next-model: opus

## 2026-10-17 12:53 Task 1: `STOCK` in `islands.js` + the stock fake shell + stock cases
status: done
done: b2cec9e. islands.js: `STOCK = @STOCK@`, a `did` flag set at every layout change (tray, order, mk, fresh, weather, removes, mirror adds), `stop()` (STOCK && did: saves win12Layout, out = again) after each phase; `order()` one index per pass on stock; `tray()` always a fresh copy on stock. Harness: `shell(w, screens, stock)`, `sh.stock`, `sh.endPass()`, `Panel.saved/lost`, `Widget.x`; `sub(..., STOCK = false)`; stock section with 5 cases (split-left, single, split-left again with win12TrayClean=true, split-right, patch lost).
left: nothing
notes: Deviation from the plan: the breaker is `did`, not `out === 'again'` (the One bar branch sets out = again on entry, so it never got past the helper removal; snapshot and re-key also set it without a layout change). stop() saves s first, else panels made in a broken pass were forgotten. Kept in one pass on purpose: the re-key's weather move + panel removal (the panel id leaves s there), removing all helpers, removing all island panels. Pass counts (harness, stock): split-left 16, split-right 15, single 11-12, all-monitors split-right 19. Probe (not committed): no index read or set after a layout change in the same pass. The patched engine run on the stock fake never converges (good: the fake catches it). LIVE IS BROKEN until Task 2: main.qml has no `-e s/@STOCK@/...` yet, so the installed engine would be a syntax error: do not run ./install.sh before Task 2 lands.
next: Task 2 — `main.qml` wiring, `centerLoop()` stock rules, `Tiles.PATCH_CHECK`, harness `center()`
next-model: opus

## 2026-10-17 13:20 Task 2: `main.qml` wiring, `centerLoop()` stock rules, `Tiles.PATCH_CHECK`, harness `center()`
status: done
done: 6d48646. tiles.js `PATCH_CHECK` (pacman -T + the marker `${XDG_CONFIG_HOME:-$HOME/.config}/win12-force-stock`); main.qml: `kdeChecked`, `islandsNeedPatch`, `onKdeCheckedChanged` runs both syncs once, both guards `!kdeChecked || isMirror || islandsNeedPatch`, sed `-e s/@STOCK@/!kdePatched/`, centerLoop `STOCK`: a spacer add or remove ends the pass, stock sets only the first mismatch (`miss`); TaskbarLayoutBox.qml imports tiles.js and uses `Tiles.PATCH_CHECK`. Harness: `center()` passes `kdePatched` (= !sh.stock), calls `sh.endPass()`, cap 25; 3 stock cases (mirror, undo with win12PanelOrder empty, split-left then One bar at Start=Right then center).
left: nothing
notes: Deviation: `islandsNeedPatch` uses `islandWant === "islands"`, not `cfg.taskbarLayout` (the layout box is hidden for other styles, so on stock a user who leaves the Windows 12 style still gets the one bar back; with W12 style on it is the same thing). Added to the stock fake (Task 1's): every add/move/remove/index set marks the pass (`sh.mut`, reset by endPass); an index read or set after one fails. Reason: the fake's setter saves at once, so the fast centerLoop on the stock fake still gave the right order in 2 passes; now it fails ("index read after a layout change"). islands.js passes the guard too. Stock pass counts: mirror 8, undo 7, after-restore 8. qmllint errors 0 (0 at base). PATCH_CHECK run here: "ok"; with the marker: nothing (stock). LIVE IS OK AGAIN: the sed now fills @STOCK@. TaskbarLayoutBox note text/visibility unchanged (still the old string, shown when !kdePatched): Task 3's job.
next: Task 3 — Settings note, README, FEATURES, 10 translations
next-model: sonnet

## 2026-10-17 13:45 Task 3: Settings note, README, FEATURES, 10 translations
status: done
done: 0151e3e. TaskbarLayoutBox.qml note (new text; red only when 3 Islands is picked, else normal colour; old string gone); README "Without the patched packages" paragraph; FEATURES l.71; po/template.pot + 10 po files translated (0 fuzzy, 580 translated each); 10 .mo rebuilt with the install.sh msgfmt line.
left: nothing
notes: qmllint errors 0; test_settings_keys --strict OK (141 keys). Task 4 is live (two plasmashell restarts via ./install.sh --restart, marker ~/.config/win12-force-stock; back up the three config files first).
next: Task 4 — Live forced-stock test on this (patched) PC
next-model: opus

## 2026-10-17 13:08 Task 4: Live forced-stock test on this (patched) PC
status: done
done: 1a0251e: main.qml `islandsNeedPatch` was a binding read inside onIslandWantChanged (and the islandPos/gap/... handlers) before it updated, so on stock One bar/Split -> 3 Islands BUILT the three islands; now `function islandsNeedPatch()` called by both guards. Live matrix passed after the fix.
left: nothing (Task 5: the harness cannot catch this kind of bug, it does not run QML; read the other guards that use derived bindings in change handlers)
notes: plasmashell restarted 3 times via ./install.sh --restart (marker on; marker on + the fix; marker off): one more than planned, for the fix. Claude runs in app-zellij@...service, not in plasmashell's cgroup: restarts are safe for the run. Start (user's desktop): split-right, Start pos 0, look default, autohide; host 2 = Start@0 pager@1 icontasks@2 separator@3 reveal@4; R 423 = weather@0 tray@1 clock@2 showdesktop@3 reveal@4; tray extra=19; offsets 2:559 423:866. Backups ~/.config/{plasma-org.kde.plasma.desktop-appletsrc,plasmashellrc,win12islandsrc}.bak-stockmode-20261017-1259 (not restored: not needed). Stock mode proved live by `pgrep -fc 'STOCK = true|@STOCK@/true'` = 2-4 during each switch (0 after the marker went). Stock timings (tests/tools/setlayout.sh, helper /tmp/claude-1000/t4/run.sh): split-right->split-left 1073 ms (3 panels, tray kept: no tray move needed), ->split-right 749, ->single 3326 (fresh tray 429, extra 19, Start@0 pager tasks sep tray clock showdesktop), single 2 mirror 2851 (showdesktop@0 clock tray sep spacer tasks pager Start@7), single 1 centre 3454 (spacer Start pager tasks spacer sep tray clock showdesktop), single 0 701; islands 0 BEFORE the fix built 3 islands in 4716 ms (the bug); after the fix + restart the 3 islands stayed and no sync ran (patch-lost case), islands->split-right 1094 (one task manager, 2 panels), split-right->islands: nothing changed (win12Layout and every widget identical after 4 s, no sync process), single 3324, ->split-right 3257 (fresh tray 537, extra 19). Faster than the planned 5-10 s; the harness counts more passes because its fake saves only at endPass. After every switch: neg (index < 0) = 0, tray extra = 19, journal grep win12/TypeError/ReferenceError = 0. Fast mode back (marker removed): split-left 742 ms, split-right 418 ms. End = start (same widget types and order, tray 19, offsets 2:559 535:866); new ids: R panel 535, weather 536, tray 537, reveal 569/570. A fresh panel made while hidden (autohide) reads length 12 and widget widths 0 until first shown; then it sizes and re-centres itself: not a bug. Settings note: not opened (unattended, no page selection in win12-settings); code check: shown when !kdePatched, red when cfg_taskbarLayout === "islands"; the page's PATCH_CHECK printed nothing with the marker (stock): UI left to the user for a look. Marker removed, pointer at the centre, no job left.
next: Task 5 — Whole-change review + ELI5
next-model: opus

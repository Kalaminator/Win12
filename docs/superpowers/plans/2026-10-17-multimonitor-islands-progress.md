# Multi-monitor islands — progress log

Plan: `docs/superpowers/plans/2026-10-17-multimonitor-islands.md` (spec section 10). Entry format and rules: the
plan's "Driver protocol". Append one entry per run; the last entry's `next:` / `next-model:` lines drive the loop.

## 2026-10-17 07:30 Plan written
status: done
done: plan + this log (6 tasks, none changes a KDE package: Panel.screen is writable and screenForConnector exists in stock plasma-workspace scripting; showOnlyCurrentScreen is a stock task manager key; the KWin API has screenAt/Output.name).
left: nothing
notes: base e8a51c7. One monitor only (HDMI-A-2 = id 0): multi-monitor logic is harness-only (tests/test_islands.js, 2-3 fake screens); live tasks never write a screen other than 0. Other agents edit islands.js / main.qml / kde/patches/plasma-desktop concurrently: re-read the lines you touch (line numbers in the plan are from e8a51c7).
next: Task 1 — Keys + `@SCR@/@ALL@/@SCREENS@` wiring; one chosen monitor in `islands.js`; harness screens
next-model: opus

## 2026-10-17 07:54 Task 1: Keys + `@SCR@/@ALL@/@SCREENS@` wiring; one chosen monitor in `islands.js`; harness screens
status: done
done: bab8ac9: main.xml keys islandScreen/islandAllApps (XP) + win12Mirror (General); main.qml islandScreen/islandAllApps/screenNames properties + 3 more sed -e in syncIslands; islands.js constants SCR/ALLAPPS/NAMES/sid/ALLMON/MAIN, s.scr saved, restore puts the host back on s.scr if still connected, placement moves host + islands to MAIN and uses screenGeometry(MAIN).width; harness shell(screenW, screens), sh.screens, sh.screenForConnector, sub(..., SCR, ALL) + 3 spec-10 cases.
left: nothing
notes: plan steps applied verbatim, no deviation. Checks: test_islands OK, tiles.js all checks passed, qmllint 0 errors, xmllint ok, the sed-substituted script parses (node --check) for SCR = "", all, DP-1 with no @X@ left. No live test (plan: Task 5). ALLAPPS is defined but unused until Task 2; with SCR = all, MAIN = 0 for now.
next: Task 2 — All monitors: host + app-island copies in `islands.js`; `isMirror` guards in `main.qml`
next-model: opus

## 2026-10-17 08:10 Task 2: All monitors: host + app-island copies in `islands.js`; `isMirror` guards in `main.qml`
status: done
done: 776c5c9: harness configKeys/configGroups follow currentConfigGroup + the plan's All-monitors cases (failed first at the [3,2,2] panel counts, as planned); islands.js mk('M') not filed in s, byPid/monPanels, restore also removes the copies, copyCfg/likeTM/s.mon block before the look block, others includes the copies, placement is place(k, row) per screen; main.qml isMirror + islandWant guard, isMirror returns in syncIslands/syncCenterPanel/syncPanelLook/syncDesktopTheme/syncIslandReveal, Connections on cfg.valueChanged -> islandTimer while islandScreen = all.
left: nothing
notes: Checks: test_islands OK, tiles.js all checks passed, qmllint 0 errors. Ruling: theme guard put in syncDesktopTheme (the caller of themeReader), not in themeReader.onNewData (that handler must disconnectSource first; the caller also skips the copy's firefox/icon syncs, which the host does) — cost if wrong: none seen. Ruling: also guard syncIslandReveal (not in the plan): a copy's islandWant is forced to single, so islandReveal = false and its onReady run would delete win12islandrevealEnabled and unload the host's KWin reveal script — cost if wrong: a copy can't touch the reveal script (it shouldn't anyway). Not guarded (same settings as the host, idempotent): syncWindowDeco/Colors/Blur/PanelPop/Icons-via-theme. For Task 6 review: does plasmashell's scripting panels() still list a panel whose screen was unplugged? If not, byPid misses the old copy and a new one is made (the old one leaks until the screen returns). Not testable here (one monitor).
next: Task 3 — Settings on the Taskbar page, Spanish, README
next-model: sonnet

## 2026-10-17 08:30 Task 3: Settings on the Taskbar page, Spanish, README
status: done
done: e87c4f6: ConfigTaskbar.qml cfg_islandScreen/cfg_islandAllApps + the two combo rows and hint label; template.pot + es.po (7 strings, 2 bad fuzzy guesses replaced), .mo rebuilt (573 translated, 0 fuzzy/untranslated); README paragraph.
left: nothing
notes: plan steps applied verbatim. Checks: qmllint 0 errors, msgfmt --check ok. Not seen live (settings dialog not opened).
next: Task 4 — KWin reveal script: the pointer's monitor only
next-model: opus

## 2026-10-17 08:00 Task 4: KWin reveal script: the pointer's monitor only
status: done
done: 32907b7: extras/win12-islandreveal main.js: reveal(name) per connector (screenForConnector in the sent plasmashell script, helpers on p.screen === sid true, the rest false), workspace.screenAt(cursorPos) || activeScreen for the edge and the docks filter (w.output.name), timer also starts when the pointer is on another monitor.
left: nothing
notes: Step 1 live: `win12-api screenAt=function out=HDMI-A-2 docks=HDMI-A-2,HDMI-A-2` (API as planned). Ruling: `let on;` (undefined) instead of the plan's `let on = null` — with null the closing `reveal(null)` returned early and never cleared a helper left on by a crash — cost if wrong: one extra D-Bus call at load. Checks: node --check OK; ./install.sh OK (tiles + islands tests pass), installed copy = repo; reload: no win12islandreveal/TypeError/ReferenceError lines; live split-right autohide (panels 2, 412 on screen 0): pointer on row 1079 -> 2:true 412:true, centre after 1.5 s -> 2:false 412:false. Configs backed up to the session scratchpad bk-task4 (not restored: nothing changed but the reveal keys, now false). plasmashell not restarted (the upgraded plasmoid QML from Tasks 1-3 runs after the next restart; Task 5).
next: Task 5 — Live single-monitor regression
next-model: opus

## 2026-10-17 08:05 Task 5: Live single-monitor regression
status: done
done: no code change (nothing to fix), so no code commit; this entry only.
left: nothing
notes: Step 1 (user's desktop): split-right, Start pos 0, look default (no taskbarLook key), autohide; panel 2 host = Start@0 pager@1 icontasks@2 separator@3 reveal@4; panel 412 R = weather@0 tray@1 clock@2 showdesktop@3 reveal@4; tray extra=19; offsets 2:577 412:830. Backups: ~/.config/{plasma-org.kde.plasma.desktop-appletsrc,plasmashellrc,win12islandsrc}.bak-t5mm-20261017-0800. Step 2: ./install.sh --restart OK (tiles + islands tests pass), no win12/TypeError lines in the journal. Step 3: `0 1 2:0 412:0`, kscreen-doctor HDMI-A-2 count 1: the two name sources agree. Step 4 matrix (all panels :0 every time): (1) islands 0 took 813 ms, 3 panels, host 610 < A 917 < T 999; (2) islandScreen=HDMI-A-2: same ids, same offsets after 3 s; (3) islandScreen=all: still 3 panels, win12Layout "mon":{}; (4) islands 2 took 751 ms, T 610 < A 913 < host 995; (5) islandScreen='' + single 0 took 1089 ms, 1 panel Start@0 pager@1 tasks@2 separator@3 tray@4 clock@5 showdesktop@6, tray extra=19 (weather goes in One bar, by design); (6) back to split-right 0 took 835 ms, reveal: pointer at 960,1079 -> 2:true 423:true, centre 1.5 s -> 2:false 423:false, no reveal-script errors. Step 5: UI not opened (unattended): `Show islands on` count 1 in ConfigTaskbar.qml, installed copy = repo, plasmoid installed — UI shown to the user for a look. Step 6: restored = Step 1 (same widget types and order, tray extra=19; R panel/weather/helper ids are new: 423/424/425/426, because One bar removed them; offsets 2:559 423:866 differ from Step 1 only by content width: host +54 px = one more task icon). Side facts: the old win12Layout (made before Task 1) had no "scr"; line 69's `s.scr !== undefined` guard covered it; the new one has "scr":0. `islandScreen=` (empty = default) is now written in [XP]. Screenshots /tmp/claude-1000/t5mm-{1,4,5,6}.png show no bar (autohide). Pointer left at the centre, no job running.
next: Task 6 — Whole-change review + ELI5
next-model: opus

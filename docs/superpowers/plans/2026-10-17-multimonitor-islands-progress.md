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

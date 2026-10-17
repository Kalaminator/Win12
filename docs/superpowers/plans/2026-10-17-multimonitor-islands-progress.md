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

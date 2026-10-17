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

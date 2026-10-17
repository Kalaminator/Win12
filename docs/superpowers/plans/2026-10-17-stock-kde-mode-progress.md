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

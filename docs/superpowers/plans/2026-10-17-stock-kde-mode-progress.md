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

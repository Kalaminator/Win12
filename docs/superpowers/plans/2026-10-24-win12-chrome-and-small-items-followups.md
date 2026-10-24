# Win12 Explorer chrome + small-items: deferred follow-ups

Minor findings from the two waves (merged at 66593e8). None loses data.

## Small items
- Task 1: reviewed by controller inline (diff small): ok. minor: ensureLoaded() rewrites/removes the menu file on every load when the library is empty and no file exists (cheap no-op)
- Task 2: minor (deferred): async remote stat path untested (no smb in tests); last() becomes null when the newest window closes even if older windows live (same as before); stat job has no window for auth dialogs
- Task 3: reviewed by controller inline: ok. minor: replace question untested; legacy phase-4 libraries with a name shared across kinds keep both (find returns first); prompt text now "Save as"/"Name" in all three dialogs
- Final review: merge, 0 Critical/Important, 8 minor (see final-review.md). Controller verified fresh build 38/38, pkgbuild OK, settings OK.

## Chrome
- Task 1: reviewed by controller inline: ok. minor: any other grouping with an empty label now also draws no header (guard is label-based)
- Task 2: reviewed by controller inline: ok. minor: a saved ribbon tab index (if persisted) shifts by one after Share's removal
- Task 3: reviewed by controller inline: ok. minor: excludeList is by action key, so another app's action literally keyed "rule000"/"a-select-by-rule" would also hide (unlikely)
- Task 4: reviewed by controller inline + ran the 4 panel tests (all PASS, none skipped). minor: `while (activePopupWidget()) hide()` has no iteration cap
- Task 4: AFTER-MERGE CHECK: typing in the Tags panel name field on Wayland; fallback popup if unreliable
- Task 6: reviewed by controller from report: ok. minor: menu's Copy Location/Open Terminal Here are separate actions showing the shortcut (real shortcuts on home_copy_path/file_terminal); uniqueness test checks top level only
- Task 8: minor (deferred): cursor stack/drag-threshold untested offscreen (manual); View-tab test hard-codes 28 px padding
- Task 8: AFTER-MERGE CHECK: 12-step manual list in task-8-report.md (move, resize edges+cursors, double-click max, snap, Meta+Left, buttons, Alt+F3, inactive colour, live colour change, File backstage, collapse)
- Final review: FIX FIRST — Critical: RibbonTabStrip has no sizeHint → 4x32 px in the title bar, tabs invisible (tests passed because mouseClick ignores geometry). Fix wave also: menu-only Copy Location/Open Terminal shortcuts ambiguous → text-only; window-button tooltips. Deferred minors: cursor stack with two overlapping windows; s_last null when newest closes; old Tabs config restores Home; app icon not a drag area; double reparseConfiguration; shadow not re-attached on surface recreation; Your tags capped at 8. FIX_BASE 7c72af9

## Manual checks (user, after install)
1. Drag the empty title-bar area: the window moves.
2. Hover each edge and each corner: the right resize cursor shows; drag resizes. Moving back inside restores the normal cursor.
3. Double-click the empty bar area: maximises; again: restores. Max button icon switches window-maximize / window-restore each time (also after Meta+Up / KWin menu).
4. While maximised: no resize cursor at the screen edges, no outline.
5. Drag the window to a screen edge: it snaps (quick tile). Meta+Left / Meta+Right tile.
6. Min / Max / Close buttons work; Close hover is red (#c42b1c).
7. Alt+F3 opens KWin's window menu.
8. Focus another window: the bar (and outline) turn to the inactive colour; back: active.
9. System Settings > Colours: pick a scheme with another title-bar colour: the bar follows live.
10. File tab opens the backstage; the collapse button in the title bar collapses/expands the ribbon.
11. Drag files near (not on) the window edges and rubber-band select from inside to near the edge: both still work outside the 6 px band.
12. Window size/position is restored on the next start.
1. Filter: `if (watched->isWindowType()) return false;` before the widget check (no cursor churn from the QWidgetWindow delivery). No pure seam; unchanged tests pass.
2. Scrollbars: `edgesForPress(pos, overScrollBar)` uses `ScrollBarBorder = 2`; filter passes `qobject_cast<QScrollBar*>(widget)`. Test `scrollbarsGetANarrowBand`. RED: compile error (no 2-arg overload); GREEN: explorerwindow_test passed.

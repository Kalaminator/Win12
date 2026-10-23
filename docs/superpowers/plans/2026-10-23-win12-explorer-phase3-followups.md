# Win12 Explorer phase 3: deferred follow-ups

Minor findings from the phase 3 task reviews and the final review (merged at 5f0951a). None loses data.

- Rule matcher: `compile()` ignores the test for Size/Date/Kind rules (a mismatched test is accepted; the dialog never sends one).
- Rule matcher: `parseSize` overflows on huge values ("99999999 TB", signed shift).
- Rule matcher tests missing: Created, EndsWith, name Equals, folder extension, "2MB", folder vs smaller-than.
- RuleSearch: `closingStopsTheJob` stops before the event loop runs (weak); no test for "."/".." filter or hidden files; `rulesearch.h` dereferences `compile({})` (a default `RuleMatcher` ctor is sturdier).
- Select by rule: gating compares with the current look-in, not the look-in used for the search (drifts if the field is edited after Find).
- Select by rule: Enter with focus in the result list fires the default action (Trash).
- Select by rule: Select after changing the look-in selects nothing (the window selects in the active view).
- Select by rule: result Size column sorts as text; store bytes in a sort role.
- Select by rule tests missing: reject() stops the search, keepStructure only when recursive, Kind rows.
- Actions: delete-forever test does not prove ForceConfirmation (assert `activeModalWidget()` non-null).
- Actions: right-click Select by rule opens on the active view's folder, not the right-clicked pane (split view).
- Actions: keep-structure `mkpath` not recorded for Undo (empty folders stay after Undo).
- Actions: `exploreractions.h` includes the dialog headers (only enums needed); dead `win12->isEmpty()` branch.
- Rename planner: trailing-dot name "a." gets extension "."; tests missing for Date default format, invalid date.
- Rename many: preview clash check uses the filtered view model (hidden or quick-filtered names missed; `rename()` still refuses, so an error box mid-batch). No test for the view-model path.
- Rename many: grey Unchanged rows show no reason (slash in name, empty name).
- Rename many: context-menu entry uses selection order, not view order.
- Rename many: each file rename also lands in FileUndoManager (stale Ctrl+Z after Undo last rename untested).
- Rename many dialog tests missing: clash row text, grey rows, add/remove block, drag reorder refresh.

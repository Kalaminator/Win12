# Win12 Explorer phase 4: deferred follow-ups

Minor findings from the phase 4 task reviews and the final review (merged at 9ba92d2). None loses data.

- Task 1: no recursion depth limit in Parser (huge "(((" paste overflows stack → carried to Task 7: depth limit 256); FILES positional bool == named form, Files(Subfolders:=True) passes arity (Task 7 checks args[0]); toText of 1e300/NaN/inf; FxFunctions help translated once at first call; fxparser_test script() ignores lexer error
- Task 2: int(x) cast on huge/NaN numbers UB (fxeval.cpp:638,643,651 — clamp); TEXT("12","0.0") does not convert numeric text; DATE overflow → #NUM! (Excel rolls over); Empty vs Date → #VALUE! (Excel: blank=0); no Date-vs-Date compare test; FxContext::countCache added (fresh context per run)
- Task 3: startOn reads caller's KFileItem copies on the worker (callers must not use them until finished); sort not cancellable; batchesNotPerFile may flake on a very slow machine (250 ms flush); folder SIZE 0 = Call 7 (ok)
- Task 4: std::sort unstable for case-only name differences; duplicate name+kind on disk not deduped; failed-write rollback and remove-missing-name untested
- Task 5: rejectStopsFormulaRun test weak (search yields nothing anyway); Save as with no library / empty name gives no feedback
- Task 6: no valid RED for first commit (stale binaries); Save as… inert until Task 12 wires a RuleLibrary; stale hook finds inner edit by objectName "formulaEdit" (test guards it); first clash row says "planned below"
- Task 7: `f.MoveTo -1` / `f.MoveTo (d) & "/x"` give generic "Expected a statement…"; depth counter not decremented on failure paths (harmless); member assignment and single-line If rejected (Call 10)
- Task 8: trash check keyed by path; symlink-to-folder still yielded by Files(); bare Untag word matches any colour; Lister has 4th arg (cancel)
- Task 9: empty mkpath folders left after undo; runner undo of Rename is recorded in global Ctrl+Z (redo-like); folder-merge undo risk only if plans ever include folders; conflict Cancel path untested (needs UI); Task 11 must read undoNote() before undo()
- Task 10: progressIsThrottled cannot fail (5 hook calls only; needs a long script or every=1 seam); no separate RED run
- Task 11: rejectStopsRun weak (no mid-flight cancel check); QInputDialog/message box dismissed by 100/300 ms timers (may flake); dialog dies with parent window mid-run (Task 12 wiring)
- Task 12: openScript with no folder silent (action disabled without view); ribbon My rules menu's Rename-empty message path untested (shares function); setExecSelectDialog seam added
- Task 13: runSavedRule matches by name only (Select+Script with same name → Select runs); existing library gets no service menu until next save; unknown-rule error on an empty window; include order; runSavedRule untested (GUI)
- Final: service menu %F on remote folders goes through kioexec; fx Name column uses toLocalFile() on smb:/; a pathological REGEX pins a pool thread (Stop is per row); Save silently replaces a same-named rule.
- Not built (Call 17): Run last plan button, script syntax highlighting, Do While loops, localized formula names, a Folders() function for scripts.

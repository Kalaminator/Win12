# Win12 Explorer phase 2 (Tags): deferred follow-ups

Minor findings from the phase 2 task reviews (merged at a6e1995). None loses data.

- Task 1: decode does not trim the word ("Red/ x" → " x"); paintColour looks up Overcast by name each call; no decodeAll test with a slash word.
- Task 2: load does not dedupe hand-edited duplicates; touch accepts colour >= Count; rename/recolour with an unchanged value rewrites the file; rename/recolour persistence not reload-tested; merge keeps the older `used` timestamp.
- Task 3: remove() control flow awkward (NotLocal check inside a branch); replace foreign→Win12 skips the MaxTags check; no test for Unsupported/errorText; merge-on-replace and foreign-skip-limit untested.
- Task 4: include order in sortgroup.h; colour-only tag under TagName untested; groupInfo `.first()` relies on non-empty tagGroups; viewsettings GroupRole int not range-checked.
- Task 5: ensure() trusts exists (a 0-byte SVG is never repaired); the 3-flag emblem clips the top row by 1 px; the plugin's KSharedConfig may need reparseConfiguration() for a live toggle.
- Task 5: AFTER-MERGE CHECK: Dolphin shows the flag at the icon's bottom-right; if hicolor scalable/emblems is ignored → PNG fallback (16/22/32).
- Task 6: cache-clear lambdas connected after setSourceModel (stale read risk); extraColumnData caches an invalid url; include order in foldermodel_test/foldermodel.h.
- Task 7: List-mode sizeHint reads every row's tags; invalidateTags emits per-row dataChanged with no roles; Medium/XL flags untested.
- Task 8: identity-mode layoutChanged persistent-index remap untested; taggroupproxy_test ignores f.open failure.
- Task 9: the checkable action toggles even when addTag fails; no tests for tags_remove menu wiring, tags_flags persistence, tags_group_* sync; menu ribbon icons may be scaled down by QMenu (visual check).
- Task 10: q.exec() runs the whole Baloo query before the cancel check; `"` in a tag word is not escaped in the Baloo string; QVERIFY inside a lambda in tagsearch_test; Baloo path untested end to end (manual check).
- Task 11: showResults twice with the same search double-connects `found` (Qt::UniqueConnection); F5 on results re-lists the folder instead of rerunning the search; result icons lack overlay emblems; a renamed result row is not refreshed.
- Task 12: clickPicksOne asserts only spy.count() >= 1; no test for pick + mode change driving results; tagribbon.h include order.
- Task 13: m_failed keeps only the last error text; the Baloo-failure path is untested.
- Task 14: the plasmoid switch can show a stale value after the Explorer button toggled showFlags.
- Task 15: selectByRuleAddTagOpensNewTagDialog only checks the action exists; no default button for AddTag in the dialog.

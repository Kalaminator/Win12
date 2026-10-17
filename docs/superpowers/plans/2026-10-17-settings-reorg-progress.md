# Settings reorganisation — progress log

Plan: `docs/superpowers/plans/2026-10-17-settings-reorg.md` (entry format and driver rules in its "Driver protocol"
section). Each run appends ONE entry below and exits; the driver starts the next run with the model named by the
last `next-model:` line.

## 2026-10-17 05:10 Plan written
status: done
done: plan + this log (one commit, docs only; no code touched)
left: nothing
notes: runs only after the multi-monitor plan (`docs/superpowers/plans/2026-10-17-multimonitor-islands.md`) ends with `next: none`; Task 1 re-reads the Taskbar page and `main.xml` for the two island keys and records the base commit. Spec = `docs/research/settings-reorg-proposal.md` sections 3-4.
next: Task 1 — Re-read the Taskbar page after multi-monitor; key guard + baseline; dialog screenshot procedure
next-model: sonnet

## 2026-10-17 08:20 Task 1: Re-read the Taskbar page after multi-monitor; key guard + baseline; dialog screenshot procedure
status: done
done: `tests/test_settings_keys.js` + `tests/settings-keys.txt` (141 keys); read-only markers on ConfigMenu/ConfigColours/ConfigTaskbar (commit 4e236ad)
left: nothing
notes: BASE commit for the whole change set = 7099f97 (multi-monitor done). Guard prints the 5 known duplicates (taskbarLayout, taskbarMagnetic, colorMode, accentMode, accentColor) + `settings keys OK (141 keys, duplicates allowed)`; no sixth duplicate. Island keys (ConfigTaskbar.qml, plain `property` + onActivated, no alias): `islandScreen` (String; ComboBox id screenBox, label "Show islands on:", visible `cfg_taskbarLayout !== "single"`) and `islandAllApps` (Bool; ComboBox id appsBox, label "Open apps on other monitors:", visible `screenBox.visible && cfg_islandScreen === "all"`), plus a disabled hint Label (visible screenBox.visible) after them. `win12Mirror` is in main.xml but has no control (engine key). Screenshot procedure works: dialog box 552,212–1368,866; sidebar centre SX=607; row k at SY=285+68*k (rows 0..8: Style 285, Layout 353, Shortcuts 421, Look 489, Colours 557, Search 625, Taskbar 693, Icons 761, Start Button 829); Cancel 1315,839; OK 1140,839. Click = `python3 tests/tools/w16mouse.py drag X,Y X,Y 1`. Verified: row 6 shows Taskbar; Cancel closes the dialog (closed.png shows no dialog). Use scratch dir /tmp/claude-1000/scratch-settings (the plan's scratchpad path was not needed). config.qml order now: 0 Style, 1 Layout(ConfigMenu), 2 Shortcuts(ConfigRail), 3 Look, 4 Colours, 5 Search, 6 Taskbar, 7 Icons, 8 Start Button(ConfigButton).
next: Task 2 — StyleKinds.qml + offscreen unit test; the three VERIFY sets; Style page uses the helper
next-model: sonnet

## 2026-10-17 08:35 Task 2: StyleKinds.qml + offscreen unit test; the three VERIFY sets; Style page uses the helper
status: done
done: `StyleKinds.qml`, `tests/test_stylekinds.qml` (stylekinds OK), ConfigStyle uses `kinds.*` (commit 6fa480b)
left: nothing
notes: test root is `Item` (QtObject printed nothing) and needs `QT_FORCE_STDERR_LOGGING=1` or `qml` prints no console.log: `QT_FORCE_STDERR_LOGGING=1 QT_QPA_PLATFORM=offscreen /usr/lib/qt6/bin/qml tests/test_stylekinds.qml 2>&1 | grep stylekinds` (use that in Standard checks). Key guard unchanged: 5 duplicates + OK (141 keys). Style screenshot OK; journal shows only the old "Created graphical object was not placed in the graphics scene" warning for ConfigStyle:13. VERIFY sets (grep): All Programs group (autoExpand, desktopInTree, allAppsAsMenu) = `kinds.win7Family || kinds.menuKind === "xp"` (treeExtras true only in styles/windows7.qml and xp.qml); recentHideFolders = `kinds.menuKind === "windows11" || kinds.menuKind === "windows12"` (Recommended in both styles); smallPlaces = `kinds.win7Family || kinds.menuKind === "xp" || kinds.menuKind === "pro"`; powerButtonIcon = `kinds.win7Family`. (win10/AppList.qml is shared by windows7/11/app styles but only reads the tree keys when treeExtras.)
next: Task 3 — Start menu page (new ConfigStartMenu.qml) from Style, Layout and Look
next-model: sonnet

## 2026-10-17 08:45 Task 3: Start menu page (new ConfigStartMenu.qml) from Style, Layout and Look
status: done
done: `ConfigStartMenu.qml` (6 read-only + 23 keys + vertical helper, tile image dialog, reset-positions dialog); moved blocks removed from ConfigStyle/ConfigMenu/ConfigLook; config.qml has "Start menu" (view-grid) at row 1, Layout icon now view-list-details (commit d0cf0a8)
left: nothing
notes: key guard unchanged (5 duplicates, OK 141 keys); qmllint, tiles, stylekinds pass. Screenshot row 1 OK (Size and position + Tiles groups; Windows 12 group hidden for the user's menu). Sidebar rows now: 0 Style 285, 1 Start menu 353, 2 Layout 421, 3 Shortcuts 489, 4 Look 557, 5 Colours 625, 6 Search 693, 7 Taskbar 761, 8 Icons 829, 9 Start Button (scrolls; check). ConfigMenu.qml dropped cfg_menuStyle (unused); Task 4 renames it and may add `kinds` for the recentHideFolders/All Programs rules.
next: Task 4 — Apps list page (git mv ConfigMenu.qml ConfigAppsList.qml), groups, hide-list rule, All Programs visibility
next-model: sonnet

## 2026-10-17 08:58 Task 4: Apps list page (git mv ConfigMenu.qml ConfigAppsList.qml), groups, hide-list rule, All Programs visibility
status: done
done: ConfigMenu.qml -> ConfigAppsList.qml (git mv, rename in ce02205, content in d0c17f9); six read-only keys + StyleKinds; labels renamed; hideRecentFolders only for windows11/windows12; "All Programs (Windows 7 and XP)" group only for win7Family/xp; config.qml row "Apps list" (commit d0c17f9)
left: nothing
notes: key guard unchanged (5 duplicates, OK 141 keys); tiles, stylekinds, qmllint pass; grep for old tags = 0. Screenshot row 2 (SY=421) OK for the user's style: Apps list, Hidden apps, Filtered folders; All Programs hidden. Every row except the Hide box has enabled: !hideList.checked (foldersBottom/allAppsAsMenu also need folders.checked).
next: Task 5 — Start button & shortcuts (ConfigRail merged into ConfigButton; XP Start colour from Style)
next-model: sonnet

## 2026-10-17 09:15 Task 5: Start button & shortcuts (ConfigRail merged into ConfigButton; XP Start colour from Style)
status: done
done: ConfigRail.qml deleted (git rm); ConfigButton has 4 groups (Start button, Right-click menu, Menu shortcuts, Power) + 6 read-only keys + StyleKinds; xpStart* rows/decls moved from ConfigStyle; config.qml: "Shortcuts" category gone, "Start button & shortcuts" right after "Apps list" (commit 48e989e)
left: nothing
notes: key guard unchanged (5 duplicates, OK 141 keys); tiles, stylekinds, qmllint pass. Screenshot row 3 (SY=489) OK: Start button + Right-click menu groups seen (Menu shortcuts/Power are below the fold, not scrolled). XP colour row hidden for the user's menu. Sidebar rows now: 0 Style, 1 Start menu, 2 Apps list, 3 Start button & shortcuts, 4 Look, 5 Colours, 6 Search, 7 Taskbar, 8 Icons. Only grep hit for old names is a code comment ("Start11's list").
next: Task 6 — Windows page (new ConfigWindows.qml) from the Taskbar page
next-model: sonnet

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

## 2026-10-17 09:30 Task 6: Windows page (new ConfigWindows.qml) from the Taskbar page
status: done
done: `ConfigWindows.qml` (6 read-only keys + 9 moved keys, picture dialog, XP Firefox row); block, dialog and 10 declarations removed from ConfigTaskbar; config.qml "Windows" (preferences-system-windows) after Taskbar (commit e04e32f)
left: nothing
notes: key guard unchanged (5 duplicates, OK 141 keys); tiles, stylekinds, qmllint pass. Screenshot row 8 (SY=829) OK: one group "Title bars and frames"; hint label hidden because the user's windowTheme is on; picture rows disabled until a picture is set. Sidebar rows now: 0 Style, 1 Start menu, 2 Apps list, 3 Start button & shortcuts, 4 Look, 5 Colours, 6 Search, 7 Taskbar, 8 Windows, 9 Icons (scrolls).
next: Task 7 — Appearance (git mv ConfigLook.qml ConfigAppearance.qml, Colours merged, 12 sliders from Taskbar)
next-model: opus

## 2026-10-17 09:50 Task 7: Appearance (git mv ConfigLook.qml ConfigAppearance.qml, Colours merged, 12 sliders from Taskbar)
status: done
done: ConfigLook.qml -> ConfigAppearance.qml (6 read-only keys + StyleKinds, 31 keys, groups Colours / Windows 12 shade / Transparency and corners / Texture / Fonts / Effects); ConfigColours.qml deleted; 5 win12* declarations, their 5 slider rows and the hint removed from ConfigTaskbar; config.qml "Look" -> "Appearance", "Colours" gone (commit 780832e)
left: nothing
notes: key guard: `2 duplicates: taskbarLayout (ConfigStyle.qml, ConfigTaskbar.qml); taskbarMagnetic (ConfigStyle.qml, ConfigTaskbar.qml)` + OK 141 keys, no LOST; tiles, stylekinds, qmllint pass. ConfigLook's model order confirmed (0 Automatic, 1 Dark, 2 Light, 3 Custom) so modes = [2, 1, 0, 3]; user's colorMode=0 shows "Automatic (Plasma colours)". Dropped `pragma ComponentBehavior: Bound` (the Swatch inline component reads `page`, as in ConfigColours). XP: the "Colours" section title stays, the one hint line replaces its rows. None of the 5 sliders was tied to w12GapMode. Taskbar's Windows 12 GridLayout now holds only Bottom spacing / Windows / Menu gap. Git records the rename as D+A (big content change). Screenshot row 4 (SY=557) OK, all 6 groups seen (user's menuStyle windows12, so shade + 3 Windows 12 rows show); only journal line is the old "not placed in the graphics scene" warning. Scroll a page: `drag 1354,382 1354,552 1`, then +170 per step. Sidebar rows now: 0 Style, 1 Start menu, 2 Apps list, 3 Start button & shortcuts, 4 Appearance, 5 Search, 6 Taskbar (693), 7 Windows (761), 8 Icons (829).
next: Task 8 — Taskbar page: groups, one TaskbarLayoutBox, island settings placed, merged colour rows, shared rules
next-model: opus

## 2026-10-17 10:05 Task 8: Taskbar page — groups, one TaskbarLayoutBox, island settings placed, merged colour rows, shared rules
status: done
done: ConfigTaskbar rewritten as hint + 4 groups (Windows 12 taskbar / Taskbar colour and texture / Behaviour / Plasma panel), six read-only keys + StyleKinds (with taskbarTheme), Bar colour row + its hint gone; ConfigStyle lost TaskbarLayoutBox and cfg_taskbarLayout/cfg_taskbarMagnetic; TaskbarLayoutBox header says Taskbar page only (commit a92c82c)
left: nothing
notes: key guard `--strict` now prints `settings keys OK (141 keys)` (no duplicates); tiles, stylekinds, qmllint pass. Page props: `w12` (= kinds.taskbarKind === "windows12", every row of the 12 group), `w12GapMode` keeps the full main.qml test (float && Keep a gap && single; the plan's shorter line would have changed it), `customColour` = customBox.checked && !cfg_taskbarFollowsStyle. "Taskbar colour and texture" has no visible rule: the proposal's `taskbarGenerated || !taskbarFollowsStyle` is always true (ponytail comment). Gradient/Texture/Paint-active use kinds.taskbarGenerated (same as the old followsStyle || customBox). "Show islands on" changed visible -> enabled (layout != single); "Open apps…" enabled when screenBox enabled and value "all"; island hint always shown with the group. Auto-hide hint follows tbHiding.visible; the old KDE Plasma-look hint string is dropped. Review note for Task 13: with a Windows taskbar (followsStyle on) the hint "Available with a custom taskbar colour." shows but the Override check is hidden (spec as written). Screenshots row 6 (SY=693), three scroll positions (bottom: `drag 1354,382 1354,860 1`): all four groups OK, no red warning (KDE patched); journal only the old "not placed in the graphics scene" lines. Style page: the old `Item { isSection }` after the Windows combo hint is still there; Task 10 regroups Style.
next: Task 9 — Advanced page (git mv ConfigIcons.qml ConfigAdvanced.qml); XP icons into Style; reset/backup/restore out of Style
next-model: sonnet

## 2026-10-17 08:24 Task 9: Advanced page (git mv ConfigIcons.qml ConfigAdvanced.qml); XP icons into Style; reset/backup/restore out of Style
status: done
done: ConfigAdvanced.qml (groups "Icons and wallpaper (applied at once, not on Apply)" and "Backup and reset", cfg_resetRequest + 2 FileDialogs + PromptDialog moved in); ConfigStyle has xpIcons/xpIconTheme rows (visible kinds.anyXp, own icon-theme scan DataSource) and no resetRequest; config.qml "Advanced" (icon configure) last (commits 5f9eebf rename,  content)
e0ebbe8left: nothing
notes: key guard --strict OK (141 keys); tiles, stylekinds, qmllint (no errors) pass. Bug fixed on the way: NewStuff.Button has no onChangedEntriesChanged, so the old Icons page failed to load ("Cannot assign to non-existent property"); now onEntryEvent. Screenshot row 8 (SY=829) OK: both groups seen. XP rows not screenshotted (user's style is not XP). Style still has its old Item isSection after the Windows combo; Task 10 regroups Style. Sidebar rows: 0 Style, 1 Start menu, 2 Apps list, 3 Start button & shortcuts, 4 Appearance, 5 Search, 6 Taskbar, 7 Windows, 8 Advanced.
next: Task 10 — Style page: heading, Mix and match / Windows 7 / Windows XP groups, renamed labels
next-model: sonnet

## 2026-10-17 10:40 Task 10: Style page — heading, Mix and match / Windows 7 / Windows XP groups, renamed labels
status: done
done: ConfigStyle: heading "Style", separators Mix and match / Windows 7 (visible kinds.win7Family) / Windows XP (visible kinds.anyXp); labels Window title bars:, Menu look:, Windows 7 list:, Modern list:, XP theme: (commit 0ecdf87)
left: nothing
notes: key guard --strict OK (141 keys); tiles, stylekinds, qmllint pass; no Substyle/question label left (only cfg_ key names). Screenshot row 0 OK (heading + cards); the groups are below the fold, not scrolled. The page has no xpTaskbarTheme/windowTheme/taskbarFollowsStyle rows (not in ConfigStyle), so the spec's Mix list is shorter here; Task 13 can check where they live. Journal clean.
next: Task 11 — Search page groups; final config.qml; strict guard in install.sh; HANDOFF note; all 9 screenshots
next-model: sonnet

## 2026-10-17 11:00 Task 11: Search page groups; final config.qml; strict guard in install.sh; HANDOFF note; all 9 screenshots
status: done
done: ConfigSearch groups Results / History / What to search (+ hint, "Show the "Search the internet" button"); config.qml final nine-page order, Taskbar icon preferences-desktop-plasma; install.sh runs test_settings_keys.js --strict; HANDOFF paragraph (commit 5e9c6e8)
left: nothing
notes: install.sh prints test_islands OK + settings keys OK (141 keys); qmllint clean; no old Config* name left. All 9 pages screenshotted (rows SY=285+68k, k=0..8: Style, Start menu, Apps list, Start button & shortcuts, Taskbar, Windows, Appearance, Search, Advanced): groups and labels look right, no stray old label; below-the-fold groups not scrolled. Journal: no new error after the run.
next: Task 12 — Translations: Messages.sh, es.po complete, .mo
next-model: sonnet

## 2026-10-17 12:00 Task 12: Translations: Messages.sh, es.po complete, .mo
status: done
done: Messages.sh run; 20 untranslated + 75 fuzzy msgids in po/es.po translated (fuzzy flags gone); template.pot and es .mo rebuilt (commit 411b142)
left: nothing
notes: msgfmt --check: 580 translated messages, no untranslated, no fuzzy. install.sh prints settings keys OK (141 keys). Other po/*.po were not touched by Messages.sh. Method: msgattrib --translated --no-fuzzy, msgcat --use-first with a new translation file, msgmerge --no-fuzzy-matching against template.pot (no polib here). Stray untracked messages.mo in the repo root is not ours; left alone.
next: Task 13 — Review against the proposal, fixes, ELI5 + hand-check list for the user
next-model: opus

## 2026-10-17 08:30 Task 13: Review against the proposal, fixes, ELI5 + hand-check list for the user
status: done
done: review of all 9 pages against proposal sections 3-4; fixes (commit 5fbb074): Taskbar — with a Windows taskbar (taskbarFollowsStyle on) the Colour..Task buttons rows and the "Available with a custom taskbar colour." hint now hide with the Override check (before: greyed rows + a hint pointing at a hidden control); Search — Results rows in the proposal order (group, classic, filter icons, web); Apps list — one stray indent
left: nothing required. Optional, not done (spec gaps, not deviations): (1) Appearance texture rows (Placement/Fade/Darken/Recolour) and Taskbar Placement/Texture opacity/Tint are disabled until a texture is picked with no hint line (spec lists none; the picker sits right above); a hint needs a new msgid + Spanish. (2) "All Programs (Windows 7 and XP)" group title keeps a style tag because the proposal's own heading has it; drop to "All Programs" only if the user wants (msgid change).
notes: Step 1: strict guard `settings keys OK (141 keys)`; stylekinds OK; qmllint no errors; TaskbarLayoutBox only in ConfigTaskbar.qml:1; no question labels (only dialog titles); es.po 580 translated. Gating grep hits only ConfigStyle.qml:86-87 (the style cards' own "chosen" state, not gating: fine). Appearance hint keeps the old msgid with \"Plasma's own\" quotes (proposal says "kept"). colorMode modes [2,1,0,3] both ways and Menu/Text colour only for Custom: confirmed. Style combos Taskbar:/Window title bars: write taskbarFollowsStyle/xpTaskbarTheme/windowTheme (so the Mix rows are there). Screenshots after install: Taskbar (colour group goes straight to Gradient for the user's Windows taskbar) and Search (new order) OK; Cancel closed the dialog; journal clean.
(a) ELI5 for the user:
- Win12 Settings now has 9 pages: Style, Start menu, Apps list, Start button & shortcuts, Taskbar, Windows, Appearance, Search, Advanced.
- Each setting lives on ONE page only. Before, some were on two pages and fought each other.
- Style = pick your Windows look. Start menu = size, place, tiles. Apps list = what the list of programs shows.
- Start button & shortcuts = the button, its right-click menu, the side shortcuts, power.
- Taskbar = bar layout, islands, colour, behaviour. Windows = title bars and frames.
- Appearance = colours, see-through, corners, textures, fonts (old Look + Colours together).
- Advanced = icon theme, wallpaper, backup, reset.
- Rows that do not fit your style are hidden. Rows that wait for another switch are grey, with a short line that says why.
- Your saved settings did not change. Only where the switches sit changed. Spanish is complete.
(b) Hand-check list (open Win12 Settings, change, OK, reopen: it stuck; then put it back):
- Style: change "Menu look:", OK, reopen: it stuck.
- Start menu: tick "Full screen menu", OK, reopen: it stuck.
- Apps list: tick "Show recently added apps", OK, reopen: it stuck. Tick "Hide the apps list (tiles only)": the rows below go grey, not gone.
- Start button & shortcuts: switch Start button "Icon"/"Picture", OK, reopen: it stuck.
- Taskbar: tick "Floating bar", OK, reopen: it stuck.
- Windows: move "Opacity:", OK, reopen: it stuck (needs a Windows title bar picked on Style).
- Appearance: tick "Solid Start menu", OK, reopen: it stuck. Colour mode "Custom" shows Menu colour and Text colour; other modes hide them.
- Search: tick "Classic search…", OK, reopen: it stuck.
- Advanced: pick an icon theme: it changes at once (no OK needed); pick the old one back.
- Mix: with Style card Windows 12 and "Menu look:" Windows 7, the Windows 7 group shows on Style and the All Programs group shows on Apps list.
(c) docs/FEATURES.md still names old pages (follow-up, not done): l.18 "Start Menu page" (now Style + Start menu), l.29 "Layout page" (-> Apps list / Start menu), l.42 "Shortcuts page" (-> Start button & shortcuts), l.50 "Look page" and l.61 "Colours page" (-> Appearance), l.63 "from the Look page" (-> Appearance), l.89 "Start Button page" and l.99 (-> Start button & shortcuts), l.103 "Open it from the Layout page" (the Edit menu layout button is now on Start menu); Icons page section, if any, -> Advanced.
next: none
next-model: none

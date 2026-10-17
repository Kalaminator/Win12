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

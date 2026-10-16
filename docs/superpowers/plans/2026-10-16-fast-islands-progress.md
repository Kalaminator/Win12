# Fast islands — progress log

Plan: `docs/superpowers/plans/2026-10-16-fast-islands.md` (entry format and driver rules in its "Driver protocol"
section). Each run appends ONE entry below and exits; the driver starts the next run with the model named by the
last `next-model:` line.

## 2026-10-17 00:00 Plan written
status: done
done: plan + this log committed (no implementation yet)
left: Tasks 1-15
notes: sudo pacman (only) works without a password; check with `sudo -n /usr/bin/pacman -V`
next: Task 1 — `kde/` tooling (build.sh, pkgbuild-win12.sh, pacman hook, fixture test)
next-model: sonnet

## 2026-10-17 00:25 Task 1: kde/ tooling
status: done
done: kde/build.sh, kde/pkgbuild-win12.sh, kde/win12-kde.hook, tests/test_pkgbuild.sh, empty kde/patches/<pkg>/.keep dirs (4d98253); `sh tests/test_pkgbuild.sh` prints test_pkgbuild OK, bash -n build.sh clean
left: nothing
notes: files written exactly as in the plan; the test's second-run refusal message is expected output. Patch dirs hold only .keep (pkgbuild-win12.sh globs *.patch, so Tasks 2-4 must add real patches before a build).
next: Task 2 — libplasma patch: moving an applet keeps one config level
next-model: opus

## 2026-10-17 00:30 Task 2: libplasma patch — moving an applet keeps one config level
status: done
done: kde/patches/libplasma/0001-win12-containment-move-keeps-config.patch (1110e31): containment.cpp move branch + coronatest moveAppletKeepsConfig. Test failed before the fix (plugin= empty), passes after (8/8, QT_QPA_PLATFORM=offscreen). Patch passes `patch --dry-run -Np1` on a clean v6.8.0 tree.
left: nothing
notes: one change from the plan: the loader prefixes are built from the real \x1d group paths (walk KConfigGroup::parent(), like KConfigLoader), not "Containments<id>Applets", so moves into/out of an embedded containment (tray) also work. addApplet takes std::unique_ptr in 6.8 (test wraps it). Build deps installed via sudo -n pacman: plasma-wayland-protocols, wayland-protocols, vulkan-headers. CMake lib target is `Plasma` (no KF6Plasma target; build `coronatest`, it rebuilds the lib). Dev tree: ~/.cache/win12-kde/src/libplasma (build/ inside, patch applied, uncommitted). The QWARN "Cannot connect Containment::(nullptr)" comes from the upstream line `connect(currentContainment, nullptr, applet, nullptr)` (left as is). Task 10: check() may need QT_QPA_PLATFORM=offscreen.
next: Task 3 — plasma-workspace patch: scripting index fixes + `TasksModel.appIdFilter`
next-model: opus

## 2026-10-17 00:50 Task 3: plasma-workspace patch — scripting index fixes + `TasksModel.appIdFilter`
status: done
done: kde/patches/plasma-workspace/0001-win12-scripting-index-and-appidfilter.patch (0061c14): widget.cpp (SkipEmptyParts in index get/set, clamp to length()), tasksmodel.{h,cpp} appIdFilter + appIdFilterExclude (filterAcceptsRow, after appName), win12appidfiltertest.cpp + CMake entry. Test failed to compile before (no setAppIdFilter), 6/6 pass after (ctest -R win12). plasmashell target compiles. Patch passes `patch --dry-run -Np1` on a clean v6.8.0 tree.
left: nothing
notes: change from the plan: test setup must call m.componentComplete() BEFORE setLauncherList (not used by QML, else a 0 ms timer sets the source model and rowCount() stays 0); setup() returns bool, tests QVERIFY it. The test sets ENVIRONMENT QT_QPA_PLATFORM=offscreen through set_tests_properties, so check() needs no env. BUILD_TESTING=ON in plasma-workspace needs Qt6 WaylandCompositor = package qt6-wayland (was not installed; installed now with `sudo -n pacman -S --asdeps qt6-wayland`). Arch's PKGBUILD may not list it: Task 11 should add `makedepends+=(qt6-wayland)` for plasma-workspace in kde/pkgbuild-win12.sh, or the build fails in configure on another machine. Configure also reports KF6DocTools missing (optional, harmless). Dev tree: ~/.cache/win12-kde/src/plasma-workspace (build/ inside, Ninja, Debug; patch applied, uncommitted). The `git stash` in Step 7 fails with `git add -N` files; check with `git archive HEAD | tar -x -C <tmp>` + `patch --dry-run -Np1` instead. For Task 4: panel `reorderApplets` -> `LayoutManager.reorderApplets` already calls save() directly; only main.qml:101 (onAppletAdded) uses Qt.callLater.
next: Task 4 — plasma-desktop patch: immediate panel save, island registry, task manager drag/drop
next-model: opus

## 2026-10-17 00:40 Task 4: plasma-desktop patch — immediate panel save, island registry, task manager drag/drop
status: done
done: kde/patches/plasma-desktop/0001-win12-panel-save-and-islands.patch (c58aa3f): panel main.qml saves at once; taskmanager win12islandregistry.{h,cpp} (QML singleton TaskManagerApplet.Win12Islands) + autotests/win12islandregistrytest (2/2 pass; configure failed before the class existed); main.xml win12Island; qml/main.qml register/refilter/Connections + shouldShrinkToZero; Task.qml drag mime text/x-win12-appid + text/x-win12-launcher; MouseHandler.qml no hover-activate + island drop. Plugin target builds, singleton is in the qmltyperegistrations; qmllint shows only stock-class warnings (unqualified, itemAt). Patch passes `patch --dry-run -Np1` on a clean v6.8.0 tree.
left: nothing
notes: changes from the plan: (1) TasksModel treats an empty include list as "no filter", so a pinned/tray island with no apps sets appIdFilter ["win12-no-app"] (else an empty tray island would show every app). (2) appIdOf(url) falls back to the URL itself when appDataFromUrl has no id (plain executable launchers), and the drag mime uses `AppId || String(LauncherUrlWithoutIcon)` and the RAW launcher URL (not tryDecodeApplicationsUrl), so registry ids, filter matches and launcher lists use one form. (3) Hover/drop island code also requires `tasks.win12Island` (a stock task manager acts exactly as before); onDropped computes its own rtlSpacerAdjustment and uses isGroupDialog ? itemAt : childAt like onPositionChanged. (4) Applet links PW::LibTaskManager (include is `<tasktools.h>`); test links KF6::Service too (tasktools.h includes KService). Build deps installed now with `sudo -n pacman -S --asdeps`: intltool kaccounts-integration kdoctools libibus packagekit-qt6 scim (all are in Arch's makedepends, so Task 12 needs nothing extra). Dev tree: ~/.cache/win12-kde/src/plasma-desktop (build/ Ninja Debug, patch applied, new files `git add -N`). Export command that works: `git diff --no-color -- . ':!build'` (do not `git add -N .`, it would add build/). For Task 6: write `win12Island` before `launchers` is safe either way (a launcher an instance holds is only removed when the registry gives its app to ANOTHER island, and the final config wins), but after a swap check the pinned island still lists every pinned launcher.
next: Task 5 — Win12 `islandlogic.js` + `tests/test_islands.js` (pure tests + fake-shell harness)
next-model: opus

## 2026-10-17 00:43 Task 5: Win12 `islandlogic.js` + `tests/test_islands.js` (pure tests + fake-shell harness)
status: done
done: package/contents/ui/code/islandlogic.js (roles, trayOrder, listsFor, mergeLists), tests/test_islands.js (pure asserts + fake plasmashell harness running today's islands.js), install.sh runs it after test_tiles.js (fc78805). Failed before the file existed (ENOENT), `node tests/test_islands.js` prints test_islands OK; a broken roles() makes it fail.
left: nothing
notes: one fake change from the plan: runIslands() first drops removed widgets from each panel's `gone` list, except system trays. Why: Widget.remove() = applet->destroy() -> deleteLater, so in real Plasma a removed widget is gone by the next evaluateScript call; with the plan's "always lingers" fake, single looped forever at fix() on the removed reveal helper (index -1 sorts first). Trays still linger (the W16 live quirk win12Gone filters). Pass counts with today's one-step-per-pass script: split-left 16, single 7, split-right 12, single 7 (Task 6 goal: 2). Typed `widgets(type)` in the fake skips lingering widgets (real Plasma would list a lingering tray there too; the script only types HELPER).
next: Task 6 — Win12 `islands.js` fast engine + `islands` mode; `syncCenterPanel` in one pass
next-model: opus

## 2026-10-17 00:47 Task 6: Win12 `islands.js` fast engine + `islands` mode; `syncCenterPanel` in one pass
status: done
done: islands.js rewritten as in the plan (one pass per switch, `islands` mode, roles/launchers swap, win12TrayClean), main.qml (@POS@ + islandlogic.js in front of islands.js in syncIslands, `islandPos` re-sync, syncCenterPanel sets all indexes in one pass, comment updated), main.xml label, tests/test_islands.js plan cases (4e1e417). Failed before (`islands Left converges fast`), `test_islands OK` + `tiles.js: all checks passed` after; qmllint main.qml errors 0 before and after; both generated scripts (islands, centre) parse in node.
left: nothing
notes: pass counts in the harness (were 16/7/12/7): split-left 3, single 2, islands Left 3, Center 2, Right 2, single 2, split-left -> islands 3, islands -> split-left 3. Three changes from the plan: (1) `passes()` returns Infinity, not -1, when a switch never ends (-1 passed every `<= n` assert; the two old `> 0` checks are now `<= 40`). (2) The single branch puts each removed helper's id in `gone` (a removed widget stays listed until the script ends; without it `order(p)` set index on the removed helper, which crashes the fake and is wrong in real Plasma). (3) Split islands leave the 0 px reveal helper out of `order(R)` (saves one pass). For Task 13 (live): islands.js does not mirror the host's own widget order at Right (Start at the far end); with centerTaskbar on, syncCenterPanel reverses the host and adds its spacer in a custom-length island: check the look at Right and Center, and that both scripts do not fight (they share the flock).
next: Task 7 — Win12 `kdePatched` gate + "3 Islands" entry and warning in settings
next-model: opus

## 2026-10-17 00:48 Task 7: Win12 `kdePatched` gate + "3 Islands" entry and warning in settings
status: done
done: main.qml `kdePatched` + `patchCheck` DataSource (`pacman -T libplasma-win12 plasma-workspace-win12 plasma-desktop-win12`), `onKdePatchedChanged` runs syncCenterPanel + syncIslands, both now start with `if (!configReady || !kdePatched) return;`; TaskbarLayoutBox.qml: 4th value `islands`, entry "3 Islands: pinned apps, open apps, tray", new help text, red warning label with the kde/build.sh hint (dd73e7d). qmllint: TaskbarLayoutBox 0 errors, main.qml 0 errors; `test_islands OK`, `tiles.js: all checks passed`; `grep -c kdePatched main.qml` = 5.
left: nothing
notes: one change from the plan: the warning label and the long help label use `Layout.maximumWidth: Kirigami.Units.gridUnit * 24` + `wrapMode: Text.Wrap` (the repo's pattern in ConfigTaskbar.qml:236; `Layout.fillWidth` does not wrap inside a FormLayout). New i18n strings for Task 8 (all in TaskbarLayoutBox.qml): "3 Islands: pinned apps, open apps, tray", the new help text ("Split layouts put the tray, clock and weather into separate floating panels (islands). 3 Islands also splits pinned and open apps; drag an app icon to another island to move it there. Touching the bottom edge shows all of them."; it replaces the old help string), and the warning ("The taskbar layouts and the Start position need Win12's patched KDE packages. Run kde/build.sh from the Win12 source folder, then log out and in."). `pacman -T` says "not patched" on this machine now (expected until Task 12). Do NOT run install.sh before Task 12 on the live desktop: the gate would stop all island / Start-position syncs (panels stay as they are, no harm, but the user would see the warning).
next: Task 8 — Translations
next-model: sonnet

## 2026-10-17 01:00 Task 8: Translations
status: done
done: po/template.pot refreshed, 3 new es.po entries translated (the changed help text lost its fuzzy mark), .mo rebuilt (f066605); `msgfmt --check` clean, "566 translated messages." with no fuzzy/untranslated.
left: nothing
notes: Messages.sh only moved line numbers elsewhere. The header `#, fuzzy` at es.po line 6 was already there (not counted by msgfmt).
next: Task 9 — Live-test tools in `tests/tools/` + docs
next-model: sonnet

## 2026-10-17 01:10 Task 9: Live-test tools in `tests/tools/` + docs
status: done
done: tests/tools/{w16mouse.py (+ drag mode), state.sh, setlayout.sh}, README section "Patched KDE for the taskbar layouts (Arch)", HANDOFF entry (d6f0b8b). python ast parse + `sh -n` pass; usage line mentions drag.
left: nothing
notes: drag mode was only syntax-checked, not run (needs /dev/uinput on the live desktop; Task 14). After a drag the script clears sys.argv so the old move loop does not run.
next: Task 10 — Build + install patched libplasma
next-model: opus

## 2026-10-17 00:55 Task 10: Build + install patched libplasma
status: done
done: `kde/build.sh libplasma` (detached, log ~/.cache/win12-kde/libplasma.log) printed WIN12_BUILD_OK libplasma in 272 s; patch applied in prepare(), check() ran coronatest (1/1 passed, `100% tests passed out of 1`), install via sudo -n pacman -U. `pacman -Q libplasma` = 6.8.0-1, `pacman -T libplasma-win12` missing=0, Provides libplasma-win12=6.8.0-1. No repo change, so no code commit (only this log).
left: nothing
notes: check() needed no QT_QPA_PLATFORM here (the run's env has WAYLAND_DISPLAY=wayland-0 / DISPLAY=:0); an env without a display may need it. build.sh installs every *.pkg.tar.zst in the clone, so libplasma-debug 6.8.0-1 got installed too (makepkg's debug split; harmless). Task 15/README: the undo `sudo pacman -S libplasma ...` leaves libplasma-debug from the patched build; mention `pacman -R libplasma-debug` or skip -debug in build.sh. The running plasmashell still uses the old library until the restart in Task 13. Packages: ~/.cache/win12-kde/libplasma/libplasma-6.8.0-1-x86_64.pkg.tar.zst (+ -debug).
next: Task 11 — Build + install patched plasma-workspace
next-model: opus

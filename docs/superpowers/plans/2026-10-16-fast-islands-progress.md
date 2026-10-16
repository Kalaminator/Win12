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

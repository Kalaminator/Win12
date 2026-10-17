# Readable glass — progress log

Plan: `docs/superpowers/plans/2026-10-17-readable-glass.md` (spec section 12). Entry format and rules: the plan's
"Driver protocol". Append one entry per run; the last entry's `next:` / `next-model:` lines drive the loop.

## 2026-10-17 14:50 Plan written
status: done
done: spec section 12 (12.1 layer A edge+shadow with the Plasma 6.8 shadow mechanics, 12.2 kwin patch, 12.3 widget side, 12.4 tests), this plan (8 tasks), this log. No product code.
left: nothing
notes: Key finding: KWin 6.8's blur already applies a colour matrix in its on-screen pass (`blur.cpp` l.60 `colorTransformMatrix`, `onscreen.frag` `* colorMatrix`), so layer B is a kcfg + C++ change with NO shader edit. Plasma 6 floating panels get their shadow from the theme's `shadow-*` tiles through KWindowShadow with negative padding (Panel.qml `*ShadowMargin`), which gap mode (docked + ring) cannot use: it paints the shadow into the ring. Arch's kwin PKGBUILD fits `pkgbuild-win12.sh` unchanged; no makedepend missing on this PC. Tasks 1-4 are layer A (visible on every KDE), 5-6 layer B (build only; install is the user's Task 8). This PC: floating split-right islands, accent glass, Wayland.
next: Task 1 — `xptheme.py`: `edge=1` (two-tone edge, shadow tiles, gap-ring shadow) + generation checks
next-model: opus

## 2026-10-17 15:53 User approved
status: done
done: the user approved the plan and asked for the install to be done for them.
left: Tasks 1-8
notes: The user agreed to the plasmashell restarts of Task 4 (at most 2; this chat runs in its own service and survives). sudo is still not available to agents: Task 6 builds only; Task 8 (install, log out/in) stays the user's.
next: Task 1 — `xptheme.py`: `edge=1` (two-tone edge, shadow tiles, gap-ring shadow) + generation checks
next-model: opus

## 2026-10-17 15:58 Task 1: `xptheme.py`: `edge=1` — two-tone edge, shadow tiles, gap-ring shadow
status: done
done: aeea035 — `frame(..., edge=(K,.35,W,.25))` = border + 1 px inner ring on 4 sides and 4 corners (replaces `hi` when on); `shadow_tiles(x0, y0, S=12, op=.3)` (8 `shadow-*` groups in a 3x3 grid, sides linear, corners radial, 4 `shadow-hint-*-margin` 12x12) in all 3 variants below the `floating` frame (svg height `2*fh+180` when on); `ring_frame(..., edge)`: 6 shadow rings + 2 edge strokes; `taskbar(..., edge="0")`. New `tests/test_xptheme_edge.py` (shadow ids 12 / ring strokes 8 / mask strokes 0 / none without edge; also radius=2).
left: nothing
notes: Values: edge outer black .35, inner white .25; tiles 12 px black .3 -> 0 (two stops); gap shadow k=min(6,n-1). Deviation from the plan: gap-mode shadow is drawn as 1 px stroked RINGS outside the island (opacity 1-.95^(k+1-i), i.e. the same look as k nested .05 rects) instead of filled nested rects, because filled rects sit under the translucent glass and would darken the whole island by ~.26 black. Verified: without `edge=1` all SVGs byte-identical to HEAD~ (windows12 args floating + ring, all schemes, --deco, 5 button styles with line=1/gradient = 176 SVGs; only metadata `Id` = folder name differs); with it all SVGs parse, rsvg renders show the dark line over white (pixel 166) and the light one over black, corners join cleanly. The plan's `grep -c stroke-opacity = 2` counts lines, not shapes; the test counts shapes. In the ring theme the 8 strokes are all in `center`, none in `mask-center`.
next: Task 2 — Widget: key, checkbox + note, `Tiles.KWIN_CHECK`, `kwinPatched`, preset arg, kwinrc keys
next-model: opus

## 2026-10-17 16:00 Task 2: Widget: key, checkbox + note, `Tiles.KWIN_CHECK`, `kwinPatched`, preset arg, kwinrc keys
status: done
done: 7db3973 — `main.xml` `win12ReadableGlass` (Bool, default true, after `win12Noise`); `tiles.js` `KWIN_CHECK` under `PATCH_CHECK`; `main.qml`: `windows12` preset `+ (cfg.win12ReadableGlass ? " edge=1" : "")` (last arg), `win12Contrast` (30 / -1) + `win12Target` (85 light bar / 20 dark or accent / -1), both `on...Changed: blurTimer.restart()`, `own(key, v)` in `syncBlurStrength()` (write or `--delete`, before the reconfigure); `ConfigAppearance.qml`: `cfg_win12ReadableGlass`, `kwinPatched` (true until the check answers) from a `KWIN_CHECK` DataSource, checkbox + note right after the grain row; `tests/settings-keys.txt` baseline.
left: nothing
notes: Checks: settings keys `--strict` OK (142 keys), test_tiles + test_islands pass, qmllint error count 0 before and after on both files. The `sh -c` string was evaluated in node with mocks: on = `kwriteconfig6 ... Win12Contrast 30` + `Win12Target 85`, off = two `--delete`; both pass `sh -n`. New strings for Task 3 (both in ConfigAppearance.qml): "Readable glass: edge, shadow and an even background" and "The even background needs Win12's KWin patch: run kde/build.sh kwin from the Win12 source folder, then log out and in. Edge and shadow work without it." Not live-tested (Task 4).
next: Task 3 — i18n + docs
next-model: sonnet

## 2026-10-17 16:10 Task 3: i18n + docs
status: done
done: feaddb9 — `package/Messages.sh`; the 2 new strings translated in all 10 `po/*.po` (the long one had a fuzzy guess from msgmerge: flag removed and text written); `.mo` rebuilt with the install.sh msgfmt line (582 messages each, 0 fuzzy); README paragraph on `kde/build.sh kwin` under the patched-KDE section; `docs/FEATURES.md` l.97 (the file is in `docs/`, not the repo root) gains the readable-glass text; `kde/build.sh` usage line gains `[kwin]`.
left: nothing
notes: `settings keys OK (142 keys)`. Task 4 is the live test on this PC (plasmashell restarts allowed, at most 2; back up the 4 config files first).
next: Task 4 — Live test of layer A on this PC
next-model: opus

## 2026-10-17 16:07 Task 4: Live test of layer A on this PC
status: done
done: c275d58 — `tests/tools/w17edge.py` (column scan; pick an x through an EMPTY part of an island: icons, text and tooltips give bigger steps). `./install.sh --restart` once (no 2nd restart, no tune): theme 1d8ca74959 -> win12-taskbar-964f795aa5, 12 `shadow-` ids in all 3 variants, 0 svg/TypeError/ReferenceError in the journal. Floating split-right (x=1700): white step .268 mean_inside .890, black step .279 mean_inside .039. One-bar gap mode (x=700/1500): white step .340 mean .890, black step .254 mean .039. Gap column between the islands: no edge (white x=1229 step .039, black x=1200 step 0). All >= .06: pass.
left: nothing
notes: Values unchanged (edge black .35 / white .25, tiles 12 px .3 -> 0, gap rings k=6): no tune needed. Screen is 2560x1440 physical (logical 1920x1080, scale 1.333); island top edge at y=1386 (black line) / 1387 (white line); floating shadow = rows 1374-1385 (L 1.0 -> .71 over white), gap shadow = rows 1380-1385 inside the ring. Over black the shadow is invisible and the white inner line carries the edge, as designed. Layer B baseline for Task 8: mean_inside .890 (white) vs .039 (black) = .85 apart; over white the white tray icons and clock are hard to read (light ink on light glass), which is what B fixes. Gotchas: the agent shell had a stale WAYLAND_DISPLAY=wayland-0 (spectacle aborted "Authorization required"); use `WAYLAND_DISPLAY=wayland-1 XAUTHORITY=/run/user/1000/xauth_wRFbny` from `systemctl --user show-environment`. Gap mode (single + taskbarWindows=1) sets the panel `hiding=none`, so a maximized window ends above it; for the shots I set `panelById(2).hiding='autohide'` by script, then `none` again before switching back. Restored with taskbarWindows=0 + `setlayout.sh split-right` (903 ms): same widgets, tray extra=19, but the right panel and its weather/reveal widgets got new ids (535->571, 536->572), lengths 357/540 -> 299/457 (recorded before the restart, which also killed menu-started apps), and the theme hash 964f795aa5 -> 2f49088acd with the same look (white step .268 / mean .890 again; same edge fills). Hash input that moved is probably `win12Radius` (barThickness) or `accentHex`; not chased (pre-existing, not from edge=1) — worth a look in Task 7. Backups: `~/.config/{plasma-org.kde.plasma.desktop-appletsrc,plasmashellrc,win12islandsrc,kwinrc}.bak-glass-20261017-1602`. PNGs: `/tmp/claude-1000/scratch-glass/{white,black,gap-white,gap-black,white-after}.png`. Nothing left running; pointer at the centre.
next: Task 5 — kwin patch + QTest + `test_pkgbuild.sh` kwin case
next-model: opus

## 2026-10-17 16:11 Task 5: kwin patch + QTest + `test_pkgbuild.sh` kwin case
status: done
done: a69eba4 — `kde/patches/kwin/0001-win12-blur-readable-glass.patch` (9-line header + git diff; 6 files, +159/-31): `blur.kcfg` Win12Contrast (100) / Win12Target (50); new `src/plugins/blur/colormatrix.h` `KWin::win12ColorTransformMatrix(sat, contrast, target)` (transl = (1-contrast)*target; the old static function is gone from blur.cpp); `reconfigure()` builds `m_colorMatrix` (target .5) and `m_win12Matrix`; `blur()` picks `squeeze = w->isDock() || windowClass contains "plasmashell"` once after `modulation` and both on-screen setUniform calls use it; `blur.h` `m_win12Matrix`; `autotests/win12_blur_colormatrix_test.cpp` + CMake block after testFtrace (`win12BlurColorMatrix`, test name `win12-blur-colormatrix`, Qt::Test Qt::Gui). `tests/test_pkgbuild.sh`: kwin case (provides kwin-win12=6.8.0-1, no qt6-wayland).
left: nothing
notes: Standalone test from the PATCHED pristine v6.8.0 clone: 6 passed, 0 failed, no -Wall warnings (needs moc first: `/usr/lib/qt6/moc <test.cpp> -o <dir>/win12_blur_colormatrix_test.moc` and `-I<dir>`; inside the kwin build AUTOMOC does it). `patch -Np1 --dry-run` on a clean clone: rc 0, no fuzz. `sh tests/test_pkgbuild.sh` OK; a mutated provides makes it print "FAIL: kwin edit". blur.cpp itself was NOT compiled (needs the full kwin tree: that is Task 6). Uses `EffectWindow::isDock()`/`windowClass()` (effectwindow.h l.453/l.439). kwin installed = 6.8.0-1, qt6-base 6.12.0-3. Scratch clone: `/tmp/claude-1000/scratch-kwin/kwin` (reset to pristine).
next: Task 6 — Build `kwin-win12` (no install)
next-model: sonnet

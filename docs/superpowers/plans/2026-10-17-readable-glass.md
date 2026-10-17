# Readable glass — implementation plan (spec section 12)

> For agentic workers: follow the "Driver protocol" below. One task per run, one commit per task, then one
> progress-log entry. Steps use `- [ ]` checkboxes.

**Goal:** the Windows 12 islands look about the same over any background. Layer A (every KDE): a 1 px two-tone
edge and a soft shadow drawn by the generated Plasma Style. Layer B (patched KDE only, optional): a 4th patched
package, kwin, whose blur effect pulls the background behind panels and Plasma popups toward one brightness with
reduced contrast, driven by two `kwinrc [Effect-blur]` keys Win12 writes. Keys absent = stock output, bit for bit.

**Architecture:** `xptheme.py --taskbar ... edge=1` adds the edge lines to `frame()`/`ring_frame()` and the shadow
(Breeze-style `shadow-*` tiles for floating/docked bars, painted into the ring in gap mode). `main.qml` adds
`edge=1` to the `windows12` preset while `cfg.win12ReadableGlass` is on and writes/deletes `Win12Contrast` /
`Win12Target` in `syncBlurStrength()`. The kwin patch adds the two kcfg keys, moves `colorTransformMatrix` into a
header with a `target` parameter, builds a second matrix and uploads it for `isDock() || windowClass contains
"plasmashell"` windows in the existing on-screen pass. No shader change. One new settings key, one checkbox, one
note, one new detection command (`Tiles.KWIN_CHECK`).

**Tech stack:** KDE Plasma 6.8.0 (Wayland on the dev PC), KWin 6.8.0 (`src/plugins/blur`, kcfg/QTest), Python 3
SVG generator, QML, node tests, Arch `makepkg`, gettext (10 languages in `po/`), PIL for the screenshot check.

**Spec:** `docs/superpowers/specs/2026-10-16-fast-islands-design.md` section 12 (read it first every run: 12.1
is layer A with the Plasma shadow mechanics, 12.2 the kwin patch, 12.3 the widget side, 12.4 the tests).

**Base commit:** `6a473ee`. Line numbers below are from that commit; re-read the lines you touch.

## Evidence (read-only; from the 6.8.0 sources and the repo at 6a473ee)

- KWin 6.8 `src/plugins/blur/blur.cpp`: l.60 `static QMatrix4x4 colorTransformMatrix(qreal saturation, qreal
  contrast)` (contrast translation `(1.0 - contrast) / 2.0` in the 4th row, saturation with Rec.709 weights);
  l.250-266 `reconfigure()` reads `BlurConfig` and sets `m_colorMatrix = colorTransformMatrix(saturation/100,
  1.0)`; l.488 `shouldBlur()`; l.552 `blur(renderTarget, viewport, EffectWindow *w, ...)`; l.827 (rounded) and
  l.857 (plain) `setUniform(...colorMatrixLocation, m_colorMatrix)`. `blur.h` l.149 `QMatrix4x4 m_colorMatrix;`.
  `blur.kcfg`: group `Effect-blur`, `BlurStrength` 15, `NoiseStrength` 5, `Saturation` 150. Shaders
  `onscreen.frag` / `onscreen_rounded.frag`: `fragColor = (sum / 12.0) * colorMatrix`. `src/plugins/CMakeLists.txt`
  has `blur`, no contrast; `src/wayland/` has no contrast protocol. `EffectWindow` (`src/effect/effectwindow.h`):
  `bool isDock()` l.453, `QString windowClass()` l.439. `layershellv1window.cpp` l.26: scope `dock` ->
  `WindowType::Dock` (plasmashell panels on Wayland).
- KWin autotests: `autotests/CMakeLists.txt` l.202-210 shows the shape (`add_executable`, `target_link_libraries(x
  Qt::Test kwin)`, `add_test(NAME kwin-x ...)`, `ecm_mark_as_test`). Our test links `Qt::Test Qt::Gui` only.
  Root `CMakeLists.txt` l.510-515: `BUILD_TESTING` needs `Qt6WaylandClientPrivate` (Arch `qt6-wayland`, a kwin
  dependency already); `pacman -T` over the kwin PKGBUILD's depends+makedepends reports nothing missing.
- Arch kwin PKGBUILD (`gitlab.archlinux.org/archlinux/packaging/packages/kwin`, `6.8.0-1`): no `prepare()`,
  `-DBUILD_TESTING=OFF` on its own line, `validpgpkeys` present -> `kde/pkgbuild-win12.sh` works unchanged;
  `kde/build.sh kwin` clones tag `$(pacman -Q kwin | cut -d' ' -f2)`, runs the script with `kde/patches/kwin`,
  builds (`makepkg -f --noconfirm`, no `-s` without sudo), prints `WIN12_BUILD_OK kwin` or the pacman -U line.
- plasma-workspace 6.8 `shell/panelshadows.cpp` l.155-162 tiles `shadow-top .. shadow-topleft`, l.170-260 padding
  = `shadow-hint-<side>-margin` size (else tile size) + extra padding; `shell/panelview.cpp` l.1667-1689 borders
  disabled where the window + padding touches the screen edge, l.1709-1720 `updateShadows()` ->
  `PanelShadows::self()->addWindow(this, enabledBorders(), extraShadowPadding)`. plasma-desktop 6.8
  `desktoppackage/contents/views/Panel.qml` l.40 `prefix: ['floating', '']`, l.55-58 fixed floating paddings from
  the theme's `floating` prefix, l.69-72 `*ShadowMargin = -<floating inset>`.
- Repo: `xptheme.py` l.12 `frame(prefix, x0, y0, W, H, c, border, stops, hi=None, margins, gid, center, under, img,
  horiz, vpad)` (border = 1 px outer ring + inset fill, `hi` = 1 px top highlight only; corner quarter rings at
  l.52-56); l.229 `taskbar(base, colour, alpha, radius, line, fallback, image, mode, imagealpha, tint, buttons,
  accent, solid, gap, gradient, strength, ring, size)`; l.274 `ring_frame()`; l.288-300 the three variants loop
  (`""`, `translucent/`, `opaque/`), mask frame, `floating` prefix frame; l.681 `taskbar(a.out, **dict(kv.split("=",
  1) ...))` so a new kwarg is a new `k=v`. `main.qml`: l.395-405 `taskbarPresets` (windows12 preset), l.250
  `winDark`, l.403 `win12Hex`, l.620 `win12GapMode`, l.505-530 theme generation (hash of args + fallback; same
  hash = no regeneration), l.650-665 `Tiles.PATCH_CHECK` DataSource (`kdePatched`, `kdeChecked`), the
  `syncBlurStrength()` block (search `function syncBlurStrength`) with `one(key, saved, v)`, the flock and
  `reconfigureEffect`. `tiles.js` l.554 `PATCH_CHECK`. `config/ConfigAppearance.qml` l.26 `win12`, l.229-242 the
  blur and grain rows, l.39-41 `cfg_win12*`. `config/TaskbarLayoutBox.qml` l.15-26 the DataSource + note pattern.
  `tests/test_settings_keys.js` (one control per key, `--baseline`), `tests/test_pkgbuild.sh`,
  `tests/tools/w16mouse.py` (`x,y[,hold_s]`; `W16_SCREEN=WxH`), `package/Messages.sh`, `install.sh --restart`.
- The dev PC now: `taskbarLayout=split-right`, `taskbarWindows=0`, `taskbarGap=8`, `taskbarHiding=0`,
  `taskbarRound=30`, `win10AccentStart=true`, `colorMode=0` (follows the system), `taskbarFloating` default true
  -> floating islands (Plasma floating, autohide), accent-tinted glass; the one-bar gap mode is one layout switch
  away (`taskbarLayout=single`, `taskbarWindows=1`). Current theme `win12-taskbar-1d8ca74959`.

## Global constraints

- Everything from the stock-KDE-mode plan's "Global constraints" applies (Arch only; sudo NOT available to agents:
  they build with makepkg but never install; names `win12*`; short files; WHY comments; `// ponytail:` notes;
  `<= ~40 tool calls / ~100k tokens` per run; test and build output through `| tail`; grep then read small ranges,
  never a whole big file; live safety: back up `~/.config/plasma-org.kde.plasma.desktop-appletsrc`,
  `~/.config/plasmashellrc`, `~/.config/win12islandsrc` and `~/.config/kwinrc` before any live test; restart
  plasmashell only with `./install.sh --restart` and only in Task 4; never restart KWin: that is a logout).
- Waiting costs zero tokens: the kwin build (Task 6) is ONE `run_in_background` call; the agent ends its turn and
  continues on the completion notice. No polling, no sleep loops; one blocking `sleep` to let a window settle is
  fine. Nothing left running at the end of a run (`jobs`, no `qml`, no `w16mouse`, pointer away from the edge).
- Stock behaviour untouched: without `edge=1` every generated SVG is byte-identical to today (diff two
  generations); with the kwinrc keys absent the kwin patch's output equals upstream (defaults 100/50 give the
  identity contrast matrix).
- No task changes `kde/patches/{libplasma,plasma-workspace,plasma-desktop}`, the pacman hook or `kde/build.sh`'s
  default package list; `kde/build.sh kwin` is explicit.
- One test lock: at most two agents run tests at once; the reviewer (Task 7) is read-only apart from running tests.
- Strings: new user-visible strings go through `package/Messages.sh`; 10 `po/*.po` translated (no fuzzy), `.mo`
  rebuilt with the `install.sh` msgfmt line.
- Commits: one per task, message `<area>: <what>` + a short why, ending with the two attribution lines the driver
  prompt gives (when it gives none: the same two lines as this plan's commit).

## Driver protocol (progress log format)

File: `docs/superpowers/plans/2026-10-17-readable-glass-progress.md`. Each run:

1. Reads the spec (section 12), this plan, and the progress log. The last entry's `next:` line names the task to
   do; `next-model:` names the model the driver must start for it.
2. Does that task's steps, verifies, commits, then appends ONE entry:

```
## <YYYY-MM-DD HH:MM> Task N: <title>
status: done | blocked | partial | skipped
done: <what landed, commit hash>
left: <what is not done, or "nothing">
notes: <facts the next run needs; numbers; files>
next: Task M — <title>            (or `next: none` when Task 8 is done)
next-model: opus | sonnet | none
```

3. Exits. The driver reads the LAST `next-model:` line to choose the model for the next run.

Rules: `status: blocked` is for tasks that need something unavailable (sudo pacman, the user at the keyboard, a
missing makedepend). A blocked task's dependants (see **Depends on**) are `skipped` with `left: blocked by Task
N`; `next:` then points at the next task that does not depend on it. `partial` = out of budget mid-task: `next:`
names the SAME task and `notes:` the exact step to resume at (work committed at a safe point first, or `git stash
push -m "Task N partial"` and noted). A model name in a task header (`Model: opus|sonnet`) is what the previous
entry's `next-model:` must carry. Task 8 is the user's: the agent that finishes Task 7 writes `next: Task 8 —
user's kwin install and live check` and `next-model: none`.

## Task order

1. `xptheme.py`: `edge=1` (two-tone edge, shadow tiles, gap-ring shadow) + generation checks
2. Widget: key, checkbox + note, `Tiles.KWIN_CHECK`, `kwinPatched`, preset arg, kwinrc keys
3. i18n + docs (10 translations, README/FEATURES line)
4. Live test of layer A on this PC (screenshots over white and black, both layouts), tune, `w17edge.py`
5. kwin patch + QTest + `tests/test_pkgbuild.sh` kwin case, dry-run apply and a standalone compile of the test
6. Build `kwin-win12` (no install), record the pkg path and the user's checklist
7. Whole-change review + ELI5
8. The user: install kwin-win12, log out/in, live check of layer B

Tasks 1-4 (A) and 5-6 (B) are independent; A first because it is visible today on every KDE.

## Review focus

- Byte-identical SVGs without `edge=1`: generate the current `windows12` args twice (with and without the new code
  path) and `diff -r`; the mask frames never change; the `floating` prefix frame is untouched.
- Edge lines are inside the shape (the fill is inset by `b`), 1 px each, present on all 4 sides and the 4 corners
  of every piece; gap mode's strokes are at .5 and 1.5 px inset so they render crisp at 1x.
- Shadow tiles only in the non-ring theme, in all 3 variants, with the 4 hint margins = tile size; the ring theme
  paints its shadow inside the ring and never outside the center element's box.
- `main.qml`: `edge=1` only in the `windows12` preset and only while `cfg.win12ReadableGlass`; the kwinrc keys are
  written while `win12Glass && cfg.win12ReadableGlass` and deleted otherwise (also when Windows 12 goes off);
  `onWin12ContrastChanged`/`onWin12TargetChanged` restart `blurTimer`; `KWIN_CHECK` is a separate DataSource and
  nothing in the layout engine reads `kwinPatched`.
- kwin patch: defaults 100/50 give the identity contrast matrix (`qFuzzyCompare(contrast, 1.0)` short-circuits);
  the per-window pick touches both on-screen passes (l.827 and l.857) and nothing else in `blur()`; the header's
  function is the only copy (the old static one is gone); the test is registered with `win12` in its name; the
  patch applies with `patch -Np1` to the 6.8.0 tarball (that is what `prepare()` does).
- Settings: `node tests/test_settings_keys.js --strict` OK, baseline updated, one control for `win12ReadableGlass`.
- Strings: every new string in `po/template.pot` and translated in the 10 po files, `.mo` rebuilt, `msgfmt --check`
  clean.

## Defaults picked without asking (the user should know)

- Edge: outer black .35, inner white .25, independent of the bar colour (one of them shows on any background).
  Shadow: 12 px tiles, black .3 -> 0 (floating/docked); 6 nested rects at .05 in gap mode. Task 4 may tune these
  +/- .1 from the screenshots and records the final values.
- Squeeze: `Win12Contrast=30`, `Win12Target` 20 for a dark or accent bar, 85 for a light bar (same light/dark rule
  as the theme's ink). No slider in v1; the two kwinrc keys are the knob.
- Which windows: `isDock()` or `windowClass()` containing `plasmashell` (panels, third-party docks, every
  plasmashell popup incl. the Win12 menu). App windows and krunner keep the stock blur.
- The kwinrc keys are written whether or not kwin is patched (a stock KWin ignores unknown keys); the Appearance
  note tells the user what is missing. The pacman hook is not extended to kwin: after a stock KWin update B turns
  off silently and the note comes back.
- `edge=1` applies to the `windows12` preset only (the other presets are near-opaque).
- The checkbox default is on: existing users get the edge and shadow at their next theme regeneration.

---

### Task 1: `xptheme.py`: `edge=1` — two-tone edge, shadow tiles, gap-ring shadow

**Model:** opus. **Depends on:** nothing.

**Files:**
- Modify: `package/contents/ui/code/xptheme.py` (`frame()` l.12-80, `taskbar()` l.229-300, `ring_frame()` l.274)

**Interfaces:** `taskbar(..., edge="0")`; `edge` truthy (not `"0"`, `""`, `"false"`) turns on the three things.
`frame()` gets `edge=None` = `(outer_colour, outer_opacity, inner_colour, inner_opacity)`; when given it implies
`border=(outer_colour, outer_opacity)` and draws the inner 1 px ring on all four sides and corners (radii `c - 1`
and `c - 2`, same quarter-ring path shape as the border at l.52-56). A helper `shadow_tiles(x0, y0, S=12, op=.3)`
returns the 8 `shadow-*` groups + 4 `shadow-hint-*-margin` rects (sides: `<linearGradient>` black `op` -> 0 going
outward; corners: `<radialGradient>` centred on the inner corner, radius S). `ring_frame()` gets `edge` and
`shadow` flags: strokes `stroke="#000" stroke-opacity=".35"` at .5 px inset and `stroke="#fff" stroke-opacity=".25"`
at 1.5 px inset on the rounded rect (`fill="none"`, `stroke-width="1"`), and before the box 6 rects grown by
`i = 1..min(6, n - 1)` px (rx + i) with `fill="#000" fill-opacity=".05"`.

**Steps:**
- [ ] Read spec 12.1 and `xptheme.py` l.12-80, l.229-300. Record today's output first: `python3
      package/contents/ui/code/xptheme.py --taskbar colour=3d8ee6 alpha=0.15 radius=12 line=0 buttons=win11 gap=8
      fallback=default --out /tmp/claude-1000/.../scratchpad/rg-before` and the same with `ring=8 size=1200x44`
      into `rg-before-ring` (use the session scratchpad path).
- [ ] `frame()`: add `edge=None`; when set, `border = edge[:2]` and after the border ring in `piece_corner` add the
      inner quarter ring (`c - b` to `c - b - 1`) in `edge[2:]`; add the 1 px inner rects to `top` (replaces the
      `hi` rect when `edge`), `bottom`, `left`, `right`. Keep `hi` working for the windows7 preset. Comment WHY:
      "readable glass: the dark line shows over white, the light one over black".
- [ ] `shadow_tiles()` + call it in the non-ring branch of the variants loop (l.295-300) when `edge`, placed below
      the `floating` frame (y past `30 + 2 * fh + 60`); 8 groups: `shadow-top` (W x S), `shadow-bottom`,
      `shadow-left` (S x H), `shadow-right`, four S x S corners; hints `shadow-hint-{top,right,bottom,left}-margin`
      sized S. Mind the svg `size=` so nothing is clipped.
- [ ] `ring_frame()`: strokes and the shadow stack when `edge`; the mask ring_frame call stays without them.
- [ ] `taskbar(..., edge="0")`: `on = edge not in ("0", "", "false")`; `frame(..., edge=(K, .35, W, .25) if on else
      None)` in the panel frame only (not the mask, not the `floating` frame); `hi` unchanged when `edge` is off.
- [ ] Verify: regenerate both `rg-before*` sets into `rg-after*` WITHOUT `edge=1`: `diff -r` empty for both. Then
      with `edge=1` into `rg-edge` and `rg-edge-ring`: every `.svg` parses (`python3 -c 'import sys, xml.etree
      .ElementTree as E; [E.parse(p) for p in sys.argv[1:]]' $(find rg-edge* -name '*.svg')`); `grep -c
      'id="shadow-' rg-edge/widgets/panel-background.svg` = 12 (and the same in `translucent/` and `opaque/`), 0 in
      `rg-edge-ring`; `grep -c 'stroke-opacity' rg-edge-ring/widgets/panel-background.svg` = 2 (not in the mask
      group: check the `mask-center` group has no stroke). Optional visual: `rsvg-convert` or `qml` if present,
      else skip.
- [ ] Commit: `xptheme: readable glass — edge=1 draws a two-tone 1 px edge and a shadow (tiles for floating/docked
      bars, painted ring in gap mode)`. Entry `notes:` carries the opacities and the tile size.

### Task 2: Widget: key, checkbox + note, `Tiles.KWIN_CHECK`, `kwinPatched`, preset arg, kwinrc keys

**Model:** opus. **Depends on:** Task 1 (the arg must exist before the preset sends it).

**Files:**
- Modify: `package/contents/config/main.xml` (after `win12Noise` l.364-367)
- Modify: `package/contents/ui/code/tiles.js` (l.554, add `KWIN_CHECK`)
- Modify: `package/contents/ui/main.qml` (l.395-405 preset, l.650-665 check pattern, the `syncBlurStrength` block)
- Modify: `package/contents/ui/config/ConfigAppearance.qml` (l.39-41 property, after the grain row l.236-242)
- Modify: `tests/settings-keys.txt` (via `--baseline`)

**Interfaces:** key `win12ReadableGlass` (Bool, default true, label "W18 Windows 12 readable glass: two-tone edge
+ shadow in the theme (edge=1) and the kwin-win12 squeeze keys"). `Tiles.KWIN_CHECK = "pacman -T kwin-win12
>/dev/null 2>&1 && echo ok"`, read by `ConfigAppearance.qml` only (`main.qml` writes the keys either way, so it
needs no check). Preset: `taskbarPresets.windows12` gets `+ (cfg.win12ReadableGlass ? " edge=1" : "")`. `readonly property int win12Contrast: win12Glass && cfg.win12ReadableGlass ? 30 : -1`,
`readonly property int win12Target: win12Contrast < 0 ? -1 : Kirigami.ColorUtils.brightnessForColor("#" +
win12Hex) === Kirigami.ColorUtils.Light ? 85 : 20`; both `on...Changed: blurTimer.restart()`. In
`syncBlurStrength()` a second helper `own(key, v)` = `v >= 0 ? "kwriteconfig6 " + g + key + " " + v + "; " :
"kwriteconfig6 " + g + key + " --delete; "` appended to the same `sh -c` string before the `reconfigureEffect`.

**Steps:**
- [ ] Read spec 12.3, `main.qml` l.395-410, l.650-665, the `syncBlurStrength` block, `ConfigAppearance.qml`
      l.20-45 and l.212-250, `TaskbarLayoutBox.qml` l.15-26.
- [ ] `main.xml` entry; `tiles.js` `KWIN_CHECK` next to `PATCH_CHECK` with a WHY comment (separate from the three:
      layouts never depend on kwin).
- [ ] `main.qml`: preset arg, `win12Contrast`/`win12Target`, `own()` in `syncBlurStrength()`, the two `onChanged`
      lines. No `kwinPatched` in main.qml (nothing would read it). Update the block comment above `win12Glass`
      (it names the keys Win12 owns).
- [ ] `ConfigAppearance.qml`: `property bool cfg_win12ReadableGlass`; after the grain row a `QQC2.CheckBox`
      (`visible: page.win12`, text `i18n("Readable glass: edge, shadow and an even background")`, `checked:
      page.cfg_win12ReadableGlass`, `onToggled`); below it a `QQC2.Label` (`visible: page.win12 &&
      page.cfg_win12ReadableGlass && !page.kwinPatched`, wrap, `Layout.maximumWidth: Kirigami.Units.gridUnit *
      24`, text `i18n("The even background needs Win12's KWin patch: run kde/build.sh kwin from the Win12 source
      folder, then log out and in. Edge and shadow work without it.")`) and a `P5Support.DataSource` on
      `Tiles.KWIN_CHECK` setting `page.kwinPatched` (import `tiles.js` as the layout box does).
- [ ] Verify: `node tests/test_settings_keys.js --baseline && node tests/test_settings_keys.js --strict | tail -1`
      (OK, 142 keys); `node tests/test_tiles.js | tail -1`; `node tests/test_islands.js | tail -1`; `qmllint
      package/contents/ui/main.qml package/contents/ui/config/ConfigAppearance.qml 2>&1 | grep -c error` = 0 (or
      equal to the base count: check base first with `git stash`); `sh -n` is not possible for the generated
      `sh -c` string, so print `syncBlurStrength`'s string once through a `console.log` in a scratch copy or read
      it carefully: two `kwriteconfig6 ... Win12Contrast` and `Win12Target` writes in the on case, two `--delete`
      in the off case.
- [ ] Commit: `settings/main.qml: readable glass — win12ReadableGlass key and checkbox, edge=1 in the Windows 12
      preset, Win12Contrast/Win12Target kwinrc keys, KWIN_CHECK note`.

### Task 3: i18n + docs

**Model:** sonnet. **Depends on:** Task 2.

**Files:**
- Modify: `po/template.pot`, `po/*.po` (ar de es fr hi it ja pt_BR ru zh_CN), `package/contents/locale/*/LC_MESSAGES/plasma_applet_org.win12.menu.mo`
- Modify: `README.md`, `FEATURES.md` (the blur line, FEATURES l.48 area), `kde/build.sh` header comment (usage line
  gains `[kwin]`; the loop needs no change)

**Steps:**
- [ ] `sh package/Messages.sh` (from anywhere); translate the 2 new strings in every po file (plain, no fuzzy);
      rebuild the `.mo` files with the `install.sh` msgfmt line; `msgfmt --check --statistics` on each `| tail -1`.
- [ ] README: one paragraph under the patched-KDE section: the optional 4th package, `kde/build.sh kwin`, what it
      changes, the way back `sudo pacman -S kwin`. FEATURES: extend the transparency/blur line with "two-tone edge
      and shadow; even background behind the glass with the kwin-win12 patch".
- [ ] `kde/build.sh` l.3 usage comment: `[kwin]` with "(not built by default; spec 12.2)". `kde/hook` untouched.
- [ ] Verify: `grep -c fuzzy po/*.po` all 0; `node tests/test_settings_keys.js --strict | tail -1` OK.
- [ ] Commit: `i18n/docs: readable glass — 2 strings in 10 languages, README/FEATURES, build.sh usage`.

### Task 4: Live test of layer A on this PC

**Model:** opus. **Depends on:** Tasks 1-3.

What it proves: the generated theme loads (no KSvg error, mask and blur unchanged), the edge is visible over a
white and over a black window in both layouts, the shadow shows where spec 12.1 says (hugging the floating
islands, top-only on the docked bar, inside the ring in gap mode), nothing else moved (island sizes, margins,
autohide). What it cannot prove: layer B (kwin not patched yet; Task 8).

This task restarts plasmashell through `./install.sh --restart` (new widget code) and possibly once more per
tuning round. Apps started from the Start menu die with it. Do it only when the previous entry or the driver
prompt says the user is away or agreed.

**Files:**
- Create: `tests/tools/w17edge.py` (~25 lines, PIL: `w17edge.py <png> <x> [bottom_px=120]` prints `edge_y=<row>
  step=<max |dL| between neighbouring rows in the bottom band> mean_inside=<mean L of the 10 rows under edge_y>`,
  L = Rec.709 luminance 0..1)
- Create (scratchpad, not committed): `white.qml` / `black.qml` = `import QtQuick; Window { visible: true;
  visibility: Window.Maximized; color: "white" }`

**Steps:**
- [ ] Read spec 12.4; the multimonitor plan Task 5 (`docs/superpowers/plans/2026-10-17-multimonitor-islands.md`
      l.613-660) for the layout-switch method and timing helpers; the settings-reorg progress log l.15-30 for the
      screenshot procedure (`spectacle -b -n -o`, `w16mouse` rows, `QT_FORCE_STDERR_LOGGING`).
- [ ] Back up the four config files (`.bak-glass-<date>`); record layout, theme name (`kreadconfig6 --file plasmarc
      --group Theme --key name`), panel ids; `git status` clean.
- [ ] `./install.sh --restart 2>&1 | tail -5`; ONE `sleep 8`; the theme name changed (new hash); `ls
      ~/.local/share/plasma/desktoptheme/win12-taskbar-*/widgets/panel-background.svg | xargs grep -c
      'id="shadow-'` = 12 (floating) ; `journalctl --user -u plasma-plasmashell -b --since -1min | grep -ci
      'svg\|TypeError\|ReferenceError'` = 0.
- [ ] Shots, one blocking command each: `(timeout 25 /usr/lib/qt6/bin/qml white.qml &) ; sleep 2; python3
      tests/tools/w16mouse.py 960,1079,6 & sleep 2.5; spectacle -b -n -o <scratch>/white.png; wait` (same for
      black). Then `python3 tests/tools/w17edge.py white.png <x through an island>` and `... black.png <x>`: both
      `step >= .06`; note `mean_inside` of both (expected far apart today: ~.9 vs ~.15 with an accent bar; this is
      the baseline for Task 8). Also one column through the gap BETWEEN islands shows no edge (sanity).
- [ ] Switch to the one-bar gap mode (`taskbarLayout=single`, `taskbarWindows=1` with the Task 5 method), wait
      for `done`, repeat the two shots and the two measurements; shadow visible inside the ring (a second
      `w17edge.py` call with `bottom_px` covering the ring; the step at the island edge >= .06). Switch back to
      the user's layout and position; the desktop equals the recording.
- [ ] Tune only if a step fails or the look is clearly off (shadow too heavy/light): edit `xptheme.py` opacities,
      then the retune loop: `rm -rf ~/.local/share/plasma/desktoptheme/win12-taskbar-*; plasma-apply-desktoptheme
      "$(kreadconfig6 --file ~/.config/plasma-org.kde.plasma.desktop-appletsrc --group ... savedDesktopTheme ||
      echo default)"; ./install.sh --restart` (same args = same hash = no regeneration otherwise). Max two rounds;
      record the final values in `notes:` and in the plan's "Defaults" line if they changed.
- [ ] Nothing left running (`jobs`; no `qml`, `w16mouse`, `spectacle`; pointer moved to 960,540). Keep the four
      PNGs in the scratchpad path named in `notes:`.
- [ ] Commit: `tests/tools: w17edge.py (edge step and inside luminance of a screenshot column)` plus any xptheme
      tune: `xptheme: readable glass — live tune (<values>)`.

### Task 5: kwin patch + QTest + `test_pkgbuild.sh` kwin case

**Model:** opus. **Depends on:** nothing (B is independent of A).

**Files:**
- Create: `kde/patches/kwin/0001-win12-blur-readable-glass.patch`
- Modify: `tests/test_pkgbuild.sh` (a kwin PKGBUILD variant: edit succeeds, `provides = kwin-win12=...`, no
  `qt6-wayland` line added)
- Scratch (not committed): a shallow clone of `https://invent.kde.org/plasma/kwin.git` at `v6.8.0` in the
  scratchpad to author and `git diff` the patch

**Interfaces (the patch, `-Np1` against `kwin-6.8.0/`):**
- `src/plugins/blur/blur.kcfg`: entries `Win12Contrast` (Int, default 100) and `Win12Target` (Int, default 50)
  in group `Effect-blur`.
- New `src/plugins/blur/colormatrix.h`: `inline QMatrix4x4 win12ColorTransformMatrix(qreal saturation, qreal
  contrast, qreal target)` = the l.60 function with `transl = (1.0 - contrast) * target`; a 6-line comment: why
  (readable glass), the row-vector convention of `onscreen.frag`, `target .5` = upstream.
- `blur.cpp`: delete the static l.60-86 function, `#include "colormatrix.h"`; `reconfigure()`: `m_colorMatrix =
  win12ColorTransformMatrix(sat, 1.0, 0.5); m_win12Matrix = win12ColorTransformMatrix(sat,
  BlurConfig::win12Contrast() / 100.0, BlurConfig::win12Target() / 100.0);`; in `blur()` before the two on-screen
  branches: `const bool squeeze = w->isDock() || w->windowClass().contains(QLatin1String("plasmashell")); const
  QMatrix4x4 &colorMatrix = squeeze ? m_win12Matrix : m_colorMatrix;` and both `setUniform(...colorMatrixLocation,
  colorMatrix)`. `blur.h` l.149: `QMatrix4x4 m_win12Matrix;` with a one-line comment.
- `autotests/win12_blur_colormatrix_test.cpp` (QTest, `#include "../src/plugins/blur/colormatrix.h"`): rows apply
  as `QVector4D(r, g, b, 1) * M`; cases: (1.0, 1.0, .5) identity on 5 colours; (1.5, 1.0, .5) identity on greys
  and luminance-preserving on a colour (|dL| < 1e-4); (1.0, .3, .2) white -> .44 and black -> .14; (1.0, .3,
  .85) white -> .895, black -> .595 (tolerance 1e-4); defaults 100/50 after `/100.0` equal case 1.
  `autotests/CMakeLists.txt`: `add_executable(win12BlurColorMatrix win12_blur_colormatrix_test.cpp)`,
  `target_link_libraries(win12BlurColorMatrix Qt::Test Qt::Gui)`, `add_test(NAME win12-blur-colormatrix COMMAND
  win12BlurColorMatrix)`, `ecm_mark_as_test(...)`.

**Steps:**
- [ ] Read spec 12.2; `git clone -q --depth 1 --branch v6.8.0 https://invent.kde.org/plasma/kwin.git <scratch>/kwin`;
      read `src/plugins/blur/blur.cpp` l.55-90, l.250-266, l.800-870 and `blur.h` l.140-155 (small ranges only).
- [ ] Make the edits in the clone; `git -C <scratch>/kwin diff --stat`; write the patch with a header paragraph
      (what, why, keys, which windows) like the three existing patches: `git -C <scratch>/kwin diff >
      kde/patches/kwin/0001-win12-blur-readable-glass.patch` (new files included: `git add -N` first).
- [ ] Standalone compile + run of the test (proves the math before the long build): `g++ -std=c++20 -fPIC
      $(pkg-config --cflags Qt6Test Qt6Gui) -I<scratch>/kwin <scratch>/kwin/autotests/win12_blur_colormatrix_test.cpp
      $(pkg-config --libs Qt6Test Qt6Gui) -o <scratch>/cm && QT_QPA_PLATFORM=offscreen <scratch>/cm | tail -3`
      (all PASS). If `pkg-config` lacks Qt6Test, note the missing package (`qt6-base` has it on Arch).
- [ ] Apply check against the Arch source tarball shape: `git -C <scratch>/kwin stash` is not enough — do `git
      -C <scratch>/kwin checkout -- . && git clean -fdq && patch -Np1 --dry-run -d <scratch>/kwin -i
      $PWD/kde/patches/kwin/0001-*.patch | tail -3`.
- [ ] `tests/test_pkgbuild.sh`: add the kwin variant (copy of the libplasma block with `pkgname=kwin`, expect
      `provides = kwin-win12=6.8.0-1` and no `qt6-wayland`); `sh tests/test_pkgbuild.sh | tail -2` OK.
- [ ] Commit: `kde: kwin patch — readable glass squeeze in the blur effect (Win12Contrast/Win12Target, panels and
      plasmashell popups), QTest, test_pkgbuild kwin case`.

### Task 6: Build `kwin-win12` (no install)

**Model:** sonnet. **Depends on:** Task 5.

What it proves: the patch applies to Arch's 6.8.0 source in `prepare()`, everything compiles with `BUILD_TESTING=ON`,
`check()` runs `win12-blur-colormatrix` (and nothing else of ours), the package is ready for the user.

**Steps:**
- [ ] `pacman -Q kwin` is `6.8.0-1` (else `notes:` the version; build.sh clones that tag). Check deps once:
      `cd <scratch>/kwin-pkgbuild && curl -s -o PKGBUILD https://gitlab.archlinux.org/archlinux/packaging/packages/kwin/-/raw/main/PKGBUILD
      && bash -c 'source ./PKGBUILD; pacman -T "${makedepends[@]}" "${depends[@]}"'` prints the missing ones; if
      any, `status: blocked` with the exact `sudo pacman -S --asdeps <list>` for the user and stop.
- [ ] ONE background call: `kde/build.sh kwin > <scratch>/kwin-build.log 2>&1` with `run_in_background`; end the
      turn. Expect 30-90 min (kwin plus ~130 integration test executables). On the completion notice: `tail -5
      <scratch>/kwin-build.log` shows `WIN12_BUILD_OK kwin` and the `to install:` line; `grep -c
      'win12-blur-colormatrix .*Passed' <scratch>/kwin-build.log` = 1 (ctest line in check()).
- [ ] If makepkg fails in `prepare()` (patch no longer applies) or in the build: `status: blocked`, `left:` the
      first error (`grep -n -m3 'error\|FAILED\|Hunk' log`), `next: Task 5` to fix the patch.
- [ ] Record in `notes:`: the pkg path (`ls ~/.cache/win12-kde/kwin/*.pkg.tar.zst`), build time, the kwin version,
      and the nested-run recipe for anyone who wants it later: `LD_LIBRARY_PATH=<pkgdir>/usr/lib
      QT_PLUGIN_PATH=<pkgdir>/usr/lib/qt6/plugins <pkgdir>/usr/bin/kwin_wayland --virtual --no-lockscreen -- ` plus a
      blur-requesting dock client and `spectacle` inside; not done (spec 12.4).
- [ ] No commit (nothing in the repo changed); the log entry alone.

### Task 7: Whole-change review + ELI5

**Model:** opus. **Depends on:** Tasks 1-6 (4 or 6 may be `skipped`/`blocked`; the review then says so).

**Steps:**
- [ ] `git diff 6a473ee --stat`; read the diffs of `xptheme.py`, `main.qml` (the preset, the check, the
      `syncBlurStrength` block only), `tiles.js`, `main.xml`, `ConfigAppearance.qml`, the kwin patch, the tests
      and docs. Check every item of "Review focus".
- [ ] Run: `node tests/test_tiles.js | tail -1`, `node tests/test_islands.js | tail -1`, `node
      tests/test_settings_keys.js --strict | tail -1`, `sh tests/test_pkgbuild.sh | tail -1`, `qmllint` on the two
      QML files, `msgfmt --check` on the 10 po files `| tail -3`, the Task 1 byte-identity diff once more.
- [ ] Fix small findings directly (comments, an opacity typo, a missing `--delete`); bigger ones to `left:` with
      the exact line and why.
- [ ] Append an ELI5 paragraph to the entry (5-8 lines: what every user gets (edge + shadow), what the patched
      user gets after installing kwin-win12 (even background), why the look is the same over white and black, how
      the dev PC was tested, what only the user can test) and the Task 8 checklist pointer.
- [ ] Commit (when something changed): `review: readable glass — <fixes>`. Entry: `next: Task 8 — user's kwin
      install and live check`, `next-model: none`.

### Task 8: The user: install kwin-win12, log out/in, live check of layer B (user, at the keyboard, needs sudo)

Do this with nothing important open: logging out closes everything. About 10 minutes plus the log in/out.

- [ ] Before: `ls ~/.cache/win12-kde/kwin/*.pkg.tar.zst` lists the built package (Task 6's `notes:` has the path;
      if missing, run `kde/build.sh kwin` in a terminal first: it builds and, with sudo, installs). `pacman -Q
      kwin` is `6.8.0-1`.
- [ ] `cp ~/.config/kwinrc{,.bak-glass}`. Note how an island looks over a white and a dark window now (or keep
      Task 4's PNGs).
- [ ] `sudo pacman -U ~/.cache/win12-kde/kwin/kwin-6.8.0-1-x86_64.pkg.tar.zst` (the exact file name from `ls`).
      `pacman -T kwin-win12` prints nothing. Log out and in (KWin is replaced on login; the Wayland session cannot
      restart KWin in place).
- [ ] After login: `kreadconfig6 --file kwinrc --group Effect-blur --key Win12Contrast` = 30 and `Win12Target` =
      20 (dark/accent bar) or 85 (light bar); if empty, open Win12 Settings > Appearance, toggle "Readable glass"
      off and on, wait 2 s, read again.
- [ ] Look: maximize a white window (a browser on a blank page) and a dark one; the islands should look nearly
      the same grey behind the tint on both; blur still visible; app windows with blur unchanged; the Start menu
      background also evened. Optional measure: Task 4's shot commands + `python3 tests/tools/w17edge.py
      <png> <x>`: `mean_inside` of white vs black shots differs by <= .2 (Task 4 recorded ~.75 before).
- [ ] The Appearance page no longer shows the KWin note.
- [ ] `journalctl --user -b | grep -i 'blur' | tail -3` shows no shader or effect error.
- [ ] Way back at any time: `sudo pacman -S kwin` (stock), log out and in; Win12 keeps working (A stays, the note
      returns). Keys can be removed by hand: `kwriteconfig6 --file kwinrc --group Effect-blur --key Win12Contrast
      --delete` (same for `Win12Target`) and `qdbus6 org.kde.KWin /Effects reconfigureEffect blur`.
- [ ] Tell the agent the result (good / which step failed / a photo) so the progress log can close with `next: none`.

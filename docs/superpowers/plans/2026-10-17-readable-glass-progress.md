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

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

# Win12

![Win12](docs/images/hero.png)

Inspired by Stardock Start11.

Complete Windows-style theming for KDE Plasma 6. Change the style of the Start menu, taskbar, Start button and
window decorations together, or mix and match each one. Every part also has a KDE Plasma default, so you can
leave any of them as it was.

## Windows XP

![Windows XP](docs/images/xp.webp)

- Three menu looks: Classic XP, Modern classic and Modern.
- Colour schemes Blue, Olive Green and Silver, or your own custom colour.
- Option to use XP-style icons.
- Lighter tray strip on the taskbar, as in XP.
- XP menu styles come with a matching XP taskbar theme.

## Windows 7

![Windows 7](docs/images/windows7.webp)

Classic and Modern variants, with a matching taskbar and window decorations.

## Windows 10

![Windows 10](docs/images/windows10.webp)

Live-tile groups, folders and pages, with a matching taskbar and window decorations.

## Windows 11

![Windows 11](docs/images/windows11.webp)

Pinned grid, folders and recommended list, with an optional centred Start button and taskbar icons.

## Windows 12

![Windows 12](docs/images/windows12.webp)

- A menu based on the Windows 12 concept art. The app list can sit on the left or the right, and the menu
  position is Left, Center or Right.
- Floating island taskbar that hides itself over full-screen windows.
- Choose: windows fill the screen under an auto-hiding island, or keep a gap above an always-visible island.
- Dock-style split islands, with an optional magnetic effect.
- Transparency and blur, with sliders for How light, How dark, Transparency and Blur strength.
- Bottom spacing and menu gap settings.
- Pop-up animations.
- Gradient option.
- Your own images for the taskbar and the title bars.

## Other styles

![Other styles](docs/images/others.webp)

App, Pro, Launcher and the KDE Plasma default.

## More features

- Tiles, including real KDE widgets inside tiles.
- Firefox title bar handling that works with the XP style.
- Icons page with downloads from the KDE Store.
- Taskbar auto-hide option and menu position setting.
- Right-click menus.
- Search with filters and history, pinned apps, websites, files and folders, keyboard navigation.
- Backup, restore and reset of all settings. Everything Win12 changes (taskbar style, title bars, colours) is
  undone when you switch it off, because your own Plasma Style, window decoration and colour scheme are saved first.
- Translations: English, Spanish, French, German, Italian, Brazilian Portuguese, Russian, Arabic, Simplified
  Chinese, Japanese and Hindi. It follows your Plasma language.

Full list: [docs/FEATURES.md](docs/FEATURES.md)

## Requirements

- KDE Plasma 6.x. Tested on Plasma 6.8, Arch Linux, Wayland.
- `python3` (generates the taskbar styles, title bars and colour schemes).
- `qdbus6` (also `qdbus-qt6`), `kreadconfig6`, `kwriteconfig6`, `kpackagetool6` (part of Plasma / KDE Frameworks).

## Install

```sh
git clone https://github.com/Kalaminator/Win12.git
cd Win12
./install.sh            # add --restart to reload Plasma right away
```

Then right-click your panel, choose *Add or Manage Widgets*, search **Win12** and drag it to the panel. Open its
settings with right-click, *Configure Win12*, or run `win12-settings`.

If you used the old Start12 version, `install.sh` migrates your setup automatically and keeps a backup in
`~/.config/win12-migration-backup-*`.

Update: `git pull && ./install.sh --restart`.

## Patched KDE for the taskbar layouts (Arch)

The split and 3 Islands layouts and the Start position move panel widgets through Plasma's scripting. Stock Plasma
6.8 does that one step at a time (5-8 s per switch) and loses tray icons when a tray is moved, so Win12 ships small
patches for three packages and rebuilds them from Arch's own PKGBUILDs:

    kde/build.sh            # libplasma, plasma-workspace, plasma-desktop; 20-40 min; asks for sudo to install

Without the patched packages the split layouts and the Start position still work the slow way (one step every 0.3 s, 5-10 s per change, icons jump). 3 Islands and dragging apps between islands are off, and the Taskbar page says so. After each
Plasma update that note is the sign to run `kde/build.sh` again (`kde/build.sh` also installs the tiny `win12-kde-hook` package, a pacman hook that prints
"Run kde/build.sh again" after a stock Plasma update; it is its own package so the upgrade cannot remove it). Undo at any time:
`sudo pacman -S libplasma plasma-workspace plasma-desktop`, then
`sudo pacman -R win12-kde-hook libplasma-debug plasma-workspace-debug plasma-desktop-debug` (makepkg also built and installed
those). Patches: `kde/patches/`; what they change is in
`docs/superpowers/specs/2026-10-16-fast-islands-design.md`.

Multi-monitor (extended desktops): Taskbar page → *Show islands on* picks the monitor (default: the main one) or
*All monitors* (Start and the app islands on every monitor, the tray island on the main one; *Open apps on other
monitors* chooses that monitor's windows or all). Pin apps from the main monitor; a monitor plugged or unplugged
rebuilds the islands. Mirrored screens need nothing.

## Uninstall

```sh
kpackagetool6 -t Plasma/Applet -r org.win12.menu
```

## Icons and wallpapers

No Microsoft icons or wallpapers are bundled. You can get community icon packs from the KDE Store through the
Icons page in the settings.

## Project layout

- `package/`: the Plasma widget (`org.win12.menu`).
- `tests/`: tests for the pure logic (`node tests/test_tiles.js`).
- `po/`: translations.
- `tools/make-release.sh`: builds the `.plasmoid` package.
- `docs/`: design notes and job list.

## License

[MIT](LICENSE). Contributions welcome.

Win12 is an independent project, not affiliated with or endorsed by Stardock or Microsoft. Windows and
Start11 are trademarks of their owners.

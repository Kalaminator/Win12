#!/bin/sh
# Install or update the Win12 widget for the current user, then reload Plasma.
set -e
cd "$(dirname "$0")"
# Developer checks run only when node is installed.
if command -v node >/dev/null 2>&1; then node tests/test_tiles.js && node tests/test_islands.js && node tests/test_settings_keys.js --strict; fi
if command -v makepkg >/dev/null 2>&1; then sh tests/test_explorer_pkgbuild.sh; fi
# Translations: po/<lang>.po -> contents/locale/<lang>/LC_MESSAGES/plasma_applet_<id>.mo (Plasma looks there);
# the compiled .mo files are shipped, so gettext is only needed to rebuild them.
if command -v msgfmt >/dev/null 2>&1; then
    for po in po/*.po; do
        lang=$(basename "$po" .po)
        mkdir -p "package/contents/locale/$lang/LC_MESSAGES"
        msgfmt --check -o "package/contents/locale/$lang/LC_MESSAGES/plasma_applet_org.win12.menu.mo" "$po"
    done
fi
if kpackagetool6 -t Plasma/Applet -l | grep -qx org.win12.menu; then
    kpackagetool6 -t Plasma/Applet -u package
else
    kpackagetool6 -t Plasma/Applet -i package
fi
# Win12 Explorer (ribbon file manager): an Arch package built with makepkg.
if command -v pacman >/dev/null 2>&1; then sh explorer/build.sh || echo "Win12 Explorer: build failed (see above); the rest of Win12 is installed"; else echo "Win12 Explorer: skipped (needs Arch makepkg; the Windows settings page says so)"; fi
# Generated themes + "Win12 Settings" command; the widget runs the same script on start (store installs).
sh package/contents/ui/code/setup.sh --force

# One-time Start12 -> Win12 migration (plugin id, marker keys, generated theme names). Idempotent: does nothing
# once no old name is left. Runs after setup.sh so the win12-* themes already exist when the old ones go.
C="${XDG_CONFIG_HOME:-$HOME/.config}"; D="${XDG_DATA_HOME:-$HOME/.local/share}"
A="$C/plasma-org.kde.plasma.desktop-appletsrc"
if grep -qs 'start12' "$A" "$C/kwinrc" "$C/kdeglobals" "$C/plasmarc" "$C/plasmashellrc" \
    || [ -d "$D/plasma/plasmoids/org.start12.menu" ]; then
    BK="$C/win12-migration-backup-$(date +%Y%m%d-%H%M%S)"
    mkdir -p "$BK"
    for f in "$A" "$C/plasmashellrc" "$C/kwinrc" "$C/kdeglobals" "$C/plasmarc"; do [ -f "$f" ] && cp -p "$f" "$BK/"; done
    echo "Migrating Start12 -> Win12 (backup in $BK)"
    systemctl --user stop plasma-plasmashell.service
    # Only our names: the plugin id, panel/applet marker keys, taskbar pins and values naming generated themes
    # (a path like .../Start12-assets/... is left alone).
    [ -f "$A" ] && sed -i -E -e 's/org\.start12\.menu/org.win12.menu/g' \
        -e 's/^start12(Saved|Fill|Centre|Island|Layout|PanelOrder|Gone)/win12\1/' \
        -e 's/start12-pin-/win12-pin-/g' -e 's/(=|__aurorae__svg__)start12-/\1win12-/g' "$A"
    for f in "$C/kwinrc" "$C/kdeglobals" "$C/plasmarc" "$C/plasmashellrc"; do
        [ -f "$f" ] && sed -i -E 's/start12panelpop/win12panelpop/g; s/__aurorae__svg__start12-/__aurorae__svg__win12-/g; s/=start12-/=win12-/g' "$f"
    done
    for f in "$D"/applications/start12-pin-*.desktop; do [ -f "$f" ] && mv "$f" "$(dirname "$f")/win12-pin-${f##*/start12-pin-}"; done
    kpackagetool6 -t Plasma/Applet -r org.start12.menu >/dev/null 2>&1 || rm -rf "$D/plasma/plasmoids/org.start12.menu"
    rm -rf "$D"/plasma/desktoptheme/start12-* "$D"/aurorae/themes/start12-* "$D"/color-schemes/start12-*.colors \
        "$D/kwin/effects/start12panelpop" "$D/start12" "$D/applications/start12-settings.desktop" \
        "$HOME/.local/bin/start12-settings" "$HOME"/.cache/plasma_theme_start12-*.kcache
    Q="$(command -v qdbus6 || command -v qdbus-qt6 || echo qdbus)"
    "$Q" org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect start12panelpop >/dev/null 2>&1 || true
    "$Q" org.kde.KWin /KWin reconfigure >/dev/null 2>&1 || true
    systemctl --user start plasma-plasmashell.service
    MIGRATED=1
fi
if [ "$1" = "--restart" ] && [ -z "$MIGRATED" ]; then
    systemctl --user restart plasma-plasmashell.service
fi

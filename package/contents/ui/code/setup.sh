#!/bin/sh
# One-time setup the widget runs on every start (also run by install.sh): generated Plasma Styles,
# window decorations and colour schemes (xptheme.py) plus the "Win12 Settings" command and menu entry.
# A KDE Store / Discover install only unpacks the package, so this is how those files get made.
# Quick no-op unless the package version changed or a generated file is missing; --force always runs.
cd "$(dirname "$0")" || exit 1
D="${XDG_DATA_HOME:-$HOME/.local/share}"
B="$HOME/.local/bin"
STAMP="$D/win12/setup-version"
VER=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["KPlugin"]["Version"])' ../../../metadata.json)
if [ "$1" != "--force" ] && [ "$(cat "$STAMP" 2>/dev/null)" = "$VER" ] \
    && [ -d "$D/plasma/desktoptheme/win12-xp-classic" ] && [ -d "$D/aurorae/themes/win12-xp-classic" ] \
    && [ -f "$D/color-schemes/win12-xp-classic.colors" ] && [ -x "$B/win12-settings" ] && [ -d "$D/kwin/effects/win12panelpop" ] \
    && [ -d "$D/kwin/scripts/win12islandreveal" ] && [ -d "$D/plasma/plasmoids/org.win12.reveal" ] \
    && [ -d "$D/plasma/plasmoids/org.win12.showdesktop" ]; then
    exit 0
fi
set -e
python3 xptheme.py "$D/plasma/desktoptheme"
python3 xptheme.py --deco "$D/aurorae/themes"
python3 xptheme.py --scheme "$D/color-schemes"
install -Dm755 ../../extras/win12-settings "$B/win12-settings"
install -Dm644 ../../extras/win12-settings.desktop "$D/applications/win12-settings.desktop"
# The effect ships as metadata.json.in: a metadata.json inside the plasmoid makes Plasma scan it as an applet.
# W16: same for the island reveal KWin script and its 0 px panel helper widget, and the show desktop strip.
for X in win12-panelpop:KWin/Effect win12-islandreveal:KWin/Script win12-reveal:Plasma/Applet win12-showdesktop:Plasma/Applet; do
    FX=$(mktemp -d) && cp -r "../../extras/${X%%:*}/." "$FX" && mv "$FX/metadata.json.in" "$FX/metadata.json"
    kpackagetool6 -t "${X#*:}" -u "$FX" >/dev/null 2>&1 || kpackagetool6 -t "${X#*:}" -i "$FX" >/dev/null
    rm -rf "$FX"
done
mkdir -p "$D/win12"
echo "$VER" > "$STAMP"

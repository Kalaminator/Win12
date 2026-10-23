#!/bin/sh
# win12-explorer-default on/off/status against fake xdg-mime, kwriteconfig6 and busctl (never touches the real home).
set -e
SCRIPT="$(cd "$(dirname "$0")/.." && pwd)/explorer/win12-explorer-default"
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
mkdir "$T/bin"
cat > "$T/bin/xdg-mime" <<'F'
#!/bin/sh
if [ "$1" = query ]; then cat "$T/mime"; else printf '%s' "$2" > "$T/mime"; fi
F
printf '#!/bin/sh\necho "$@" >> "$T/kw.log"\n' > "$T/bin/kwriteconfig6"
printf '#!/bin/sh\necho "$@" >> "$T/bus.log"\n' > "$T/bin/busctl"
chmod +x "$T/bin/"*
export T HOME="$T" XDG_CONFIG_HOME="$T/c" XDG_DATA_HOME="$T/d" PATH="$T/bin:$PATH"
S="$T/c/win12-explorer/saved-default"
SVC="$T/d/dbus-1/services/org.win12.explorer.FileManager1.service"
eq() { [ "$(cat "$1")" = "$2" ] || { echo "FAIL: $1 = '$(cat "$1")', want '$2'"; exit 1; }; }
: > "$T/kw.log"; : > "$T/bus.log"
printf '%s' org.kde.dolphin.desktop > "$T/mime"

sh "$SCRIPT" on >/dev/null
eq "$S" org.kde.dolphin.desktop
eq "$T/mime" org.win12.explorer.desktop
grep -q 'Name=org.freedesktop.FileManager1' "$SVC"
grep -q 'Exec=/usr/bin/win12-explorer --daemon' "$SVC"
[ "$(sh "$SCRIPT" status)" = on ]

sh "$SCRIPT" on >/dev/null
eq "$S" org.kde.dolphin.desktop
eq "$T/mime" org.win12.explorer.desktop

sh "$SCRIPT" off >/dev/null
eq "$T/mime" org.kde.dolphin.desktop
[ ! -e "$S" ] && [ ! -e "$SVC" ]
grep -q ReleaseFileManager1 "$T/bus.log"
[ "$(sh "$SCRIPT" status)" = off ]
sh "$SCRIPT" off >/dev/null
[ ! -s "$T/kw.log" ]

: > "$T/mime"
sh "$SCRIPT" on >/dev/null
eq "$S" unset
sh "$SCRIPT" off >/dev/null
grep -q -- '--group Default Applications --key inode/directory --delete' "$T/kw.log"
eq "$T/mime" org.win12.explorer.desktop

rc=0; sh "$SCRIPT" bogus >/dev/null 2>&1 || rc=$?
[ "$rc" = 2 ]
echo "test_explorer_default OK"

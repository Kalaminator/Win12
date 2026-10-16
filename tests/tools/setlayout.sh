#!/bin/sh
# usage: setlayout.sh <single|split-left|split-right|islands> [pos 0|1|2] [look]
# Sets the Win12 taskbar layout (+ Start position, + taskbarLook) through plasmashell scripting and prints how long
# the sync took: from the write until no sync loop holds the panel lock any more (goal < 1500 ms).
L=${XDG_RUNTIME_DIR:-/tmp}/win12-panel.lock
t0=$(date +%s%N)
qdbus6 org.kde.plasmashell /PlasmaShell evaluateScript "for (const p of panels()) for (const w of p.widgets('org.win12.menu')) {
 w.currentConfigGroup = ['XP']; w.writeConfig('taskbarLayout', '$1'); ${3:+w.writeConfig('taskbarLook', '$3');}
 ${2:+w.currentConfigGroup = ['Style']; w.writeConfig('menuPosition', $2);} w.reloadConfig(); }" >/dev/null
i=0; while flock -n "$L" true && [ $i -lt 40 ]; do sleep 0.05; i=$((i + 1)); done   # wait for the sync to start (max 2 s)
flock "$L" true
echo "took $(( ($(date +%s%N) - t0) / 1000000 )) ms"

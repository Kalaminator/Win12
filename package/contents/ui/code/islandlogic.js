// Pure helpers for code/islands.js (main.qml syncIslands puts this file in front of it) and tests/test_islands.js.
// No .pragma library, no exports: plain functions, read by the plasmashell script engine and by node.
// roles(pos): pos 0 Left, 1 Center, 2 Right (Start position). The host panel holds Start (and the win12Layout key);
// A = the other app island, T = the tray island. Table in the spec (5.2): Center puts Start with the open apps.
function roles(pos) {
    return { host: pos === 1 ? "open" : "pinned", other: pos === 1 ? "pinned" : "open",
             row: pos === 2 ? ["T", "A", "host"] : pos === 1 ? ["A", "host", "T"] : ["host", "A", "T"] };
}
// The tray island's widget order: its task manager on the side away from the screen edge, then weather, then the
// widgets moved out of the host (tray, clock, show desktop) in the user's order; Right = full mirror.
function trayOrder(pos, tm, weather, moved) {
    return pos === 2 ? moved.slice().reverse().concat([weather, tm]) : [tm, weather].concat(moved);
}
// Launcher lists (comma-joined, as KConfig stores them) for [host TM, other TM]: first entry into islands = the
// host's pins go to whichever is 'pinned'; a role change swaps the lists so pinned apps stay in the pinned island.
function listsFor(cur, wantHost, hostList, otherList) {
    if (!cur) return wantHost === "pinned" ? [hostList, ""] : ["", hostList];
    return cur === wantHost ? [hostList, otherList] : [otherList, hostList];
}
// Leaving islands: one list again for the original task manager, pinned island first.
function mergeLists(pinned, open, tray) {
    return [pinned, open, tray].map(s => String(s || "")).filter(s => s).join(",");
}

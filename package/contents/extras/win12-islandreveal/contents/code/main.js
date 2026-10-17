// W16/10: Plasma reveals each auto-hidden panel only from the edge strip under it. With split Windows 12 islands this
// script shows them together, per monitor: the pointer on the bottom pixel row of a monitor sets "reveal" on the
// org.win12.reveal helpers of that monitor's panels (islands.js puts one in each island; NeedsAttention keeps an
// auto-hidden panel shown) and clears the others; DELAY ms with the pointer above the islands' top - MARGIN, or on
// another monitor, clears it and Plasma hides them as usual. Loaded by main.qml syncIslandReveal().
const MARGIN = 16, DELAY = 700;
let on;   // connector name of the revealed monitor, null = none; undefined at load so the first reveal(null) runs
function reveal(name) {
    if (on === name) return;
    on = name;
    callDBus("org.kde.plasmashell", "/PlasmaShell", "org.kde.PlasmaShell", "evaluateScript",
        "const sid = " + (name === null ? "-1" : "screenForConnector('" + name.replace(/[^A-Za-z0-9_-]/g, "") + "')")
        + "; for (const p of panels()) for (const w of p.widgets('org.win12.reveal')) { w.currentConfigGroup = ['General']; w.writeConfig('reveal', p.screen === sid); }");
}
const timer = new QTimer();
timer.singleShot = true;
timer.interval = DELAY;
timer.timeout.connect(() => reveal(null));
workspace.cursorPosChanged.connect(() => {
    // the pointer's monitor (not workspace.activeScreen = the focused window's)
    const c = workspace.cursorPos, out = workspace.screenAt(c) || workspace.activeScreen, name = out.name;
    const s = workspace.clientArea(KWin.FullScreenArea, out, workspace.currentDesktop), bottom = s.y + s.height;
    if (c.y >= bottom - 1 && !(workspace.activeWindow && workspace.activeWindow.fullScreen)) { timer.stop(); reveal(name); return; }
    if (on === null) return;
    // horizontal docks of this monitor only: a full-height side panel also reaches the bottom and would put top at 0 (never hide)
    const top = Math.min(...workspace.stackingOrder.filter(w => w.dock && w.output && w.output.name === name && w.frameGeometry.width > w.frameGeometry.height
        && w.frameGeometry.y + w.frameGeometry.height >= bottom - 1).map(w => w.frameGeometry.y));
    if (on !== name || c.y < top - MARGIN) { if (!timer.active) timer.start(); } else timer.stop();
});
reveal(null);   // a helper left "on" by a crash

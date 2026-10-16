// W16: Plasma reveals each auto-hidden panel only from the edge strip under it. With split Windows 12 islands this
// script shows them all: the pointer on the bottom pixel row sets "reveal" on every org.win12.reveal helper (islands.js
// puts one in each split panel; NeedsAttention keeps an auto-hidden panel shown); DELAY ms with the pointer above
// the islands' top - MARGIN clears it and Plasma hides them as usual. Loaded by main.qml syncIslandReveal().
const MARGIN = 16, DELAY = 700;
let on = true;
function reveal(v) {
    if (on === v) return;
    on = v;
    callDBus("org.kde.plasmashell", "/PlasmaShell", "org.kde.PlasmaShell", "evaluateScript",
        "for (const p of panels()) for (const w of p.widgets('org.win12.reveal')) { w.currentConfigGroup = ['General']; w.writeConfig('reveal', " + v + "); }");
}
const timer = new QTimer();
timer.singleShot = true;
timer.interval = DELAY;
timer.timeout.connect(() => reveal(false));
workspace.cursorPosChanged.connect(() => {
    const c = workspace.cursorPos, s = workspace.clientArea(KWin.FullScreenArea, workspace.activeScreen, workspace.currentDesktop), bottom = s.y + s.height;
    if (c.y >= bottom - 1 && !(workspace.activeWindow && workspace.activeWindow.fullScreen)) { timer.stop(); reveal(true); return; }
    if (!on) return;
    // horizontal docks only: a full-height side panel also reaches the bottom and would put top at 0 (never hide)
    const top = Math.min(...workspace.stackingOrder.filter(w => w.dock && w.frameGeometry.width > w.frameGeometry.height
        && w.frameGeometry.y + w.frameGeometry.height >= bottom - 1).map(w => w.frameGeometry.y));
    if (c.y < top - MARGIN) { if (!timer.active) timer.start(); } else timer.stop();
});
reveal(false);   // a helper left "on" by a crash

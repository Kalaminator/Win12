// Run: node tests/test_islands.js
// 1) pure helpers in code/islandlogic.js; 2) code/islands.js against a fake plasmashell scripting API (enough of
// Panel/Widget for the script: ids never reused, index = live position, a removed widget lingers with empty config).
const fs = require("fs"), path = require("path"), assert = require("assert");
const read = f => fs.readFileSync(path.join(__dirname, "../package/contents/ui/code/" + f), "utf8");
const L = new Function(read("islandlogic.js") + "; return { roles, trayOrder, listsFor, mergeLists };")();

// roles: host (holds Start) is pinned at Left/Right, open at Center; row order left -> right (spec 5.2)
assert.deepStrictEqual(L.roles(0), { host: "pinned", other: "open", row: ["host", "A", "T"] });
assert.deepStrictEqual(L.roles(1), { host: "open", other: "pinned", row: ["A", "host", "T"] });
assert.deepStrictEqual(L.roles(2), { host: "pinned", other: "open", row: ["T", "A", "host"] });
// tray island order: apps TM on the side away from the screen edge; Right = full mirror
assert.deepStrictEqual(L.trayOrder(0, 9, 8, [1, 2, 3]), [9, 8, 1, 2, 3]);
assert.deepStrictEqual(L.trayOrder(2, 9, 8, [1, 2, 3]), [3, 2, 1, 8, 9]);
// launchers: first entry -> the pinned island gets the host's list; role swap -> lists swap; no change -> same
assert.deepStrictEqual(L.listsFor("", "pinned", "a,b", ""), ["a,b", ""]);
assert.deepStrictEqual(L.listsFor("", "open", "a,b", ""), ["", "a,b"]);
assert.deepStrictEqual(L.listsFor("pinned", "open", "a,b", "c"), ["c", "a,b"]);
assert.deepStrictEqual(L.listsFor("open", "open", "c", "a,b"), ["c", "a,b"]);
assert.strictEqual(L.mergeLists("a,b", "", "c"), "a,b,c");
assert.strictEqual(L.mergeLists("", "", ""), "");

// ---- fake plasmashell ----
function shell(screenW = 1920) {
    let nextId = 1; const panels = [], reg = {};
    const cfg = o => { o.cfg = {}; o.currentConfigGroup = ["General"];
        o.readConfig = function (k, d = "") { const g = this.cfg[this.currentConfigGroup[0]] || {}; return k in g ? g[k] : d; };
        // like the real scripting API: a string written to the StringList "launchers" reaches QML as ONE launcher
        o.writeConfig = function (k, v) { assert.ok(k !== "launchers" || Array.isArray(v), "launchers written as a string: " + v); (this.cfg[this.currentConfigGroup[0]] = this.cfg[this.currentConfigGroup[0]] || {})[k] = v; };
        // the patched task manager files its launchers in the island registry under the island it has at that moment:
        // a reload loads launchers before win12Island (each change seen at once), like the live KConfigPropertyMap
        o.reloadConfig = function () { const g = this.cfg.General || {}, s = this.seen = this.seen || {};
            for (const k of ["launchers", "win12Island"]) if (String(g[k] || "") !== String(s[k] || "")) { s[k] = g[k];
                if (s.win12Island) for (const l of s.launchers || []) reg[l] = s.win12Island; } };
        return o; };
    const widget = (type, panel) => cfg({ id: nextId++, type, panel,
        get configKeys() { return Object.keys(this.cfg.General || {}); },
        get index() { return this.panel ? this.panel.ws.indexOf(this) : -1; },
        set index(i) { const ws = this.panel.ws; if (ws.indexOf(this) < 0) return; ws.splice(ws.indexOf(this), 1); ws.splice(Math.min(i, ws.length), 0, this); },
        remove() { this.panel.ws.splice(this.panel.ws.indexOf(this), 1); this.panel.gone.push(this); this.cfg = {}; this.panel = null; } });
    class Panel {
        constructor() { cfg(this); this.id = nextId++; this.ws = []; this.gone = []; this.lost = []; this.orphan = new Set(); this.location = "bottom"; this.height = 44;
            this.lengthMode = "fill"; this.alignment = "left"; this.minimumLength = 0; this.maximumLength = 0; this.offset = 0;
            this.floating = true; this.hiding = "none"; this.screen = 0; panels.push(this); }
        get length() { return this.lengthMode === "fill" ? 1920 : this.ws.length * 40 + 16; }
        widgets(type) { const all = this.ws.concat(this.lost, this.gone); return type ? all.filter(w => w.type === type && w.panel === this) : all; }
        get widgetIds() { return this.widgets().map(w => w.id); }
        addWidget(x) { if (typeof x === "string") { const w = widget(x, this); this.ws.push(w); return w; }
            // orphan: Plasma's insert out of range (old x past the target's widgets): in the panel, not in the layout
            const o = x.panel, i = o.ws.indexOf(x); if (i >= 0) o.ws.splice(i, 1); else o.lost.splice(o.lost.indexOf(x), 1);
            x.panel = this; (this.orphan.delete(x.id) ? this.lost : this.ws).push(x); return x; }
        remove() { panels.splice(panels.indexOf(this), 1); for (const w of this.ws) w.panel = null; }
    }
    const sh = { Panel, panels: () => panels.slice(), screenGeometry: () => ({ x: 0, y: 0, width: screenW, height: 1080 }), out: [], print: s => sh.out.push(String(s)) };
    // the user's bar: Start, task manager with two pins, separator, tray, clock, show desktop
    const p = new Panel(); const me = p.addWidget("org.win12.menu"); const tm = p.addWidget("org.kde.plasma.icontasks");
    tm.writeConfig("launchers", ["a.desktop", "b.desktop"]);
    for (const t of ["marginsseparator", "systemtray", "digitalclock", "showdesktop"]) p.addWidget("org.kde.plasma." + t);
    sh.host = p; sh.me = me; sh.tm = tm; sh.reg = reg; return sh;
}
function runIslands(sh, subs) {
    // Widget.remove() is applet->destroy() -> deleteLater: gone by the next evaluateScript call. Only a removed
    // system tray stays listed across calls (seen live, W16; islands.js filters it through win12Gone).
    for (const q of sh.panels()) q.gone = q.gone.filter(w => /systemtray/.test(w.type));
    let src = read("islandlogic.js") + "\n" + read("islands.js");
    for (const k in subs) src = src.split("@" + k + "@").join(String(subs[k]));
    sh.out = []; new Function("panels", "Panel", "screenGeometry", "print", src)(sh.panels, sh.Panel, sh.screenGeometry, sh.print);
    return sh.out[0];
}
// passes until the script prints done (Infinity = never, so "<= n" asserts fail on a loop that never ends)
function passes(sh, subs, max = 10) { for (let i = 1; i <= max; i++) if (runIslands(sh, subs) === "done") return i; return Infinity; }
const types = q => q.widgets().filter(w => w.panel === q).map(w => w.type.replace("org.kde.plasma.", "").replace("org.win12.", "w12:"));
const sub = (sh, WANT, POS = 0, MAG = true) => ({ ID: sh.me.id, WANT, POS, MAG, GAP: 8 });

// split-left today (one step per pass, so up to 40 passes): 3 panels, tray/clock/show desktop in the right island
// after the fresh tray, helpers everywhere
let sh = shell(); assert.ok(passes(sh, sub(sh, "split-left"), 40) <= 40, "split-left converges");
assert.strictEqual(sh.panels().length, 3);
const R = sh.panels().find(q => q.readConfig("win12Island") === "R");
assert.deepStrictEqual(types(R).filter(t => t !== "w12:reveal"), ["systemtray", "digitalclock", "showdesktop"]);
// back to single: one panel, the order as before (the tray is a fresh id, same place)
assert.ok(passes(sh, sub(sh, "single"), 40) <= 40, "single converges"); assert.strictEqual(sh.panels().length, 1);
assert.deepStrictEqual(types(sh.host), ["w12:menu", "icontasks", "marginsseparator", "systemtray", "digitalclock", "showdesktop"]);

// ---- fast engine + islands (patched KDE): a switch converges in <= 3 passes ----
const isl = (sh, k) => sh.panels().find(q => q.readConfig("win12Island") === k);
const cfgOf = (w, k) => { w.currentConfigGroup = ["General"]; return String(w.readConfig(k, "")); };
sh = shell();
assert.ok(passes(sh, sub(sh, "islands", 0)) <= 3, "islands Left converges fast");
assert.strictEqual(sh.panels().length, 3);
let A = isl(sh, "A"), T = isl(sh, "T");
assert.deepStrictEqual(types(sh.host).filter(t => t !== "w12:reveal"), ["w12:menu", "icontasks", "marginsseparator"]);
assert.deepStrictEqual(types(A).filter(t => t !== "w12:reveal"), ["icontasks"]);
assert.deepStrictEqual(types(T).filter(t => t !== "w12:reveal"), ["icontasks", "weather", "systemtray", "digitalclock", "showdesktop"]);
const tmA = A.widgets("org.kde.plasma.icontasks")[0], tmT = T.widgets("org.kde.plasma.icontasks")[0];
assert.strictEqual(cfgOf(sh.tm, "win12Island"), "pinned"); assert.strictEqual(cfgOf(sh.tm, "launchers"), "a.desktop,b.desktop");
assert.strictEqual(cfgOf(tmA, "win12Island"), "open"); assert.strictEqual(cfgOf(tmA, "launchers"), "");
assert.strictEqual(cfgOf(tmT, "win12Island"), "tray"); assert.ok("launchers" in tmT.cfg.General, "tray island TM has no explicit pins");
assert.strictEqual(cfgOf(sh.host, "win12TrayClean"), "true");   // the one-time fresh tray happened
assert.ok(sh.host.offset < A.offset && A.offset < T.offset, "magnetic row host, A, T");
// Center: roles and launchers swap, nothing moves between panels, row A, host, T
const before = [sh.host, A, T].map(q => q.widgetIds.join());
assert.ok(passes(sh, sub(sh, "islands", 1)) <= 3, "Center converges fast");
assert.deepStrictEqual([sh.host, A, T].map(q => q.widgetIds.join()), before);
assert.strictEqual(cfgOf(sh.tm, "win12Island"), "open"); assert.strictEqual(cfgOf(sh.tm, "launchers"), "");
assert.strictEqual(cfgOf(tmA, "win12Island"), "pinned"); assert.strictEqual(cfgOf(tmA, "launchers"), "a.desktop,b.desktop");
assert.ok(A.offset < sh.host.offset && sh.host.offset < T.offset, "magnetic row A, host, T");
// Right: full mirror, tray island reversed
assert.ok(passes(sh, sub(sh, "islands", 2)) <= 3, "Right converges fast");
assert.deepStrictEqual(types(T).filter(t => t !== "w12:reveal"), ["showdesktop", "digitalclock", "systemtray", "weather", "icontasks"]);
assert.strictEqual(cfgOf(sh.tm, "win12Island"), "pinned"); assert.strictEqual(cfgOf(sh.tm, "launchers"), "a.desktop,b.desktop");
assert.ok(T.offset < A.offset && A.offset < sh.host.offset, "magnetic row T, A, host");
// back to One bar: one panel, old order, the pins merged back, stock task manager again
tmT.writeConfig("launchers", ["z.desktop"]);   // a pin dropped into the tray island comes back too (pinned first)
assert.ok(passes(sh, sub(sh, "single")) <= 3, "single converges fast");
assert.strictEqual(sh.panels().length, 1);
assert.deepStrictEqual(types(sh.host), ["w12:menu", "icontasks", "marginsseparator", "systemtray", "digitalclock", "showdesktop"]);
assert.strictEqual(cfgOf(sh.tm, "win12Island"), ""); assert.strictEqual(cfgOf(sh.tm, "launchers"), "a.desktop,b.desktop,z.desktop");
// splits still converge fast, and islands -> split-left goes through the restore
sh = shell(); assert.ok(passes(sh, sub(sh, "split-left")) <= 3); assert.ok(passes(sh, sub(sh, "islands", 0)) <= 5); assert.ok(passes(sh, sub(sh, "split-left")) <= 6);
// Start at Right mirrors the One bar (syncCenterPanel). Live 2026-10-17: the switch moved the Start widget into the
// new island, which then split itself again every pass until plasmashell ran out of files and crashed.
for (const m of ["split-left", "islands"]) { sh = shell(); sh.host.ws.reverse();
    assert.ok(passes(sh, sub(sh, m, 0)) <= 3, m + " from a mirrored bar converges"); assert.strictEqual(sh.panels().length, 3);
    assert.strictEqual(sh.me.panel, sh.host, "Start stays in the host");
    assert.deepStrictEqual(types(isl(sh, m === "islands" ? "T" : "R")).filter(t => /tray|clock|desktop/.test(t)), ["systemtray", "digitalclock", "showdesktop"]); }
// the Start widget dragged into an island by hand: no new islands from there
sh = shell(); assert.ok(passes(sh, sub(sh, "split-left", 0)) <= 3); isl(sh, "R").addWidget(sh.me);
assert.strictEqual(runIslands(sh, sub(sh, "split-left", 0)), "done"); assert.strictEqual(sh.panels().length, 3);
// no task manager in the host (taken out for real, not a lingering removal): islands does nothing
sh = shell(); sh.host.ws.splice(sh.host.ws.indexOf(sh.tm), 1); sh.tm.panel = null;
assert.strictEqual(runIslands(sh, sub(sh, "islands", 0)), "done"); assert.strictEqual(sh.panels().length, 1);
// leaving islands from Center: the host TM is "open" when the merged pins arrive (live: the pins became =open)
sh = shell(); passes(sh, sub(sh, "islands", 0)); passes(sh, sub(sh, "islands", 1)); passes(sh, sub(sh, "islands", 0));
passes(sh, sub(sh, "islands", 1)); assert.ok(passes(sh, sub(sh, "single", 1)) <= 3);
assert.deepStrictEqual(sh.reg, { "a.desktop": "pinned", "b.desktop": "pinned" }, "registry after leaving islands from Center");
// layout to layout goes directly (live: restore + rebuild took 1.6-2.4 s): the panels are re-keyed, not rebuilt, and
// the one weather widget is kept
const weathers = sh => sh.panels().flatMap(q => q.widgets("org.kde.plasma.weather"));
sh = shell(); passes(sh, sub(sh, "split-left")); let wid = weathers(sh)[0].id;
for (const [m, from] of [["islands", "R"], ["split-right", "T"], ["islands", "R"], ["split-left", "T"], ["split-right", "R"], ["split-left", "R"]]) {
    const id = isl(sh, from).id; assert.ok(passes(sh, sub(sh, m, 2)) <= 3, m + " from a split converges fast");
    assert.strictEqual(isl(sh, m === "islands" ? "T" : "R").id, id, m + ": tray side panel kept");
    assert.strictEqual(sh.panels().length, m === "split-right" ? 2 : 3); assert.deepStrictEqual(weathers(sh).map(w => w.id), [wid], m + ": one weather, kept");
    assert.deepStrictEqual(types(isl(sh, m === "islands" ? "T" : "R")).filter(t => /tray|clock|desktop/.test(t)), m === "islands" ? ["showdesktop", "digitalclock", "systemtray"] : ["systemtray", "digitalclock", "showdesktop"]); }
assert.ok(passes(sh, sub(sh, "single")) <= 3); assert.strictEqual(sh.panels().length, 1);
assert.deepStrictEqual(types(sh.host), ["w12:menu", "icontasks", "marginsseparator", "systemtray", "digitalclock", "showdesktop"]);
assert.strictEqual(cfgOf(sh.tm, "win12Island"), ""); assert.strictEqual(cfgOf(sh.tm, "launchers"), "a.desktop,b.desktop");
// a mirrored bar (Start at Right; syncCenterPanel keeps the unmirrored order in win12PanelOrder) comes back unmirrored
sh = shell(); sh.host.currentConfigGroup = ["General"]; sh.host.writeConfig("win12PanelOrder", sh.host.widgetIds.join(";")); sh.host.ws.reverse();
passes(sh, sub(sh, "split-left", 2)); assert.ok(passes(sh, sub(sh, "single", 0)) <= 3);
assert.deepStrictEqual(types(sh.host), ["w12:menu", "icontasks", "marginsseparator", "systemtray", "digitalclock", "showdesktop"], "mirrored bar restored unmirrored");
// a widget moved home lands off the bar (Plasma insert out of range, index -1): it goes out and in again
sh = shell(); passes(sh, sub(sh, "islands", 2)); const desk = sh.panels().map(q => q.widgets("org.kde.plasma.showdesktop")[0]).find(w => w);
sh.host.orphan.add(desk.id); assert.ok(passes(sh, sub(sh, "single", 2)) <= 3);
assert.strictEqual(desk.index, 5, "orphaned widget back on the bar, last");
// back to One bar at Start=Right: main.qml's centerLoop mirrors the restored bar from win12PanelOrder. A saved order
// written while the host was an island lacked the moved widgets, so each pass flipped them (25 passes, wrong order).
const qml = fs.readFileSync(path.join(__dirname, "../package/contents/ui/main.qml"), "utf8"), c0 = qml.indexOf("function centerLoop() {");
const center = (sh, want) => { const js = new Function("Plasmoid", "islandWant", qml.slice(qml.indexOf("{", c0) + 1, qml.indexOf("const dbus", c0)) + "return js;")({ id: sh.me.id }, want);
    for (let i = 1; i <= 10; i++) { sh.out = []; new Function("panels", "print", js)(sh.panels, sh.print); if (sh.out[0] === "done") return i; } return Infinity; };
sh = shell(); sh.me.currentConfigGroup = ["Style"]; sh.me.writeConfig("centerTaskbar", "true"); sh.me.writeConfig("menuPosition", 2);
passes(sh, sub(sh, "islands", 2)); center(sh, "islands"); assert.ok(passes(sh, sub(sh, "single", 2)) <= 3);
assert.ok(center(sh, "single") <= 3, "Start position sync converges after the restore");
assert.deepStrictEqual(types(sh.host), ["showdesktop", "digitalclock", "systemtray", "marginsseparator", "panelspacer", "icontasks", "w12:menu"], "mirrored");
console.log("test_islands OK");

// Run: node tests/test_islands.js
// 1) pure helpers in code/islandlogic.js; 2) code/islands.js against a fake plasmashell scripting API (enough of
// Panel/Widget for the script: ids never reused, index = live position, a removed widget lingers with empty config).
const fs = require("fs"), path = require("path"), assert = require("assert");
const read = f => fs.readFileSync(path.join(__dirname, "../package/contents/ui/code/" + f), "utf8");
const L = new Function(read("islandlogic.js") + "; return { roles, trayOrder, listsFor, mergeLists };")();

// roles: host (holds Start) is pinned at Left/Right, open at Center; row order left -> right (spec 5.2)
assert.deepStrictEqual(L.roles(0), { host: "pinned", other: "open", row: ["host", "A", "T"] });
assert.deepStrictEqual(L.roles(1), { host: "open", other: "pinned", row: ["A", "host", "T"] });
assert.deepStrictEqual(L.roles(2), { host: "pinned", other: "open", row: ["T", "host", "A"] });
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
    let nextId = 1; const panels = [];
    const cfg = o => { o.cfg = {}; o.currentConfigGroup = ["General"];
        o.readConfig = function (k, d = "") { const g = this.cfg[this.currentConfigGroup[0]] || {}; return k in g ? g[k] : d; };
        o.writeConfig = function (k, v) { (this.cfg[this.currentConfigGroup[0]] = this.cfg[this.currentConfigGroup[0]] || {})[k] = v; };
        o.reloadConfig = () => {}; return o; };
    const widget = (type, panel) => cfg({ id: nextId++, type, panel,
        get configKeys() { return Object.keys(this.cfg.General || {}); },
        get index() { return this.panel ? this.panel.ws.indexOf(this) : -1; },
        set index(i) { const ws = this.panel.ws; ws.splice(ws.indexOf(this), 1); ws.splice(Math.min(i, ws.length), 0, this); },
        remove() { this.panel.ws.splice(this.panel.ws.indexOf(this), 1); this.panel.gone.push(this); this.cfg = {}; this.panel = null; } });
    class Panel {
        constructor() { cfg(this); this.id = nextId++; this.ws = []; this.gone = []; this.location = "bottom"; this.height = 44;
            this.lengthMode = "fill"; this.alignment = "left"; this.minimumLength = 0; this.maximumLength = 0; this.offset = 0;
            this.floating = true; this.hiding = "none"; this.screen = 0; panels.push(this); }
        get length() { return this.lengthMode === "fill" ? 1920 : this.ws.length * 40 + 16; }
        widgets(type) { const all = this.ws.concat(this.gone); return type ? all.filter(w => w.type === type && w.panel === this) : all; }
        get widgetIds() { return this.widgets().map(w => w.id); }
        addWidget(x) { if (typeof x === "string") { const w = widget(x, this); this.ws.push(w); return w; }
            x.panel.ws.splice(x.panel.ws.indexOf(x), 1); x.panel = this; this.ws.push(x); return x; }
        remove() { panels.splice(panels.indexOf(this), 1); for (const w of this.ws) w.panel = null; }
    }
    const sh = { Panel, panels: () => panels.slice(), screenGeometry: () => ({ x: 0, y: 0, width: screenW, height: 1080 }), out: [], print: s => sh.out.push(String(s)) };
    // the user's bar: Start, task manager with two pins, separator, tray, clock, show desktop
    const p = new Panel(); const me = p.addWidget("org.win12.menu"); const tm = p.addWidget("org.kde.plasma.icontasks");
    tm.writeConfig("launchers", "a.desktop,b.desktop");
    for (const t of ["marginsseparator", "systemtray", "digitalclock", "showdesktop"]) p.addWidget("org.kde.plasma." + t);
    sh.host = p; sh.me = me; sh.tm = tm; return sh;
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
// passes until the script prints done (-1 = never)
function passes(sh, subs, max = 10) { for (let i = 1; i <= max; i++) if (runIslands(sh, subs) === "done") return i; return -1; }
const types = q => q.widgets().filter(w => w.panel === q).map(w => w.type.replace("org.kde.plasma.", "").replace("org.win12.", "w12:"));
const sub = (sh, WANT, POS = 0, MAG = true) => ({ ID: sh.me.id, WANT, POS, MAG, GAP: 8 });

// split-left today (one step per pass, so up to 40 passes): 3 panels, tray/clock/show desktop in the right island
// after the fresh tray, helpers everywhere
let sh = shell(); assert.ok(passes(sh, sub(sh, "split-left"), 40) > 0, "split-left converges");
assert.strictEqual(sh.panels().length, 3);
const R = sh.panels().find(q => q.readConfig("win12Island") === "R");
assert.deepStrictEqual(types(R).filter(t => t !== "w12:reveal"), ["systemtray", "digitalclock", "showdesktop"]);
// back to single: one panel, the order as before (the tray is a fresh id, same place)
assert.ok(passes(sh, sub(sh, "single"), 40) > 0, "single converges"); assert.strictEqual(sh.panels().length, 1);
assert.deepStrictEqual(types(sh.host), ["w12:menu", "icontasks", "marginsseparator", "systemtray", "digitalclock", "showdesktop"]);
console.log("test_islands OK");

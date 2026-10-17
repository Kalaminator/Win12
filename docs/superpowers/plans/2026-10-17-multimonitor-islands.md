# Islands on extended multi-monitor setups — implementation plan (spec section 10)

> **For agentic workers:** this plan is executed by an unattended loop of fresh `claude -p` runs (see "Driver
> protocol"). Each run does ONE task. Inside a run use superpowers:executing-plans (checkbox steps below).

**Goal:** the Win12 islands live on a chosen monitor (default: the main one) or on every monitor (Start + app islands
copied per monitor, tray island on the main one), the show-together reveal acts per monitor, and a monitor plugged or
unplugged rebuilds the islands — with no new KDE patch.

**Architecture:** `islands.js` (one plasmashell script, run by `main.qml syncIslands()` until it prints `done`) gets
three new placeholders: `@SCR@` (the "Show islands on" value), `@ALL@` ("All apps on every monitor") and `@SCREENS@`
(the connected connector names from `Qt.application.screens`). It resolves names with the stock scripting call
`screenForConnector(name)`, moves panels with the stock writable `Panel.screen`, and for "All monitors" creates per
screen a copy of the host panel (a second `org.win12.menu` instance marked `win12Mirror` + a task manager) and, in
3 Islands, the other app island; copies' panels carry `win12Island = M`. The KWin reveal script measures the
pointer's output (`workspace.screenAt`) and reveals that monitor's islands only. The node fake-shell harness grows
2-3 fake screens; live tests check single-monitor regressions only.

**Tech stack:** KDE Plasma 6.8.0 (patched by `kde/`, Qt 6.12), plasmashell scripting API, KWin scripting API, QML,
node for the harness, gettext for es.po.

**Spec:** `docs/superpowers/specs/2026-10-16-fast-islands-design.md` section 10 (read it first every run; sections
5.1-5.2 for the island vocabulary). Parent plan: `docs/superpowers/plans/2026-10-16-fast-islands.md` (Task 6 = the
engine this plan extends).

**Base commit:** `e8a51c7`. Line numbers below are from that commit; other agents edit `islands.js`, `main.qml` and
`kde/patches/plasma-desktop` concurrently, so every task starts with `git log --oneline -3`, `git status --short` and
re-reads the lines it touches (grep first).

## No KDE change needed (evidence, read-only clones under `~/.cache/win12-kde/src/<pkg>` or the scratchpad `kde/`)

- A panel is put on a screen from a script by **assigning `panel.screen = <id>`**: plasma-workspace
  `shell/scripting/containment.h:39` `Q_PROPERTY(int screen READ screen WRITE setScreen)` (Panel repeats it,
  `panel.h:30`); `containment.cpp:79-85` `setScreen` → `ShellCorona::setScreenForContainment(containment, screen)`
  (`shellcorona.cpp:2624`): for a panel it calls `containment->setScreen(id)` and either `panelView->setScreenToFollow`
  or, for a panel without a view yet (one created in the same script), `createWaitingPanels()`. A screen id that is
  not connected destroys the panel view (`:2657-2663`), so the script only writes ids that `screenForConnector`
  returned (>= 0).
- Screen ids: `screenForConnector(name)` (`scriptengine_v1.cpp:199`, exposed as a global, `scriptengine.cpp:188`)
  returns `ScreenPool::idForName` = the index in the connected-screens list, `-1` when not connected
  (`screenpool.cpp:38-44`); the primary screen is the list's first entry (`screenpool.cpp:55-61`), so **id 0 = main
  monitor**. `screenGeometry(id)` and `screenCount` are globals too (`scriptengine.cpp:108,127`).
- Task managers filter by screen with the **stock key `showOnlyCurrentScreen`** (plasma-desktop
  `applets/taskmanager/main.xml:9`, bound to `tasksModel.filterByScreen` in `applets/taskmanager/qml/main.qml:161`
  with `screenGeometry: Plasmoid.containment.screenGeometry` at `:157`).
- KWin 6.8.0 scripting has `workspace.screenAt(QPointF)`, `workspace.cursorPos`, `Window.output` and `Output.name`
  (symbols in `/usr/lib/libkwin.so.6`; Task 4 prints them live before relying on them).
- Pinned apps on every monitor and app moves on all monitors come for free: the island registry maps app -> island
  role (not per monitor), and a copy task manager with the same `win12Island` role and `launchers` shows the same.

## Global constraints

- Everything from the parent plan's "Global constraints" applies (Arch only, sudo = `/usr/bin/pacman` only, names
  `win12*`, short files, WHY comments, `// ponytail:` notes, `<= ~40 tool calls / ~100k tokens` per run, test output
  through `| tail`, live safety: back up `~/.config/plasma-org.kde.plasma.desktop-appletsrc`, `~/.config/plasmashellrc`
  and `~/.config/win12islandsrc` before any live test, restart only with `systemctl --user restart plasma-plasmashell`).
- Only ONE monitor is connected (HDMI-A-2, 1920x1080, plasmashell id 0). Never write a `screen` other than 0 live:
  a panel sent to a missing screen disappears until that screen appears. Multi-monitor logic is proven in
  `tests/test_islands.js` with 2-3 fake screens only.
- No task in this plan changes `kde/patches/`. If one turns out to be needed, the task ends `status: blocked`,
  `notes: needs <pkg> patch: <why>` and the plan is amended first.
- Connector names reach `islands.js` through `sed`: main.qml strips anything but `[A-Za-z0-9,_-]` from them.
- Config keys: `islandScreen` (String, `""` = main monitor, `"all"` = every monitor, else a connector name),
  `islandAllApps` (Bool) in the `XP` group next to `taskbarLayout`; `win12Mirror` (Int, 0 = a real Start) in `General`.
- Commits: one per task, message `<area>: <what>` + a short why, ending with the two attribution lines the driver
  prompt gives (when it gives none: the same two lines as this plan's commit).

## Driver protocol (progress log format)

File: `docs/superpowers/plans/2026-10-17-multimonitor-islands-progress.md`. Each run:

1. Reads the spec (section 10 + 5.1-5.2), this plan, and the progress log. The last entry's `next:` line names the
   task to do; `next-model:` names the model the driver must start for it.
2. Does that task's steps, verifies, commits, then appends ONE entry:

```
## <YYYY-MM-DD HH:MM> Task N: <title>
status: done | blocked | partial | skipped
done: <what landed, commit hash>
left: <what is not done, or "nothing">
notes: <facts the next run needs; timing numbers; files>
next: Task M — <title>            (or `next: none` when Task 6 is done)
next-model: opus | sonnet | none
```

3. Exits. The driver reads the LAST `next-model:` line to choose the model for the next run.

Rules: `status: blocked` is for tasks that need something unavailable (sudo pacman, a package that is not installed
yet, a second monitor). A blocked task's dependants (see each task's **Depends on**) are `skipped` with
`left: blocked by Task N`; `next:` then points at the next task that does not depend on it. `partial` = out of
budget mid-task: the entry's `next:` names the SAME task and `notes:` says the exact step to resume at (work is
committed at a safe point first, or stashed with `git stash push -m "Task N partial"` and noted). A model name in a
task header (`Model: opus|sonnet`) is what the previous entry's `next-model:` must carry.

## Task order

| # | Model | Task | Depends on |
|---|---|---|---|
| 1 | opus | Keys + `@SCR@/@ALL@/@SCREENS@` wiring; one chosen monitor in `islands.js`; harness screens | - |
| 2 | opus | All monitors: host + app-island copies in `islands.js`; `isMirror` guards in `main.qml` | 1 |
| 3 | sonnet | Settings on the Taskbar page, Spanish, README | 1 |
| 4 | opus | KWin reveal script: the pointer's monitor only (+ live API check) | - |
| 5 | opus | Live single-monitor regression | 1, 2, 3, 4 |
| 6 | opus | Whole-change review + ELI5 | all |

No task changes a KDE package.

## Review focus

Inputs the spec implies but names no test for; each has its test in the owning task:

1. The chosen monitor is unplugged (or its name never matches `screenForConnector`): islands fall back to the main
   monitor instead of vanishing (Task 1 harness case "unplugged name"; Task 5 prints `screenForConnector('HDMI-A-2')`
   next to `Qt.application.screens` names to prove the two name sources agree).
2. A Start copy must never run the engine or the centre-spacer sync on its own panel, even though it is a full
   `org.win12.menu` instance with the host's settings (Task 2 harness case "copy id prints done"; `isMirror` guards).
3. A monitor removed while "All monitors" is on: its copies are deleted (not left as hidden panels on a dead id);
   added back: they return (Task 2 harness case "unplug/replug").
4. The settings copy to the mirrors must never keep the script printing `again` (a copy is complete in one pass)
   (Task 2 harness case "settings change reaches the copy, script still done").
5. The pointer on the bottom edge of monitor B must reveal B's islands and clear A's; leaving the edge for another
   monitor clears after DELAY (Task 4: no harness for KWin; code path reviewed in Task 6, single-monitor behaviour
   checked live in Task 5).

## Defaults picked without asking (the user should know)

- `islandScreen = ""` means "follow the main (primary) monitor" and is shown as `<name> (main)` in the list; picking
  the main monitor's own name is also accepted and pins it by name.
- "Open apps on other monitors" only sets `showOnlyCurrentScreen` on the COPIES' task managers. The main monitor's
  task managers stay stock (every window) = Windows' "Main taskbar and taskbar where window is open".
- "All monitors" applies to the split layouts too (a host copy: Start + task manager; no extra islands), not only to
  3 Islands.
- Pins are one-way: the copies' launcher lists are overwritten from the main monitor's task managers every pass.
  Pin from the main monitor (a pin made on a copy is undone within ~1 s). Noted in the settings help text.
- The copy Start gets the host's whole configuration (all groups) copied on every engine pass, and the host re-runs
  the engine (1 s debounce) on any settings change while "All monitors" is on. The copy's own settings dialog works
  but its edits are overwritten by the next copy.
- `s.mon` (the copies' panel/widget ids) is keyed by connector name, not screen id: plasmashell ids shift when a
  screen is unplugged.

---

### Task 1: Keys + `@SCR@/@ALL@/@SCREENS@` wiring; one chosen monitor in `islands.js`; harness screens

Model: opus
Depends on: -

**Files:**
- Modify: `package/contents/config/main.xml` (after the `taskbarMagnetic` entry, ~384-387, group `XP`; and in group
  `General`, ~687+), `package/contents/ui/main.qml` (`syncIslands` ~702-713: three more `sed -e`; new properties after
  `islandPos` ~666-667), `package/contents/ui/code/islands.js` (line 16 constants; `s` creation ~80; restore branch
  ~55-65; placement ~113-118), `tests/test_islands.js` (`shell()`, `runIslands`, `sub`, new cases).

**Interfaces:**
- Consumes: scripting globals `screenForConnector(name)` -> int (-1 = not connected), `screenGeometry(id)`, writable
  `Panel.screen`; existing `islands.js` state `s` (host key `win12Layout` JSON).
- Produces: placeholders `@SCR@` (string: `''` | `'all'` | connector name), `@ALL@` (`true`|`false`), `@SCREENS@`
  (comma-joined connector names); `islands.js` constants `SCR`, `ALLAPPS`, `NAMES`, `sid(name)`, `MAIN`, `ALLMON`;
  `s.scr` (the host's screen before the islands); harness `shell(screenW, screens)`, `sh.screens`,
  `sh.screenForConnector`, `Panel.screen`, `sub(sh, WANT, POS, MAG, SCR, ALL)`. Task 2 builds on all of these.

- [ ] **Step 1: harness screens** — in `tests/test_islands.js`:

Replace `function shell(screenW = 1920) {` with `function shell(screenW = 1920, screens = ["HDMI-A-2"]) {`.
In `class Panel`'s constructor the line already has `this.screen = 0;` (keep). Replace the `sh` object line with:

```js
    // 10: screens like plasmashell's ScreenPool: id = index in the connected list, 0 = primary, -1 = not connected
    const sh = { Panel, panels: () => panels.slice(), screens, screenForConnector: n => screens.indexOf(n),
        screenGeometry: id => ({ x: screenW * id, y: 0, width: screenW, height: 1080 }), out: [], print: s => sh.out.push(String(s)) };
```

In `runIslands` replace the `new Function(...)` line with:

```js
    sh.out = []; new Function("panels", "Panel", "screenGeometry", "screenForConnector", "print", src)(sh.panels, sh.Panel, sh.screenGeometry, sh.screenForConnector, sh.print);
```

Replace the `sub` line with:

```js
const sub = (sh, WANT, POS = 0, MAG = true, SCR = "", ALL = false) => ({ ID: sh.me.id, WANT, POS, MAG, GAP: 8, SCR, ALL, SCREENS: sh.screens.join(",") });
```

- [ ] **Step 2: failing cases** — before the final `console.log("test_islands OK")` add:

```js
// ---- spec 10: Show islands on (one monitor) ----
// the chosen monitor by connector name: host and islands move there; One bar puts the host back where it was
sh = shell(1920, ["HDMI-A-2", "DP-1"]);
assert.ok(passes(sh, sub(sh, "islands", 0, true, "DP-1")) <= 4, "islands on DP-1 converge");
assert.deepStrictEqual(sh.panels().map(q => q.screen), [1, 1, 1], "host, A and T on screen 1");
assert.ok(passes(sh, sub(sh, "single", 0)) <= 3); assert.strictEqual(sh.host.screen, 0, "host back on screen 0");
// default (empty) = the main monitor (id 0): a host that lives on another screen moves to it and comes back
sh = shell(1920, ["HDMI-A-2", "DP-1"]); sh.host.screen = 1;
assert.ok(passes(sh, sub(sh, "split-left", 0, true, "")) <= 4); assert.deepStrictEqual(sh.panels().map(q => q.screen), [0, 0, 0]);
assert.ok(passes(sh, sub(sh, "single", 0)) <= 3); assert.strictEqual(sh.host.screen, 1, "host back on screen 1");
// an unplugged (or unknown) name falls back to the main monitor; a saved home screen that is gone is not restored to
sh = shell(1920, ["HDMI-A-2", "DP-1"]); sh.host.screen = 1;
assert.ok(passes(sh, sub(sh, "islands", 0, true, "DP-9")) <= 4); assert.deepStrictEqual(sh.panels().map(q => q.screen), [0, 0, 0]);
sh.screens.pop(); assert.ok(passes(sh, sub(sh, "single", 0)) <= 3); assert.strictEqual(sh.host.screen, 0, "no restore to a missing screen");
```

- [ ] **Step 3: run, expect failure** — `node tests/test_islands.js 2>&1 | tail -3` — Expected: `AssertionError` at
  "host, A and T on screen 1" (or a `screenForConnector is not defined` error if Step 1 was skipped).

- [ ] **Step 4: islands.js** — line 16 becomes:

```js
const ID = @ID@, WANT = '@WANT@', POS = @POS@, MAG = @MAG@, GAP = @GAP@, SCR = '@SCR@', ALLAPPS = @ALL@, NAMES = '@SCREENS@'.split(',').filter(n => n), HELPER = 'org.win12.reveal'; let out = 'done';
// 10: screens by connector name (main.qml passes the connected ones). The islands live on the chosen monitor, else on
// the main one (plasmashell id 0 = primary); a name that is not connected falls back to the main one too. Only ids
// screenForConnector returned are ever written: a panel sent to a missing screen loses its view.
const sid = n => screenForConnector(n), ALLMON = SCR === 'all', MAIN = Math.max(0, SCR && !ALLMON ? sid(SCR) : 0);
```

In the `s = { mode: want, ... }` object (~line 80) add `scr: p.screen,` after `mode: want,`.
In the restore branch, before `put(p, 'win12Layout', ''); continue; }` (~line 65) add:

```js
  if (s.scr !== undefined && p.screen !== s.scr && NAMES.some(n => sid(n) === s.scr)) p.screen = s.scr;   // 10: home screen, if still there
```

Before the placement (`const row = ...`, ~line 114) add, and change `sw` to use `MAIN`:

```js
 for (const q of [p].concat(isls())) if (q.screen !== MAIN) { q.screen = MAIN; out = 'again'; }   // 10: all on the chosen monitor
```
and in the `const row = ...` line replace `screenGeometry(p.screen).width` with `screenGeometry(MAIN).width`.

- [ ] **Step 5: main.qml** — `main.xml` entries (after `taskbarMagnetic`, group `XP`):

```xml
    <entry name="islandScreen" type="String">
      <label>Spec 10 "Show islands on": empty = the main (primary) monitor, "all" = every monitor, else a connector name (HDMI-A-2)</label>
      <default></default>
    </entry>
    <entry name="islandAllApps" type="Bool">
      <label>Spec 10 "Open apps on other monitors" (with islandScreen = all): false = a copy's task managers show only the windows on its monitor (stock showOnlyCurrentScreen), true = every window on every monitor</label>
      <default>false</default>
    </entry>
```
and in group `General` (next to `resetRequest`):
```xml
    <entry name="win12Mirror" type="Int">
      <label>Set by islands.js on a Start copy made for another monitor: the main Start widget's id. A copy runs no island engine and touches no panel</label>
      <default>0</default>
    </entry>
```

`main.qml`, after `onIslandPosChanged: syncIslands()` (~667):

```qml
    // 10: islands on a chosen monitor (connector name, empty = main) or on every one; the connected names go to the
    // script (@SCREENS@, resolved there with screenForConnector). A monitor plugged or unplugged re-runs the sync.
    readonly property string islandScreen: islandWant !== "single" ? cfg.islandScreen : ""
    onIslandScreenChanged: syncIslands()
    readonly property bool islandAllApps: cfg.islandAllApps
    onIslandAllAppsChanged: if (islandScreen === "all") syncIslands()
    readonly property string screenNames: Array.prototype.map.call(Qt.application.screens, s => s.name).join(",").replace(/[^A-Za-z0-9,_-]/g, "")
    onScreenNamesChanged: if (islandWant !== "single") islandTimer.restart()
```

In `syncIslands()` extend the `sed` (after `-e s/@GAP@/" + cfg.taskbarGap + "/ "`):

```qml
            + "-e s/@SCR@/" + islandScreen.replace(/[^A-Za-z0-9_-]/g, "") + "/ -e s/@ALL@/" + islandAllApps + "/ -e s/@SCREENS@/" + screenNames + "/ "
```

- [ ] **Step 6: run** — `node tests/test_islands.js 2>&1 | tail -2` → `test_islands OK`; `node tests/test_tiles.js | tail -1`
  → `tiles.js: all checks passed`; `qmllint -I /usr/lib/qt6/qml package/contents/ui/main.qml 2>&1 | grep -c error` → `0`
  (warnings are fine).

- [ ] **Step 7: commit**

```bash
git add package/contents/config/main.xml package/contents/ui/main.qml package/contents/ui/code/islands.js tests/test_islands.js
git commit -m "islands: Show islands on (one monitor): islandScreen/islandAllApps keys, screens in the harness, panels follow the chosen screen"
```

---

### Task 2: All monitors: host + app-island copies in `islands.js`; `isMirror` guards in `main.qml`

Model: opus
Depends on: 1

**Files:**
- Modify: `package/contents/ui/code/islands.js` (`mk` ~82; after the roles block ~106 and before the look block ~107;
  `others` ~109; placement ~113-118; restore ~58), `package/contents/ui/main.qml` (`islandWant` ~642, `syncIslands`
  ~703, `syncPanelLook` ~715, `syncCenterPanel` ~729, the theme generator trigger: `grep -n tgen`), `tests/test_islands.js`.

**Interfaces:**
- Consumes: Task 1 (`NAMES`, `sid`, `MAIN`, `ALLMON`, `ALLAPPS`, `s.scr`); existing `mk`, `setTM`, `put`, `get`,
  `order`, `byId`, `isl`, `isls`, `R = roles(POS)`, `tm`, `s.atm`; scripting `Widget.configGroups`/`configKeys`
  (relative to `currentConfigGroup`, as `main.qml migrateConfig` uses them).
- Produces: `s.mon[name] = { h, me, tm, A?, atm? }` (panel/widget ids per connector name); copy panels with
  `win12Island = 'M'`; copy Start widgets with `General/win12Mirror = ID`; copy task managers with the source task
  manager's `win12Island` + `launchers` and `showOnlyCurrentScreen = !ALLAPPS`; `main.qml` `isMirror`.

- [ ] **Step 1: harness** — widget `configKeys`/`configGroups` must follow `currentConfigGroup` (the script copies all
  groups). In `shell()`'s `widget` factory replace the `configKeys` getter with:

```js
        get configKeys() { return Object.keys(this.cfg[this.currentConfigGroup[0]] || {}); }, get configGroups() { return Object.keys(this.cfg); },
```

- [ ] **Step 2: failing cases** — before the final `console.log`:

```js
// ---- spec 10: All monitors ----
// every other screen gets a host copy (Start copy + task manager, same role and pins) and the other app island; no tray
sh = shell(1920, ["HDMI-A-2", "DP-1", "DP-2"]);
assert.ok(passes(sh, sub(sh, "islands", 0, true, "all")) <= 5, "all monitors converge");
const onScr = (sh, k) => sh.panels().filter(q => q.screen === k);
assert.deepStrictEqual([0, 1, 2].map(k => onScr(sh, k).length), [3, 2, 2]);
let mh = onScr(sh, 1).find(q => q.widgets("org.win12.menu").length), ma = onScr(sh, 1).find(q => q !== mh);
assert.deepStrictEqual(types(mh).filter(t => t !== "w12:reveal"), ["w12:menu", "icontasks"]);
assert.deepStrictEqual(types(ma).filter(t => t !== "w12:reveal"), ["icontasks"]);
let mme = mh.widgets("org.win12.menu")[0], mtm = mh.widgets("org.kde.plasma.icontasks")[0], matm = ma.widgets("org.kde.plasma.icontasks")[0];
assert.strictEqual(cfgOf(mme, "win12Mirror"), String(sh.me.id), "Start copy marked");
assert.strictEqual(cfgOf(mh, "win12Island"), "M"); assert.strictEqual(cfgOf(ma, "win12Island"), "M");
assert.strictEqual(cfgOf(mtm, "win12Island"), "pinned"); assert.strictEqual(cfgOf(mtm, "launchers"), "a.desktop,b.desktop", "pins on every monitor");
assert.strictEqual(cfgOf(matm, "win12Island"), "open"); assert.strictEqual(cfgOf(matm, "launchers"), "");
assert.strictEqual(cfgOf(mtm, "showOnlyCurrentScreen"), "true", "only that monitor's windows by default");
assert.ok(mh.offset < ma.offset, "copy row host, A at Left");
// the copy gets this widget's settings (all groups) in one pass, and the script still prints done at once
sh.me.currentConfigGroup = ["Style"]; sh.me.writeConfig("menuStyle", "windows12");
assert.strictEqual(runIslands(sh, sub(sh, "islands", 0, true, "all")), "done");
mme.currentConfigGroup = ["Style"]; assert.strictEqual(mme.readConfig("menuStyle"), "windows12", "settings reach the copy");
// a pin made on a copy is undone (one-way from the main monitor)
mtm.writeConfig("launchers", ["a.desktop", "b.desktop", "z.desktop"]); passes(sh, sub(sh, "islands", 0, true, "all"));
assert.strictEqual(cfgOf(mtm, "launchers"), "a.desktop,b.desktop");
// Center: the copies' roles follow (host copy = open, its island = pinned); All apps on every monitor clears the filter
assert.ok(passes(sh, sub(sh, "islands", 1, true, "all", true)) <= 4, "Center on all monitors converges");
assert.strictEqual(cfgOf(mtm, "win12Island"), "open"); assert.strictEqual(cfgOf(matm, "win12Island"), "pinned"); assert.strictEqual(cfgOf(matm, "launchers"), "a.desktop,b.desktop");
assert.strictEqual(cfgOf(mtm, "showOnlyCurrentScreen"), "false"); assert.ok(ma.offset < mh.offset, "copy row A, host at Center");
// a monitor unplugged loses its copies; plugged again gets them back; one monitor again, then One bar: no copies left
sh.screens.pop(); assert.ok(passes(sh, sub(sh, "islands", 1, true, "all", true)) <= 3); assert.strictEqual(sh.panels().length, 5);
sh.screens.push("DP-2"); assert.ok(passes(sh, sub(sh, "islands", 1, true, "all", true)) <= 4); assert.strictEqual(sh.panels().length, 7);
assert.ok(passes(sh, sub(sh, "islands", 1, true, "")) <= 3); assert.strictEqual(sh.panels().length, 3);
assert.ok(passes(sh, sub(sh, "single", 1)) <= 3); assert.strictEqual(sh.panels().length, 1);
assert.deepStrictEqual(types(sh.host), ["w12:menu", "icontasks", "marginsseparator", "systemtray", "digitalclock", "showdesktop"]);
// a Start copy never becomes a host: the script run with the copy's id does nothing
sh = shell(1920, ["HDMI-A-2", "DP-1"]); passes(sh, sub(sh, "islands", 0, true, "all"));
const copy = onScr(sh, 1).find(q => q.widgets("org.win12.menu").length).widgets("org.win12.menu")[0];
assert.strictEqual(runIslands(sh, Object.assign(sub(sh, "islands", 0, true, "all"), { ID: copy.id })), "done"); assert.strictEqual(sh.panels().length, 5);
// a split layout on all monitors: the host copy only (Start + a stock task manager with the pins)
sh = shell(1920, ["HDMI-A-2", "DP-1"]); assert.ok(passes(sh, sub(sh, "split-right", 0, true, "all")) <= 4);
assert.strictEqual(onScr(sh, 1).length, 1); mtm = onScr(sh, 1)[0].widgets("org.kde.plasma.icontasks")[0];
assert.strictEqual(cfgOf(mtm, "win12Island"), ""); assert.strictEqual(cfgOf(mtm, "launchers"), "a.desktop,b.desktop");
// Right mirrors the copy too: task manager before Start
assert.ok(passes(sh, sub(sh, "split-right", 2, true, "all")) <= 4);
assert.deepStrictEqual(types(onScr(sh, 1)[0]).filter(t => t !== "w12:reveal"), ["icontasks", "w12:menu"]);
```

- [ ] **Step 3: run, expect failure** — `node tests/test_islands.js 2>&1 | tail -3` → `AssertionError` at "all monitors
  converge" (panel counts) — the current script ignores `ALLMON`.

- [ ] **Step 4: islands.js — copies.** Change `mk` so a copy panel is not filed under a layout key:

```js
 const mk = (k, al) => { const q = new Panel(); q.location = p.location; q.height = p.height; q.lengthMode = 'fit'; q.alignment = al; put(q, 'win12Island', k); if (k !== 'M') s[k] = q.id; out = 'again'; return q; };
```

After the `isl`/`isls` line (~49) add the lookups (needed by the restore branch too):

```js
 // 10: copies for the other monitors live in s.mon[connector] = { h: host copy panel, me: Start copy, tm, A: app island, atm }
 const byPid = id => panels().find(q => q.id === id), monPanels = () => !s || !s.mon ? [] : Object.keys(s.mon).map(n => [s.mon[n].h, s.mon[n].A]).reduce((a, b) => a.concat(b), []).map(byPid).filter(q => q);
```

In the restore branch (~58) `const qs = isls();` becomes `const qs = isls().concat(monPanels());` (the copies are
removed with the islands; their widgets are not in `s.moved`, so nothing comes back from them).

After the roles block (the `if (mode === 'islands') { const atm = ..., ttm = ..., cur = ... }` statement, ~103-106)
and BEFORE the look block (`const W = { floating: ...`) insert:

```js
 // 10: All monitors: every other connected screen gets a copy of the host (a Start copy with this widget's settings +
 // a task manager like the host's: same win12Island role and pins) and, in 3 Islands, the other app island; the tray
 // island stays on the main screen. Marks: panel win12Island = 'M' (never a host, like any island), Start copy
 // win12Mirror = this id (main.qml: no engine, no panel or theme syncs). Pins are one-way (main monitor wins).
 const copyCfg = (a, b) => { let ch = false; a.currentConfigGroup = [];
  for (const g of a.configGroups.slice()) { a.currentConfigGroup = b.currentConfigGroup = [g];
   for (const k of a.configKeys.slice()) { if (/Request$|^win12Mirror$/.test(k)) continue; const v = a.readConfig(k, ''); if (String(b.readConfig(k, '')) !== String(v)) { b.writeConfig(k, v); ch = true; } } }
  if (get(b, 'General', 'win12Mirror', '') !== String(ID)) { put(b, 'win12Mirror', ID); ch = true; }
  if (ch) b.reloadConfig(); };   // a copy is complete in one pass: never 'again' for it
 const likeTM = (src, dst) => { const k = get(src, 'General', 'win12Island', ''), l = get(src, 'General', 'launchers', '');
  if (get(dst, 'General', 'win12Island', '') !== k || get(dst, 'General', 'launchers', '') !== l) { setTM(dst, k, l); out = 'again'; }
  if (get(dst, 'General', 'showOnlyCurrentScreen', '') !== String(!ALLAPPS)) { put(dst, 'showOnlyCurrentScreen', !ALLAPPS); dst.reloadConfig(); out = 'again'; } };
 s.mon = s.mon || {}; const mons = ALLMON ? NAMES.filter(n => sid(n) >= 0 && sid(n) !== MAIN) : [];
 for (const n in s.mon) if (mons.indexOf(n) < 0) { for (const id of [s.mon[n].h, s.mon[n].A]) { const q = byPid(id); if (q) q.remove(); } delete s.mon[n]; out = 'again'; }
 for (const n of mons) { const m = s.mon[n] = s.mon[n] || {}, k = sid(n), at = q => { if (q.screen !== k) { q.screen = k; out = 'again'; } return q; };
  const h = at(byPid(m.h) || mk('M', 'center')); m.h = h.id;
  let mw = byId(h, m.me); if (!mw) { mw = h.addWidget(me.type); m.me = mw.id; out = 'again'; } copyCfg(me, mw);
  if (tm) { let mt = byId(h, m.tm); if (!mt) { mt = h.addWidget(tm.type); put(mt, 'launchers', ''); m.tm = mt.id; out = 'again'; } likeTM(tm, mt);
   if (order(h, POS === 2 ? [mt.id, mw.id] : [mw.id, mt.id])) out = 'again'; }
  if (mode === 'islands') { const a = at(byPid(m.A) || mk('M', 'center')); m.A = a.id;
   let mat = byId(a, m.atm); if (!mat) { mat = a.addWidget(tm.type); put(mat, 'launchers', ''); m.atm = mat.id; out = 'again'; } likeTM(byId(isl('A'), s.atm), mat); }
  else if (m.A) { const a = byPid(m.A); if (a) a.remove(); delete m.A; delete m.atm; out = 'again'; } }
 put(p, 'win12Layout', JSON.stringify(s));
```

`others` (~109) becomes `const others = ['L', 'R', 'A', 'T'].map(isl).filter(q => q).concat(monPanels());` (the copies
get the helper and the look like every island). Task 1's screen loop stays on `[p].concat(isls())` (it must NOT drag
the copies to `MAIN`).

Replace the placement (~113-118, from `// placement:` to the closing `});` of the `row.forEach`) with a function and
one call per screen:

```js
 // placement per screen: lengths are read back (a floating window adds 8 px padding each side); the pass after a change re-checks them
 const place = (k, row) => { const sw = screenGeometry(k).width; let x = Math.round((sw - row.reduce((t, q) => t + q.length, 0) - GAP * (row.length - 1)) / 2);
  row.forEach((q, i) => { const lm = MAG ? 'custom' : 'fit', al = MAG ? 'left' : ['left', 'center', 'right'][row.length === 2 ? i * 2 : i], o = Math.max(0, x - (q.floating ? 8 : 0));
   x += q.length + GAP;
   if (q.lengthMode !== lm || q.alignment !== al || MAG && (q.minimumLength > 0 || q.offset !== o)) { q.lengthMode = lm; q.alignment = al; if (MAG) { q.minimumLength = 0; q.offset = o; } out = 'again'; } }); };
 place(MAIN, mode === 'islands' ? R.row.map(k => k === 'host' ? p : isl(k)) : mode === 'split-left' ? [isl('L'), p, isl('R')] : [p, isl('R')]);
 for (const n of mons) { const m = s.mon[n]; place(sid(n), mode === 'islands' ? R.row.filter(k => k !== 'T').map(k => byPid(k === 'host' ? m.h : m.A)) : [byPid(m.h)]); } }
```
(the final `}` closes the `for (const p of panels())` loop, as before). Note `Array.prototype.flat` is avoided on
purpose (QJSEngine); keep `reduce/concat`.

- [ ] **Step 5: main.qml guards** — a copy is a full Start, but it must not touch panels, themes or KWin. Add
  `isMirror` and REPLACE the existing `islandWant` line (~642) with the one below:

```qml
    // 10: a Start copy on another monitor (islands.js sets win12Mirror): same menu, but no island engine (its panel is
    // an island: the engine would ignore it anyway), no centre spacers, no panel look or theme syncs (the host does them)
    readonly property bool isMirror: cfg.win12Mirror > 0
    readonly property string islandWant: win12Taskbar && !isMirror ? cfg.taskbarLayout : "single"
```
and `if (isMirror) return;` as the first statement of `syncIslands()`, `syncCenterPanel()`, `syncPanelLook()`, and of
the handler that runs the taskbar theme generator (`grep -n 'tgen' package/contents/ui/main.qml` → the enclosing
`onTriggered`/`on…Changed` ~line 519; the W15 comment there says two generator runs delete each other's theme, which
is exactly what a copy would cause). Also add, next to `islandTimer`:

```qml
    // 10: the copies get this widget's settings from the engine: any settings change re-runs it while All monitors is on
    Connections { target: root.cfg; enabled: root.islandScreen === "all"; function onValueChanged(key, value) { islandTimer.restart(); } }
```

- [ ] **Step 6: run** — `node tests/test_islands.js 2>&1 | tail -2` → `test_islands OK`; `node tests/test_tiles.js | tail -1`;
  `qmllint -I /usr/lib/qt6/qml package/contents/ui/main.qml 2>&1 | grep -c error` → `0`.

- [ ] **Step 7: commit**

```bash
git add package/contents/ui/code/islands.js package/contents/ui/main.qml tests/test_islands.js
git commit -m "islands: All monitors: a Start + task manager copy and the other app island per screen, isMirror guards"
```

---

### Task 3: Settings on the Taskbar page, Spanish, README

Model: sonnet
Depends on: 1

**Files:**
- Modify: `package/contents/ui/config/ConfigTaskbar.qml` (page properties ~48-56; after `TaskbarLayoutBox { cfgPage: page }`
  ~253), `po/template.pot`, `po/es.po`, `package/contents/locale/es/LC_MESSAGES/plasma_applet_org.win12.menu.mo`,
  `README.md` (the "Patched KDE for the taskbar layouts" section).

**Interfaces:** consumes keys `islandScreen`, `islandAllApps` (Task 1). A later settings reorganisation
(`docs/superpowers/specs/*settings*`) moves these two rows; for now they sit under the layout box.

- [ ] **Step 1: page properties** — next to `property string cfg_taskbarLayout` add
  `property string cfg_islandScreen` and `property bool cfg_islandAllApps`.

- [ ] **Step 2: the two rows** — right after `TaskbarLayoutBox { cfgPage: page }` (same grid: label, control pairs):

```qml
            // spec 10: islands on one monitor (by connector name; "" follows the main one) or on all of them
            QQC2.Label { text: i18n("Show islands on:"); visible: screenBox.visible }
            QQC2.ComboBox {
                id: screenBox
                visible: page.cfg_taskbarLayout !== "single"
                readonly property var names: Array.prototype.map.call(Qt.application.screens, s => s.name)
                readonly property var values: [""].concat(names.slice(1), ["all"])   // index 0 = the main monitor, saved as ""
                model: [i18n("%1 (main)", names[0] || "")].concat(names.slice(1), [i18n("All monitors")])
                currentIndex: Math.max(0, values.indexOf(page.cfg_islandScreen === names[0] ? "" : page.cfg_islandScreen))
                onActivated: index => page.cfg_islandScreen = values[index]
            }
            QQC2.Label { text: i18n("Open apps on other monitors:"); visible: appsBox.visible }
            QQC2.ComboBox {
                id: appsBox
                visible: screenBox.visible && page.cfg_islandScreen === "all"
                model: [i18n("Only apps on that monitor"), i18n("All apps on every monitor")]
                currentIndex: page.cfg_islandAllApps ? 1 : 0
                onActivated: index => page.cfg_islandAllApps = index === 1
            }
            QQC2.Label {
                visible: screenBox.visible
                Layout.maximumWidth: Kirigami.Units.gridUnit * 24
                wrapMode: Text.WordWrap
                enabled: false
                text: i18n("All monitors: every monitor gets Start and the app islands; the tray island stays on the main monitor. Moving an app icon to another island moves it on every monitor. Pin apps from the main monitor.")
            }
```
If the saved name is no longer connected the combo shows the main monitor (index 0) while the value stays; islands.js
falls back to the main monitor the same way.

- [ ] **Step 3: lint** — `qmllint -I /usr/lib/qt6/qml package/contents/ui/config/ConfigTaskbar.qml 2>&1 | grep -c error` → `0`.

- [ ] **Step 4: strings** — `sh package/Messages.sh && grep -c 'msgstr ""$' po/es.po`, then fill the new `msgstr`s in
  `po/es.po` (remove any `#, fuzzy` msgmerge added after checking):
  - `Show islands on:` → `Mostrar las islas en:`
  - `%1 (main)` → `%1 (principal)`
  - `All monitors` → `Todos los monitores`
  - `Open apps on other monitors:` → `Apps abiertas en los otros monitores:`
  - `Only apps on that monitor` → `Solo las apps de ese monitor`
  - `All apps on every monitor` → `Todas las apps en todos los monitores`
  - `All monitors: every monitor gets Start and the app islands; the tray island stays on the main monitor. Moving an app icon to another island moves it on every monitor. Pin apps from the main monitor.` →
    `Todos los monitores: cada monitor recibe Inicio y las islas de apps; la isla de la bandeja se queda en el monitor principal. Mover el icono de una app a otra isla la mueve en todos los monitores. Ancla las apps desde el monitor principal.`

  `msgfmt --check -o package/contents/locale/es/LC_MESSAGES/plasma_applet_org.win12.menu.mo po/es.po && msgfmt --statistics po/es.po 2>&1 | tail -1`
  → `N translated messages.` with no untranslated/fuzzy count.

- [ ] **Step 5: README** — in the "Patched KDE for the taskbar layouts" section add one paragraph:
  "Multi-monitor (extended desktops): Taskbar page → *Show islands on* picks the monitor (default: the main one) or
  *All monitors* (Start and the app islands on every monitor, the tray island on the main one; *Open apps on other
  monitors* chooses that monitor's windows or all). Pin apps from the main monitor; a monitor plugged or unplugged
  rebuilds the islands. Mirrored screens need nothing."

- [ ] **Step 6: commit**

```bash
git add package/contents/ui/config/ConfigTaskbar.qml po package/contents/locale README.md
git commit -m "settings: Show islands on + Open apps on other monitors on the Taskbar page, Spanish, README"
```

---

### Task 4: KWin reveal script: the pointer's monitor only

Model: opus
Depends on: -

**Files:**
- Modify: `package/contents/extras/win12-islandreveal/contents/code/main.js` (whole file, 26 lines → ~30).

**Interfaces:** consumes `Panel.screen` and `screenForConnector` in the plasmashell script it sends; the
`org.win12.reveal` helpers islands.js puts in every island panel (copies included, Task 2). Produces nothing new for
other tasks.

- [ ] **Step 1: live API check (read-only, 1 min)** — write the scratchpad file
  `/tmp/claude-1000/-home-kalaminator-Documents-Start12/b88e81db-1485-4a3f-ae50-6c8c3972125b/scratchpad/win12api.js`:

```js
const o = workspace.screenAt(workspace.cursorPos);
print("win12-api screenAt=" + typeof workspace.screenAt + " out=" + (o ? o.name : "null") + " docks=" + workspace.stackingOrder.filter(w => w.dock).map(w => w.output ? w.output.name : "?").join(","));
```
Run: `qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.loadScript <that path> win12api >/dev/null; qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.start; sleep 2; journalctl --user -b --no-pager -g win12-api -n 2 | tail -1; qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.unloadScript win12api`
Expected: `win12-api screenAt=function out=HDMI-A-2 docks=HDMI-A-2,...`. If `out=null` or `screenAt=undefined`, stop:
`status: blocked`, `notes:` the printed line (the KWin 6.8 API differs from the plan; Task 6 decides).

- [ ] **Step 2: the script** — replace the whole file with:

```js
// W16/10: Plasma reveals each auto-hidden panel only from the edge strip under it. With split Windows 12 islands this
// script shows them together, per monitor: the pointer on the bottom pixel row of a monitor sets "reveal" on the
// org.win12.reveal helpers of that monitor's panels (islands.js puts one in each island; NeedsAttention keeps an
// auto-hidden panel shown) and clears the others; DELAY ms with the pointer above the islands' top - MARGIN, or on
// another monitor, clears it and Plasma hides them as usual. Loaded by main.qml syncIslandReveal().
const MARGIN = 16, DELAY = 700;
let on = null;   // connector name of the revealed monitor, null = none
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
```

- [ ] **Step 3: syntax check** — `node --check package/contents/extras/win12-islandreveal/contents/code/main.js && echo OK`
  (KWin globals are only referenced, not run) → `OK`.

- [ ] **Step 4: reload live and smoke-test (single monitor)** — `./install.sh` (installs the KWin script package through
  `setup.sh`), then `qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.unloadScript win12islandreveal; qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.start`;
  `journalctl --user -b --no-pager -n 20 | grep -i 'win12islandreveal\|TypeError\|ReferenceError' | tail -3` → no error
  lines. If the user's layout is a split one with auto-hide: `python3 tests/tools/w16mouse.py` (see its header) to put
  the pointer on the bottom row, then `qdbus6 org.kde.plasmashell /PlasmaShell evaluateScript "for (const p of panels()) for (const w of p.widgets('org.win12.reveal')) { w.currentConfigGroup=['General']; print(p.id + ':' + w.readConfig('reveal','') + ' ') }"`
  → every helper `true`; move the pointer to the screen centre, wait 1 s, same command → every helper `false`. If
  the layout is One bar (no helpers), note "smoke test left to Task 5" and go on.

- [ ] **Step 5: commit**

```bash
git add package/contents/extras/win12-islandreveal/contents/code/main.js
git commit -m "islandreveal: reveal only the islands of the monitor under the pointer (workspace.screenAt), clear the others"
```

---

### Task 5: Live single-monitor regression

Model: opus
Depends on: 1, 2, 3, 4

Only HDMI-A-2 (id 0) is connected: this checks that the new code changes nothing for one monitor, that the two name
sources agree, and that `all` with one screen makes no copies. Never set a `screen` other than 0.

- [ ] **Step 1: record and back up** — `sh tests/tools/state.sh | head -20` (write the layout/pos/look + panel lines
  into the progress entry's `notes:`); `D=$(date +%Y%m%d-%H%M); for f in plasma-org.kde.plasma.desktop-appletsrc plasmashellrc win12islandsrc; do cp ~/.config/$f ~/.config/$f.bak-t5mm-$D 2>/dev/null; done; echo $D`.

- [ ] **Step 2: install** — `./install.sh --restart 2>&1 | tail -4` → `test_islands OK`, `tiles.js: all checks passed`,
  no error; wait for the bar (`sleep 15`).

- [ ] **Step 3: names agree** — `qdbus6 org.kde.plasmashell /PlasmaShell evaluateScript "print(screenForConnector('HDMI-A-2') + ' ' + screenCount + ' ' + panels().map(p => p.id + ':' + p.screen).join(' '))"`
  → `0 1 <id>:0 ...` (every panel on 0). `kscreen-doctor -o | grep -c HDMI-A-2` → `>= 1`. (Qt's `QScreen::name` on
  Wayland is the connector name; if `screenForConnector` prints `-1`, the names differ: `status: blocked`, `notes:`
  both outputs — islands.js then always falls back to the main monitor, which is still correct for one monitor.)

- [ ] **Step 4: matrix with the setting** — helper: `set() { qdbus6 org.kde.plasmashell /PlasmaShell evaluateScript "for (const p of panels()) for (const w of p.widgets('org.win12.menu')) { w.currentConfigGroup=['XP']; w.writeConfig('islandScreen','$1'); w.reloadConfig(); }"; }`.
  Run, with `sh tests/tools/state.sh | head -12` and `qdbus6 ... "print(panels().map(p => p.id + ':' + p.screen + ':' + p.offset).join(' '))"` after each:
  1. `sh tests/tools/setlayout.sh islands 0` (took < 1500 ms) → 3 panels, all `:0`, offsets ascending host < A < T.
  2. `set HDMI-A-2` then `sleep 3` → same 3 panels, nothing moved (same ids, same offsets).
  3. `set all` then `sleep 3` → still 3 panels (one screen: no copies), `win12Layout` JSON has `"mon":{}`.
  4. `sh tests/tools/setlayout.sh islands 2` → row T, A, host (offsets T < A < host), all `:0`.
  5. `set ''` then `sh tests/tools/setlayout.sh single 0` → 1 panel, order Start, tasks, separator, tray, clock, show
     desktop (compare with Step 1), all 19 tray icons (`state.sh` tray line `extra=`).
  6. Reveal (if the look auto-hides): `python3 tests/tools/w16mouse.py` bottom row → helpers `true`
     (command in Task 4 Step 4), centre → `false` after 1 s.
  Screenshots after 1, 4 and 5: `spectacle -b -n -f -o /tmp/claude-1000/t5mm-<n>.png`.

- [ ] **Step 5: settings page** — `kpackagetool6 -t Plasma/Applet --show org.win12.menu >/dev/null && echo installed`;
  open the Taskbar page (`win12-settings` or right-click Start → Configure), set layout 3 Islands: the row "Show islands
  on:" lists `HDMI-A-2 (main)` and `All monitors`; pick All monitors → "Open apps on other monitors:" appears. Put it
  back to `HDMI-A-2 (main)` and the layout back to the user's. (If no display interaction is possible from the run,
  check instead that `grep -c 'Show islands on' package/contents/ui/config/ConfigTaskbar.qml` is `1` and note "UI
  shown to the user for a look".)

- [ ] **Step 6: restore the user's desktop** — put `taskbarLayout` / `menuPosition` / `islandScreen` back to the Step 1
  values with `setlayout.sh` + `set`; `sh tests/tools/state.sh | head -12` must match Step 1 (same widgets, tray
  `extra=` count). If it does not: stop plasmashell (`systemctl --user stop plasma-plasmashell`), copy the three
  `.bak-t5mm-$D` files back, `systemctl --user start plasma-plasmashell`. Pointer to the centre, no job left.

- [ ] **Step 7: progress entry** — timings per switch, the Step 3 line, screenshot paths; no commit unless something
  was fixed (then one commit `islands: <fix>` with a harness case first).

---

### Task 6: Whole-change review + ELI5

Model: opus
Depends on: all

- [ ] **Step 1:** `git diff e8a51c7..HEAD --stat` and the spec section 10 side by side: map each bullet (Show islands
  on; All monitors incl. tray only on main and pins everywhere; Open apps on other monitors; moves on all monitors via
  the registry; reveal per pointer monitor; plug/unplug rebuild; harness testing) to a commit; list gaps.
- [ ] **Step 2:** read-only review of `islands.js` (the whole file, ~150 lines) for: a copy panel ever treated as a host
  or as `L/R/A/T`; `screen` written from a value that was not returned by `screenForConnector`; `out = 'again'` set
  by `copyCfg` (must not); `s.mon` left in `win12Layout` after a restore; `Array.prototype.flat`/`Object.values`
  (QJSEngine); the reveal script's evaluateScript string for an injection path (connector name filtered).
- [ ] **Step 3:** `node tests/test_islands.js | tail -1`, `node tests/test_tiles.js | tail -1`, qmllint of `main.qml`
  and `ConfigTaskbar.qml`, `grep -rn 'sudo' package tests | grep -v pacman | wc -l` → `0`.
- [ ] **Step 4:** fix small things inline (harness case first), commit `review: <what>`.
- [ ] **Step 5:** progress entry with the spec map, the gaps, and an ELI5 for the user (numbered, plain words): what
  the two settings do, what happens on a second monitor, pins from the main monitor, "the real 2-monitor check is
  yours: plug one in, set All monitors, drag an app to another island and look at both taskbars", and
  `next: none`, `next-model: none`.

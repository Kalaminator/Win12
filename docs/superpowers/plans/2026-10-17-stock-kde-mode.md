# Stock KDE mode — implementation plan (spec section 11)

> For agentic workers: follow the "Driver protocol" below. One task per run, one commit per task, then one
> progress-log entry. Steps use `- [ ]` checkboxes.

**Goal:** one widget, two modes picked at runtime. On Win12's patched KDE nothing changes (today's one-pass
engine). On a stock KDE (no patch, any distro) One bar, Split-left, Split-right and the Start position work
again the old slow way (one layout step per 0.3 s pass, 5-10 s per switch, icons jump); 3 Islands and drag
between islands stay patched-only and the settings page says so. plasmashell is never restarted by Win12.

**Architecture:** `islands.js` gets one constant `STOCK` (sed `@STOCK@` from `main.qml`, `!kdePatched`) and a
pass breaker: in stock mode the pass ends after its first layout mutation (add, move, remove, index set), `order()`
sets one index per pass and `tray()` always copies. `centerLoop()` (main.qml) gets the same `STOCK` constant and
the same two rules (f70da6e behaviour). `main.qml` gates syncs on `kdeChecked` (the check answered) instead of
`kdePatched`, and refuses to touch panels when 3 Islands is wanted on stock. The harness grows a stock fake shell.
No second engine, no new config key; the only new switch is the marker file `~/.config/win12-force-stock`.

**Tech stack:** KDE Plasma 6.8.0 (patched or stock, Qt 6.12), plasmashell scripting API, QML, node harness
(`tests/test_islands.js`), gettext (10 languages in `po/`).

**Spec:** `docs/superpowers/specs/2026-10-16-fast-islands-design.md` section 11 (read it first every run; 11.3
is the dependency table, 11.4 the engine rules, 11.5 the test model). Sections 2 and 5.1 explain why stock is slow.

**Base commit:** `1f009d0`. Line numbers below are from that commit; re-read the lines you touch (other agents
may have moved them).

## Evidence (read-only; from `kde/patches/*`, `git show f70da6e:...`, the engine and harness at 1f009d0)

- The pre-patch engine (`git show f70da6e:package/contents/ui/code/islands.js`, 75 lines) had the rule in its
  comment: "one move or index set per pass: several in one script land out of order or leave the item unplaced".
  Its `fix(q, order)` set only the first mismatching index; its restore moved one widget back per pass; adds
  were followed by `break`; a widget with `index < 0` in the right island went "out and in again". The old
  `syncCenterPanel` script (same commit) set one index per pass: "a just-added spacer reads index -1".
- Today's engine (`islands.js` 149 lines) does everything in one pass: `order()` (l.30) sets every mismatch,
  `tray()` (l.34-38) moves live once `win12TrayClean=true`, the restore (l.57-66) moves all `s.moved` back, the
  rebuild (l.87-107) makes panels, fresh task managers, weather, moves and orders, the `lost` net (l.103-104)
  bounces an unplaced widget to the host. `centerLoop()` (main.qml l.746-782) sets all indexes when `bad`.
- What each patch removes and which code relies on it: spec 11.3. Everything else the engine uses
  (`Panel.screen`, `screenForConnector`, `reloadConfig`, `configKeys`, `widgetIds`, panel geometry, `win12*`
  keys, the reveal helper and KWin script) is stock scripting API.
- The harness fake (`tests/test_islands.js` l.24-62) has patched semantics only: `index` = live position. The
  e8a51c7 harness had an `orphan` set and `lost` list modelling the stock out-of-range insert (removed in
  bdd2d2a); reuse that shape for the stock fake. `center()` (l.154-157) already runs `centerLoop()`'s script
  extracted from main.qml against the fake.
- `install.sh` runs `node tests/test_tiles.js && node tests/test_islands.js && node tests/test_settings_keys.js
  --strict`; `--restart` = `systemctl --user restart plasma-plasmashell.service`.
- Strings: `package/Messages.sh` (run from anywhere) regenerates `po/template.pot` and merges all `po/*.po`
  (ar de es fr hi it ja pt_BR ru zh_CN); the last i18n commits (`5a0cb3b`, `1f009d0`) show the .mo rebuild.

## Global constraints

- Everything from the fast-islands plan's "Global constraints" applies (Arch only, sudo = `/usr/bin/pacman` only
  and NOT available to agents in this plan, names `win12*`, short files, WHY comments, `// ponytail:` notes,
  `<= ~40 tool calls / ~100k tokens` per run, test output through `| tail`, live safety: back up
  `~/.config/plasma-org.kde.plasma.desktop-appletsrc`, `~/.config/plasmashellrc` and `~/.config/win12islandsrc`
  before any live test, restart plasmashell only with `./install.sh --restart` and only in Task 4).
- This PC runs the PATCHED KDE. No task changes `kde/patches/` or any package. Stock semantics are proven in the
  harness only (spec 11.5); the live Task 4 uses the marker file and proves only what spec 11.5 says it proves.
- With `STOCK = false` the engine and `centerLoop()` must behave exactly as at 1f009d0: every existing harness
  case passes unchanged (do not loosen a `<= 3` to make a case pass).
- One test lock: at most two agents run tests at once (one testing, one coding); the reviewer (Task 5) is
  read-only apart from running the harness.
- Win12 never restarts plasmashell. No task adds a `systemctl` call to the product.
- Commits: one per task, message `<area>: <what>` + a short why, ending with the two attribution lines the
  driver prompt gives (when it gives none: the same two lines as this plan's commit).

## Driver protocol (progress log format)

File: `docs/superpowers/plans/2026-10-17-stock-kde-mode-progress.md`. Each run:

1. Reads the spec (section 11 + 2 + 5.1), this plan, and the progress log. The last entry's `next:` line names
   the task to do; `next-model:` names the model the driver must start for it.
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

Rules: `status: blocked` is for tasks that need something unavailable (sudo pacman, the user at the keyboard).
A blocked task's dependants (see each task's **Depends on**) are `skipped` with `left: blocked by Task N`;
`next:` then points at the next task that does not depend on it. `partial` = out of budget mid-task: the
entry's `next:` names the SAME task and `notes:` says the exact step to resume at (work is committed at a safe
point first, or stashed with `git stash push -m "Task N partial"` and noted). A model name in a task header
(`Model: opus|sonnet`) is what the previous entry's `next-model:` must carry. Task 6 is the user's: the agent
that finishes Task 5 writes `next: Task 6 — user's live stock test` and `next-model: none`.

## Task order

1. Task 1 — `STOCK` in `islands.js` + the stock fake shell + stock cases (opus)
2. Task 2 — `main.qml`: `kdeChecked`, the 3 Islands gate, `@STOCK@`, `centerLoop()` stock rules; `Tiles.PATCH_CHECK`;
   harness `center()` + stock mirror case (opus)
3. Task 3 — Settings note, README, FEATURES, 10 translations (sonnet)
4. Task 4 — Live forced-stock test on this (patched) PC (opus)
5. Task 5 — Whole-change review + ELI5 (opus)
6. Task 6 — The user's live stock test (user; checklist below)

## Review focus

- Patched path unchanged: `git diff 1f009d0 -- package/contents/ui/code/islands.js` read with `STOCK = false` in
  mind; every new line is `if (STOCK ...)` or a comment, or a pure refactor the patched cases cover.
- Every layout mutation in the host loop is behind the breaker in stock mode (adds, moves incl. the weather move
  at l.98 and the mirror copies of 776c5c9, removes, `order()`); no index is read in the same pass after one.
- `tray()` ignores `win12TrayClean` only on stock; the key is still written (true after a fresh copy too).
- `main.qml`: nothing runs before `kdeChecked`; with `islands` wanted on stock neither sync touches a panel;
  `onKdeChecked` runs the syncs once; the sed line has `@STOCK@`; `centerLoop()` and the harness `center()`
  agree on the parameter list.
- The harness stock fake snapshots `saved` and `x` only in `endPass()`; `lost` widgets are in `widgets()` with
  index -1; the clamp is `saved.length - 1` after the id is removed.
- Strings: one changed string, 10 `po/*.po` translated (no fuzzy left), `.mo` rebuilt, `msgfmt --check` clean.

## Defaults picked without asking (the user should know)

- 3 Islands wanted on stock = Win12 touches no panel at all (not "fall back to Split"): a patched desktop whose
  packages were replaced by a Plasma update keeps its three islands (the task managers show every window until
  `kde/build.sh` runs again); picking 3 Islands on stock changes nothing and the note turns red.
- The forced-stock switch is a marker file (`${XDG_CONFIG_HOME:-$HOME/.config}/win12-force-stock`), read by the
  same executable check, once per plasmashell start. No config key, no UI.
- On stock every split makes a fresh tray (General group copied; the tray's own child-applet settings reset),
  exactly as before the patches (f70da6e). Accepted by the user ("slower and some icon jumping").
- Pass caps unchanged (`seq 130` x 0.3 s islands, `seq 25` Start position); a stock split needs about 15-20
  passes, a mirror about 8.
- "All monitors" on stock is harness-covered for convergence only (no live test: one monitor here).
- The settings note is one string, neutral colour, red only while 3 Islands is selected.

---

### Task 1: `STOCK` in `islands.js` + the stock fake shell + stock cases

**Model:** opus. **Depends on:** nothing.

**Files:**
- Modify: `package/contents/ui/code/islands.js` (header comment, l.17 constants, l.30 `order`, l.34 `tray`, the
  host loop l.43-149: breaker after each mutating phase)
- Modify: `tests/test_islands.js` (l.24-62 `shell`, l.63-69 `runIslands`, l.73 `sub`, new stock section)

**Interfaces:** `islands.js` reads `STOCK = @STOCK@` (boolean literal; Task 2 adds the sed; the harness `sub()`
gets `STOCK` with default `false`). `shell(screenW = 1920, screens = ["HDMI-A-2"], stock = false)`; `sh.stock`
(writable, the "patch lost" case flips it); `sh.endPass()`; `Panel.saved`, `Panel.lost`, `Widget.x`.

**Steps:**
- [ ] Read spec 11.3-11.5, `islands.js` whole (149 lines, it is the file you edit), `tests/test_islands.js`
      l.1-100 and `git show f70da6e:package/contents/ui/code/islands.js | sed -n '14,16p;35,61p'` (the old one
      step per pass shape).
- [ ] `islands.js` l.17: add `STOCK = @STOCK@` to the constants. Header comment: two lines on stock mode (why:
      `Widget.index` reads the order saved only after the script returns; the old f70da6e rule).
- [ ] Breaker: `const stop = () => STOCK && out === 'again';` next to `out`. In the host loop add
      `if (stop()) continue;` after every phase that adds, moves, removes or orders: the leaving-islands block
      (l.53-56, task manager removes), the restore (l.57-66: helper removes, then the moves loop, then the panel
      removes, then `order`), the re-key (l.67-78: weather move + panel remove), the snapshot (l.79-86 is
      config-only: no breaker), the rebuild adds (l.87-99: new panels, fresh task managers, weather add/move),
      the moves (l.102), the `lost` bounce (l.103-104 already `continue`s), `order(dst)` (l.107), the 3 Islands
      role block (l.108+: `setTM` is config-only, but read it), the helper adds, and the mirror copies of
      776c5c9 (read them: any `addWidget`/`index` there gets the breaker too). Inside the two `for (const id of
      s.moved)` loops (l.63, l.102): `if (STOCK) break;` after a move. Placement (lengths, offsets, screens) is
      config and geometry: no breaker.
- [ ] `order()` (l.30): in stock mode set only the first mismatching index and return true (a `for` loop with
      `if (STOCK) break;` after the set). Comment why: the stock setter clamps one short, the first mismatch is
      never the last item.
- [ ] `tray()` (l.34): `get(host, 'General', 'win12TrayClean', '') === 'true'` becomes
      `!STOCK && get(...) === 'true'`. Keep the `put(host, 'win12TrayClean', 'true')`. Comment why.
- [ ] Harness stock fake (keep the patched behaviour when `!sh.stock`): `Panel` gets `saved = []`, `lost = []`;
      `widgets()` returns `ws.concat(lost, gone)` (type filter as today); widget `index` getter: stock →
      `panel.saved.indexOf(id)`; setter: stock → `o = saved.filter(i => i !== id); k = Math.min(i, Math.max(0,
      o.length - 1)); o.splice(k, 0, id); saved = o;` then reorder `ws` by position in `o` (missing ids after,
      in their current order); `addWidget(existing)` stock → `at = x.x`, remove from the source `ws`/`lost`, `at
      > ws.length ? lost.push(x) : ws.splice(at, 0, x)`, `x.x = 0`; `addWidget(type)` → push, `x = 0`; widget
      `remove()` also removes from `lost`; `sh.endPass = () => panels.forEach(q => { q.saved = q.ws.map(w =>
      w.id); q.ws.forEach((w, i) => w.x = i); })` (no-op when `!sh.stock`); `runIslands` calls `sh.endPass()`
      after the script. `shell()` ends with `sh.endPass()` so the user's bar starts saved. `sub()` gets
      `STOCK = false` and emits `STOCK`.
- [ ] Stock cases (new section `// ---- stock KDE (no patch): one step per pass, spec 11 ----`, using
      `sub(sh, WANT, POS, MAG, SCR, ALL, true)` or a small `ssub` wrapper):
      (a) `shell(1920, ["HDMI-A-2"], true)`: split-left `n = passes(..., 40)`; `3 < n && n <= 40`; 3 panels;
      host types (no reveal) `[w12:menu, icontasks, marginsseparator]`, R `[systemtray, digitalclock,
      showdesktop]`, L `[weather]`; `sh.panels().every(q => !q.lost.length)`; `host.offset < L...` as the row
      order requires (read l.75-84 and the islandlogic row for split-left).
      (b) back to single <= 40: 1 panel, the original type order; the tray id differs from the start (fresh).
      (c) split-left again <= 40 although `win12TrayClean` is `"true"`: the tray id changes again.
      (d) split-right <= 40 from a fresh stock shell and back to single.
      (e) patch lost: `shell()` (patched) → `passes(sh, sub(sh, "islands", 0)) <= 3`; `sh.stock = true;
      sh.endPass()`; split-left with `STOCK = true` <= 40 → 3 panels keyed `L`, (host), `R`; exactly one
      icontasks in all panels; its `launchers` = `"a.desktop,b.desktop"`; no `lost` widget.
      (f) every existing case untouched and passing (patched `sub()` default).
- [ ] Verify: `node tests/test_islands.js | tail -3` → `test_islands OK`; `node tests/test_tiles.js | tail -1`;
      `sed -e 's/@ID@/1/' ... -e 's/@STOCK@/true/' islandlogic.js islands.js > /tmp/x.js && node --check` for
      STOCK true and false (no `@X@` left: `grep -c '@[A-Z]*@'` = 0).
- [ ] Commit: `islands: stock KDE mode — one layout step per pass behind STOCK, fresh tray always; harness stock
      fake (saved order, clamp, old-slot insert, lost) + 5 cases`.

### Task 2: `main.qml` wiring, `centerLoop()` stock rules, `Tiles.PATCH_CHECK`, harness `center()`

**Model:** opus. **Depends on:** Task 1.

**Files:**
- Modify: `package/contents/ui/code/tiles.js` (next to `QDBUS` l.550: `PATCH_CHECK`)
- Modify: `package/contents/ui/main.qml` (l.645-660 check block, l.716 + l.719-723 `syncIslands`, l.743
  `syncCenterPanel`, l.746-782 `centerLoop`)
- Modify: `package/contents/ui/config/TaskbarLayoutBox.qml` (l.15-19: use `Tiles.PATCH_CHECK`; the note text is
  Task 3's)
- Modify: `tests/test_islands.js` (l.154-157 `center()`; new stock mirror case)

**Interfaces:** `Tiles.PATCH_CHECK` = `pacman -T libplasma-win12 plasma-workspace-win12 plasma-desktop-win12
>/dev/null 2>&1 && [ ! -e "${XDG_CONFIG_HOME:-$HOME/.config}/win12-force-stock" ] && echo ok`. `main.qml`:
`kdePatched` (as today), new `kdeChecked`, new `readonly islandsNeedPatch`. `centerLoop()`'s script defines
`STOCK = <!kdePatched>`; the harness `center()` passes `kdePatched` as a third `Function` parameter.

**Steps:**
- [ ] Read spec 11.1-11.2 and 11.4, `main.qml` l.640-800, `tests/test_islands.js` l.150-160, and
      `git show f70da6e:package/contents/ui/main.qml | awk '/function syncCenterPanel/,/seq/' | sed -n '24,31p'`
      (the old one-set-per-pass lines).
- [ ] `tiles.js`: `var PATCH_CHECK = "...";` with a WHY comment (one check, two users, the marker file for the
      dev PC). Check how `TaskbarLayoutBox.qml` reaches `Tiles` (grep `import "../code/tiles.js"` in
      `package/contents/ui/config/*.qml`); add the import if missing.
- [ ] `main.qml` check block: `connectedSources: [Tiles.PATCH_CHECK]`; `onNewData` sets `kdePatched` then
      `kdeChecked = true`; replace `onKdePatchedChanged: if (kdePatched) {...}` with `onKdeCheckedChanged: {
      syncCenterPanel(); syncIslands(); }`; add `readonly property bool islandsNeedPatch: !kdePatched &&
      cfg.taskbarLayout === "islands"`. Update the W17 comment (stock = slow path, 3 Islands = patched only).
- [ ] Guards l.716 and l.743: `if (!configReady || !kdeChecked || isMirror || islandsNeedPatch) return;`.
- [ ] `syncIslands()` sed: add `-e s/@STOCK@/" + !kdePatched + "/`.
- [ ] `centerLoop()`: the script's first line gets `STOCK = " + !kdePatched + "`. Stock rules: (1) a pass that
      removed or added a spacer ends there (`out = 'again'` after the add loop too, then `if (STOCK && out ===
      'again') continue;` before `const others`); (2) when `bad`, stock sets only the first mismatch
      (`want.findIndex((id, k) => byId(id).index < 0 || cur[k] !== id)`), patched sets all as today. Keep the
      spacer `expanding` flip and the `win12Fill` writes as they are (config-only). Comments say why.
- [ ] Harness `center()`: `new Function("Plasmoid", "islandWant", "kdePatched", ...)({ id }, want,
      !sh.stock)`; after each script run call `sh.endPass()`. New cases: stock shell, One bar, `centerTaskbar`
      true + `menuPosition` 2: `center(sh, "single") <= 25`, order mirrored as l.160's expectation (with the
      spacer); `menuPosition` 0: `<= 25`, original order, `win12PanelOrder` empty; the b34e0c4 scenario on
      stock (split-left → single at pos 2 → `center` converges). Existing `center` cases unchanged (`<= 3`).
- [ ] Verify: `node tests/test_islands.js | tail -3`; `qmllint package/contents/ui/main.qml
      package/contents/ui/config/TaskbarLayoutBox.qml 2>&1 | grep -c error` = 0 (or the same count as at
      1f009d0, noted); `grep -c 'kdePatched' package/contents/ui/main.qml` sanity; `node
      tests/test_settings_keys.js --strict | tail -1`.
- [ ] Commit: `main.qml: stock KDE mode — syncs run once the check answers, 3 Islands needs the patch, @STOCK@
      to the engine, one index set per pass in centerLoop; Tiles.PATCH_CHECK with the force-stock marker`.

### Task 3: Settings note, README, FEATURES, 10 translations

**Model:** sonnet. **Depends on:** Task 2.

**Files:**
- Modify: `package/contents/ui/config/TaskbarLayoutBox.qml` (l.20-26 note)
- Modify: `README.md` (l.103-108), `docs/FEATURES.md` (l.71)
- Modify: `po/template.pot`, `po/{ar,de,es,fr,hi,it,ja,pt_BR,ru,zh_CN}.po`, the `.mo` files

**Steps:**
- [ ] Read spec 11.1, `TaskbarLayoutBox.qml`, README l.98-110, FEATURES l.69-73, and `git show 1f009d0 --stat
      | tail -15` plus `git show 5a0cb3b --stat | tail -3` (where the `.mo` files live and how the last run
      rebuilt them; grep `msgfmt` in `install.sh`, `tools/`, `package/`).
- [ ] Note: visible when `!box.kdePatched`; `color: box.cfgPage.cfg_taskbarLayout === "islands" ?
      Kirigami.Theme.negativeTextColor : Kirigami.Theme.textColor`; text (one `i18n` string): "3 Islands and
      moving apps between islands need Win12's patched KDE packages: run kde/build.sh from the Win12 source
      folder, then log out and in. On a stock KDE the One bar, the Split layouts and the Start position work,
      but a change takes a few seconds." Remove the old string.
- [ ] README l.103-108: "Without the patched packages" paragraph → the split layouts and the Start position
      still work the slow way (one step every 0.3 s, 5-10 s per change, icons jump), 3 Islands and drag between
      islands are off and the Taskbar page says so; the pacman hook sentence and the undo stay. FEATURES l.71:
      same in one line.
- [ ] `sh package/Messages.sh`; translate the new string in all 10 `po/*.po` (native wording, no fuzzy left:
      `grep -c '#, fuzzy' po/*.po` all 0); rebuild every `.mo` the way the last i18n commit did; `for f in
      po/*.po; do msgfmt --check --statistics -o /dev/null $f; done 2>&1 | grep -v '0 untranslated' | tail`.
- [ ] Verify: `qmllint package/contents/ui/config/TaskbarLayoutBox.qml 2>&1 | grep -c error`;
      `node tests/test_settings_keys.js --strict | tail -1`; `git status --short | wc -l` = only the files above.
- [ ] Commit: `settings/docs: stock KDE note (3 Islands needs the patch, splits and Start position work slowly);
      README, FEATURES, 10 translations`.

### Task 4: Live forced-stock test on this (patched) PC

**Model:** opus. **Depends on:** Tasks 1-3.

What it proves: the stock code path runs live without script errors, converges, never restarts plasmashell,
the One bar restore and the Start-position mirror come back right, and the settings note shows. What it cannot
prove: stock `Widget.index` semantics (the patched index is always fresh), the stock out-of-range insert, the
tray-icon loss of a live move. Those are harness-only (Task 1) and the user's Task 6.

This task restarts plasmashell twice through `./install.sh --restart` (new code + marker on, then marker off).
Apps started from the Start menu die with it; earlier live tasks did the same. Do it only when the previous entry
or the driver prompt says the user is away or agreed.

**Steps:**
- [ ] Read spec 11.5 and the multimonitor plan Task 5 (`docs/superpowers/plans/2026-10-17-multimonitor-islands.md`
      l.613-660: switch method, state dump and timing helpers) and its progress entry (l.48-56 of the
      multimonitor progress log).
- [ ] Record the user's desktop (layout, Start position, panel ids and widget order, tray `extraItems` count);
      back up the three config files with suffix `.bak-stockmode-<date>`; `git status` clean.
- [ ] `touch "${XDG_CONFIG_HOME:-$HOME/.config}/win12-force-stock"`; `./install.sh --restart 2>&1 | tail -5`
      (the three node tests run inside). Wait ONE blocking `sleep 8`; `journalctl --user -u
      plasma-plasmashell -b --since -1min 2>/dev/null | grep -ci 'win12\|TypeError\|ReferenceError'`.
- [ ] Matrix, each switch timed to `done` (the lock file + `win12Layout` key or the method of Task 5): split-left
      0, split-right 0, single 0, single 2 (mirror), single 1 (centre), single 0, islands 0 (expected: nothing
      changes; open the Taskbar page once and confirm the note is red; `win12Layout` unchanged), then the user's
      own layout and position. After each: panel count and order; tray `extraItems` count equals the start; no
      widget with `index < 0` (one `evaluateScript` listing `index` per widget); journal grep as above = 0.
      Expected times: 5-10 s per split, 2-4 s per mirror.
- [ ] `rm "${XDG_CONFIG_HOME:-$HOME/.config}/win12-force-stock"`; `./install.sh --restart`; one split switch:
      < 1.5 s (fast mode back); the desktop equals the recording (same widget types and order; new ids for the
      fresh tray/islands are expected and noted).
- [ ] Nothing left running (`jobs`, no `sleep`, pointer away from the bottom edge). Entry `notes:` carries the
      numbers; a bug found is fixed in this task only when it is a one-liner, else `left:` for Task 5.
- [ ] Commit only if code changed (else the log entry alone): `islands: live forced-stock fixes — <what>`.

### Task 5: Whole-change review + ELI5

**Model:** opus. **Depends on:** Tasks 1-4 (Task 4 may be `skipped`; review then says so).

**Steps:**
- [ ] `git diff 1f009d0 --stat` and the diff of `islands.js`, `main.qml` (l.640-800 only), `tiles.js`,
      `TaskbarLayoutBox.qml`, `tests/test_islands.js`, the docs. Check every item of "Review focus".
- [ ] Run `node tests/test_islands.js | tail -3`, `node tests/test_tiles.js | tail -1`, `node
      tests/test_settings_keys.js --strict | tail -1`, `qmllint` on the two QML files, `msgfmt --check` on the
      10 po files (`| tail -3`).
- [ ] Fix small findings directly (comments, a missed breaker, a missing `endPass`); bigger ones go to `left:`
      with the exact line and why.
- [ ] Append to the progress entry an ELI5 paragraph (5-8 lines: what a stock user gets, what they do not, why
      slow, how the dev PC was tested, what only the user can test) and the Task 6 checklist pointer.
- [ ] Commit (when something changed): `review: stock KDE mode — <fixes>`. Entry: `next: Task 6 — user's live
      stock test`, `next-model: none`.

### Task 6: The user's live stock test (user, at the keyboard, needs sudo)

Do this with nothing important open: logging out closes everything. About 15 minutes plus the log in/out.

- [ ] Before: `pacman -T libplasma-win12 plasma-workspace-win12 plasma-desktop-win12` prints nothing (patched).
      `ls ~/.cache/win12-kde/{libplasma,plasma-workspace,plasma-desktop}/*.pkg.tar.zst` lists the patched
      packages (these are the way back; if one folder is missing, run `kde/build.sh` first and stop here).
- [ ] `cp ~/.config/plasma-org.kde.plasma.desktop-appletsrc{,.bak-stock}; cp ~/.config/plasmashellrc{,.bak-stock};
      cp ~/.config/win12islandsrc{,.bak-stock} 2>/dev/null; true`. Note your layout and Start position.
- [ ] `sudo pacman -S libplasma plasma-workspace plasma-desktop` (stock). The `win12-kde-hook` message "Run
      kde/build.sh again" is expected. Log out, log in.
- [ ] Checks (write a yes/no and the seconds for each):
      1. Taskbar settings page: the note is shown, not red while One bar/Split is selected.
      2. One bar → Split-left: three panels within 5-15 s; every tray icon you had is there (count them);
         Start, apps, separator in the middle; weather left; tray, clock, show desktop right.
      3. Split-left → Split-right → One bar: the bar comes back in the old order within 5-15 s each.
      4. On One bar: Start position Right (mirror), then Centre, then Left: each lands within 2-5 s.
      5. Pick 3 Islands: nothing changes on the bar; the note turns red. Pick your layout again.
      6. Icons jumping during a switch and a few seconds of wait are expected. A panel that never settles after
         40 s, a missing tray icon, a widget gone, or a plasmashell crash are bugs: note what you did.
      7. `journalctl --user -u plasma-plasmashell -b | grep -i 'win12\|TypeError\|ReferenceError' | tail -20`
         (copy the lines, if any).
- [ ] Back: `sudo pacman -U ~/.cache/win12-kde/{libplasma,plasma-workspace,plasma-desktop}/*.pkg.tar.zst`
      (installs the -debug packages too, as `kde/build.sh` does). Log out, log in. `pacman -T ...` prints
      nothing again; one split switch is fast (< 1.5 s).
- [ ] If anything went wrong with the panels: restore the three `.bak-stock` files while logged out (TTY or
      another session) and log in again.
- [ ] Tell Claude the results (the yes/no list and the journal lines); the next agent appends them to the
      progress log as the Task 6 entry.

# Settings reorganisation (9 pages, one place per option) — implementation plan

> **For agentic workers:** this plan is executed by an unattended loop of fresh `claude -p` runs (see "Driver
> protocol"). Each run does ONE task. Inside a run use superpowers:executing-plans (checkbox steps below).

**Goal:** the Win12 settings window gets the 9 pages of the approved proposal (Style, Start menu, Apps list,
Start button & shortcuts, Taskbar, Windows, Appearance, Search, Advanced), every config key edited on exactly one
page, style-specific rows hidden by one shared rule set, labels in one voice, Spanish complete.

**Architecture:** config keys never change and `package/contents/config/main.xml` is not touched; only
`package/contents/config/config.qml` (page list) and the page files under `package/contents/ui/config/` move
controls around. A tiny `StyleKinds.qml` helper holds the visibility rules of proposal section 4 and every page
gates through it. A node guard (`tests/test_settings_keys.js`) proves after every task that no key lost its control
and, at the end, that none has two. Pages are built one per task, each task moving controls out of the old pages in
the same commit, so the tree is consistent at every commit.

**Tech stack:** QML (Qt 6.12, Kirigami, KCM.SimpleKCM pages), node for the guards, `/usr/lib/qt6/bin/qml` for the
helper's offscreen unit test, `/usr/lib/qt6/bin/qmllint`, gettext (`xgettext`, `msgmerge`, `msgattrib`, `msgfmt`),
`spectacle` + `tests/tools/w16mouse.py` for the screenshot checks.

**Spec:** `docs/research/settings-reorg-proposal.md` (source of truth: page list, every row's new label, visibility
and enabled rules; read sections 3 and 4 every run). Inventory of today's pages: `docs/research/settings-inventory.md`.

**Runs after:** `docs/superpowers/plans/2026-10-17-multimonitor-islands.md` (adds the "Show islands on" and "Open apps
on other monitors" controls to the Taskbar page and their keys to `main.xml`). Its progress log must end with
`next: none` before Task 1 starts. **Base commit:** the commit Task 1 starts from (Task 1 records it); the whole change
set = `git diff <base>..HEAD`.

## Global constraints

- Keys never change. `main.xml` is not edited by this plan. User configs keep working because only `config.qml` and
  the page QML change.
- A page declares `property alias cfg_<key>: <id>.<prop>` or `property <type> cfg_<key>` for every key it edits. A key a
  page only reads (style gating, enabled rules) is declared on one line that ends with `// read only`; the key guard
  honours exactly that marker. Moving a control means moving its `cfg_` declaration, its ids, dialogs, helper
  properties and imports with it, or the control silently stops saving.
- Style gating goes through `StyleKinds` only (`kinds.menuKind`, `kinds.taskbarKind`, `kinds.windowKind`,
  `kinds.anyXp`, `kinds.anyKind("windows12")`, `kinds.taskbarGenerated`, `kinds.win7Family`). No page compares
  `cfg_menuStyle` or `cfg_taskbarLook` directly after its task (Task 13 greps for it).
- Style gating uses `visible`; a dependency on another control of the same page uses `enabled`, and a whole disabled
  block gets one hint line saying why (proposal section 4 "Conventions").
- Labels are exactly the strings of proposal section 3 (noun + colon for fields, a sentence for checks, no questions,
  no "(Windows 7, XP)" tags). Ellipses are the character `…` (U+2026) as in the existing strings. "(applies at once)"
  is ONE msgid: a disabled `QQC2.Label { text: i18n("(applies at once)"); enabled: false }` beside the button.
- Page files: `package/contents/ui/config/Config<Page>.qml`; renames with `git mv` (history). Final set: ConfigStyle,
  ConfigStartMenu, ConfigAppsList, ConfigButton, ConfigTaskbar, ConfigWindows, ConfigAppearance, ConfigSearch,
  ConfigAdvanced, plus StyleKinds, StylePreview, TaskbarLayoutBox. Deleted: ConfigMenu (-> AppsList), ConfigRail
  (-> Button), ConfigLook (-> Appearance), ConfigColours (-> Appearance), ConfigIcons (-> Advanced).
- Repo style: short files, comments that say WHY, one-line `// ponytail:` notes on deliberate shortcuts; read
  `docs/HANDOFF.md` lines 1-40 first. Never read a big file whole (ConfigTaskbar ~400 lines, ConfigStyle ~340, es.po
  ~2700): grep for the anchor, then read small ranges; po work through `msgattrib` filters.
- Every task fits one run: <= ~40 tool calls / ~100k tokens. Out of budget = commit at a safe point (key guard
  shows no LOST), log `status: partial` with the exact step to resume.
- Live desktop: config pages are loaded fresh each time the dialog opens, so `sh install.sh` is enough (no restart). If
  a page still looks old, ONE `systemctl --user restart plasma-plasmashell.service` at most (more than ~5 in a short
  time trips systemd: `systemctl --user reset-failed plasma-plasmashell.service`). Always close the dialog (Cancel)
  after a screenshot; never leave it open. `kscreen-doctor --dpms on` before a screenshot (DPMS-off screenshots are
  transparent). Other agents are not editing `package/contents/ui/config` once the multi-monitor plan is done.
- Commits: one per task, message `settings: <what>` + a short why, ending with the attribution lines the driver prompt
  gives (when it gives none: the two lines of the plan commit).

## Driver protocol (progress log format)

File: `docs/superpowers/plans/2026-10-17-settings-reorg-progress.md`. Each run:

1. Reads the spec (proposal sections 3-4), this plan, and the progress log. The last entry's `next:` line names the
   task to do; `next-model:` names the model the driver must start for it.
2. Does that task's steps, verifies, commits, then appends ONE entry:

```
## <YYYY-MM-DD HH:MM> Task N: <title>
status: done | blocked | partial | skipped
done: <what landed, commit hash>
left: <what is not done, or "nothing">
notes: <facts the next run needs; key-guard output; files>
next: Task M — <title>            (or `next: none` when Task 13 is done)
next-model: opus | sonnet | none
```

3. Exits. The driver reads the LAST `next-model:` line to choose the model for the next run.

Rules: `status: blocked` is for tasks that need something unavailable (the multi-monitor plan not finished, a tool
missing). A blocked task's dependants (see each task's **Depends on**) are `skipped` with `left: blocked by Task N`;
`next:` then points at the next task that does not depend on it. `partial` = out of budget mid-task: the entry's
`next:` names the SAME task and `notes:` says the exact step to resume at (work is committed at a safe point first —
the key guard must show no LOST line — or stashed with `git stash push -m "Task N partial"` and noted). A model name
in a task header (`Model: opus|sonnet`) is what the previous entry's `next-model:` must carry.

## Task order

| # | Model | Task | Depends on |
|---|---|---|---|
| 1 | sonnet | Re-read the Taskbar page after multi-monitor; key guard `tests/test_settings_keys.js` + baseline; dialog screenshot procedure | - |
| 2 | sonnet | `StyleKinds.qml` + offscreen unit test; resolve the three VERIFY sets; Style page uses the helper | 1 |
| 3 | sonnet | Start menu page (new `ConfigStartMenu.qml`) from Style, Layout and Look | 2 |
| 4 | sonnet | Apps list page (`git mv ConfigMenu.qml ConfigAppsList.qml`), groups, hide-list rule, All Programs visibility | 3 |
| 5 | sonnet | Start button & shortcuts (`ConfigRail` merged into `ConfigButton`, XP Start colour from Style) | 2 |
| 6 | sonnet | Windows page (new `ConfigWindows.qml`) from the Taskbar page | 2 |
| 7 | opus | Appearance (`git mv ConfigLook.qml ConfigAppearance.qml` + Colours merged; 12 sliders from Taskbar; `colorMode` merge) | 3 |
| 8 | opus | Taskbar page: groups, one TaskbarLayoutBox (off Style), island settings placed, merged colour rows, shared rules | 1, 6, 7 |
| 9 | sonnet | Advanced page (`git mv ConfigIcons.qml ConfigAdvanced.qml`); XP icons into Style; reset/backup/restore out of Style | 2 |
| 10 | sonnet | Style page: heading, Mix and match / Windows 7 / Windows XP groups, renamed labels | 3, 5, 8, 9 |
| 11 | sonnet | Search page groups; final `config.qml` order/icons; strict key guard in `install.sh`; HANDOFF note; 9 screenshots | 4, 10 |
| 12 | sonnet | Translations: Messages.sh, es.po complete, .mo | 11 |
| 13 | opus | Review against the proposal, fixes, ELI5 + hand-check list for the user | 12 |

## Standard checks (every task runs these; the task says which files and what the key guard must print)

```bash
cd /home/kalaminator/Documents/Start12
# 1. QML syntax/ids of the touched files (unresolved org.kde imports are warnings, not errors; the grep keeps errors only)
/usr/lib/qt6/bin/qmllint <touched .qml files> 2>&1 | grep -E '^Error|error:' | head -5          # expected: nothing
# 2. Logic guards
node tests/test_tiles.js | tail -1                                                              # tiles.js: all checks passed
node tests/test_settings_keys.js | tail -2                                                      # the task's expected lines
QT_QPA_PLATFORM=offscreen /usr/lib/qt6/bin/qml tests/test_stylekinds.qml 2>&1 | grep stylekinds # stylekinds OK (from Task 2)
# 3. Install and look at the page (procedure and coordinates: Task 1's log entry)
sh install.sh 2>&1 | tail -3
journalctl --user --since "-60 s" --no-pager | grep -i 'org.win12\|qml' | grep -iv 'unqualified' | tail -5   # after the dialog opened: no error for the touched page
```

Screenshot procedure (one blocking call; S = `<scratchpad>/settings`, coordinates from Task 1's `notes:`):

```bash
S=/tmp/claude-1000/-home-kalaminator-Documents-Start12/b88e81db-1485-4a3f-ae50-6c8c3972125b/scratchpad/settings; mkdir -p "$S"
kscreen-doctor --dpms on >/dev/null 2>&1
sh -c 'win12-settings; sleep 4; python3 tests/tools/w16mouse.py drag SX,SYk SX,SYk 1; sleep 1; spectacle -b -n -f -o '"$S"'/<page>.png; sleep 1; python3 tests/tools/w16mouse.py drag CX,CY CX,CY 1'
python3 -c "from PIL import Image; Image.open('$S/<page>.png').crop((DX0, DY0, DX1, DY1)).save('$S/<page>-crop.png')"
```
`drag X,Y X,Y 1` = press + release at one point = a click (`w16mouse.py` never clicks on its own). `SX,SYk` = the
sidebar row of the page under test (row k counted from 0 in the CURRENT `config.qml` order), `CX,CY` = the Cancel
button, `D*` = the dialog's box. Then `Read` the crop and check the groups, labels and which rows are visible for the
user's current style. If the dialog did not open where Task 1 saw it, `Read` the full screenshot first and adjust the
numbers (write the new ones in `notes:`).

## Review focus

Inputs the spec implies but names no test for; each has its check in the owning task:

1. Mix and match (`menuLook` differs from `menuStyle`): a Windows 12 card with a Windows 7 menu must show the Windows 7
   group and hide the Windows 12 group — `tests/test_stylekinds.qml` case `mix` (Task 2); pages use `kinds.*` only
   (Task 13 grep).
2. Taskbar set to KDE Plasma (`taskbarFollowsStyle` false, `taskbarTheme` not "custom"): the Windows 12 taskbar group
   and Auto-hide hide, the Plasma hint and the Override check show — `test_stylekinds.qml` cases `mix`/`custom`
   (`taskbarKind === ""`, `taskbarGenerated`), rules in Task 8.
3. The merged `colorMode` combo keeps the stored values (Light = 2, Dark = 1, Automatic = 0, Custom = 3) both ways, and
   Menu colour / Text colour show only for 3 — Task 7's `modes` array, checked by reading the saved value back.
4. No key loses its control at any commit — `node tests/test_settings_keys.js` after every task (LOST = fail);
   `--strict` (no duplicates) from Task 8 on and in `install.sh` from Task 11.
5. "Hide the apps list" disables (does not hide) every list row including `recentHideFolders` and the All Programs
   rows, while style-hidden rows stay hidden — Task 4 rules, seen in the Task 4 screenshot with the box ticked.

## Defaults picked without asking (the user should know)

- File names: `ConfigStartMenu.qml`, `ConfigAppsList.qml`, `ConfigWindows.qml`, `ConfigAppearance.qml`,
  `ConfigAdvanced.qml` are new names; `ConfigButton.qml` keeps its name for "Start button & shortcuts" (the rail page
  is merged into it, not the other way round, because the button page already holds the right-click list).
- Read-only declarations are marked with a trailing `// read only` comment (today's own convention in ConfigTaskbar);
  Task 1 adds the marker where it is missing. That marker is the only thing the key guard understands.
- `resetRequest` is a request channel (Start menu writes "positions|…", Advanced writes "backup|restore|all|…"), so
  the key guard exempts it from the duplicate rule; every other key must end on exactly one page.
- Duplicates are allowed mid-reorg (the guard lists them) and fail only with `--strict`, which Task 8 turns on once the
  last duplicate (TaskbarLayoutBox on Style) is gone.
- Screenshots are read by the agent (the `Read` tool shows PNGs); the sidebar is clicked with `w16mouse.py` at
  coordinates Task 1 records. The driver should not run this plan while the user is using the PC (a dialog opens and
  closes once per task; Task 11 opens it nine times).
- "(applies at once)" is a disabled label beside the button (one msgid), and the Advanced group title carries its own
  "(applied at once, not on Apply)" as the proposal writes it.
- Other `.po` files (ar, de, fr, hi, it, ja, pt_BR, ru, zh_CN) get the new msgids untranslated through
  `Messages.sh`/msgmerge only if the script merges them (today it merges `es.po` only); Spanish is the only language
  completed, as before.
- `docs/FEATURES.md` (the user guide) names the old pages; it is NOT rewritten here. Task 13 lists the lines that
  need the new page names as a follow-up for the user.

---

### Task 1: Re-read the Taskbar page after multi-monitor; key guard + baseline; dialog screenshot procedure

Model: sonnet
Depends on: the multi-monitor progress log ends with `next: none` (else `status: blocked`, `next: Task 1`).

**Files:**
- Create: `tests/test_settings_keys.js`, `tests/settings-keys.txt`
- Modify (comment-only): the `cfg_` declaration lines that are read-only but lack the marker, in
  `package/contents/ui/config/ConfigMenu.qml`, `ConfigColours.qml`, `ConfigTaskbar.qml` (and any the multi-monitor work added)

**Interfaces (produces):** `node tests/test_settings_keys.js [--strict|--baseline]` (exit 1 on LOST, on a `cfg_` with no
key, or on duplicates with `--strict`); the facts in this task's `notes:` (island key names and controls, base commit,
dialog coordinates) that Tasks 8 and every screenshot step consume.

- [ ] **Step 1: record the base and what multi-monitor changed**

```bash
cd /home/kalaminator/Documents/Start12 && git rev-parse --short HEAD
git log --oneline -12 -- package/contents/ui/config package/contents/config/main.xml | cat
git diff b09085e..HEAD --stat -- package/contents/ui/config package/contents/config/main.xml package/contents/config/config.qml | tail -8
grep -n -i 'island\|monitor\|screen' package/contents/config/main.xml | grep '<entry' 
```
For each new `<entry>` (the two island keys; names come from the multi-monitor work): `grep -n 'cfg_<key>' package/contents/ui/config/*.qml`
and read ~15 lines around each hit in `ConfigTaskbar.qml` and `TaskbarLayoutBox.qml` (if the control lives there).
Write down: key name, declaration form (alias or property + handler), control type, label text, its `enabled:`/`visible:`
expression, the id, and the group it sits in. Also `wc -l package/contents/ui/config/*.qml`.

- [ ] **Step 2: write the key guard**

`tests/test_settings_keys.js`:

```js
#!/usr/bin/env node
// Settings reorganisation guard: every main.xml key that has a control on a settings page keeps exactly one.
// A `property <type|alias> cfg_<key>` line on a page = that page edits the key, unless the line says `read only`
// (pages declare the style keys read-only to gate rows; Plasma fills every cfg_ property a page declares).
//   node tests/test_settings_keys.js             LOST keys fail; duplicates are listed (allowed mid-reorg)
//   node tests/test_settings_keys.js --strict    duplicates fail too (install.sh, final state)
//   node tests/test_settings_keys.js --baseline  rewrite tests/settings-keys.txt from the current pages
const fs = require("fs"), path = require("path");
const root = path.join(__dirname, "..");
const xml = fs.readFileSync(path.join(root, "package/contents/config/main.xml"), "utf8");
const keys = [...xml.matchAll(/<entry name="([A-Za-z0-9_]+)"/g)].map(m => m[1]);
// resetRequest is a request channel, not a setting: Start menu ("positions") and Advanced ("backup|restore|all") both write it.
const sharedRequests = ["resetRequest"];
const dir = path.join(root, "package/contents/ui/config");
const owners = {};
for (const f of fs.readdirSync(dir).filter(f => f.endsWith(".qml"))) {
    for (const line of fs.readFileSync(path.join(dir, f), "utf8").split("\n")) {
        const m = line.match(/^\s*property\s+\w+\s+cfg_([A-Za-z0-9_]+)/);
        if (!m || /read only/.test(line)) continue;
        (owners[m[1]] = owners[m[1]] || []).push(f);
    }
}
const edited = keys.filter(k => owners[k]);
const baselineFile = path.join(__dirname, "settings-keys.txt");
if (process.argv.includes("--baseline")) {
    fs.writeFileSync(baselineFile, edited.join("\n") + "\n");
    console.log(`settings keys: baseline written, ${edited.length} keys`);
    process.exit(0);
}
const baseline = fs.readFileSync(baselineFile, "utf8").split("\n").filter(Boolean);
const lost = baseline.filter(k => !owners[k]);
const dups = edited.filter(k => new Set(owners[k]).size > 1 && !sharedRequests.includes(k));
const unknown = Object.keys(owners).filter(k => !keys.includes(k));
const strict = process.argv.includes("--strict");
if (lost.length) console.log("LOST (had a control, none now): " + lost.join(" "));
if (dups.length) console.log(`${dups.length} duplicates: ` + dups.map(k => `${k} (${[...new Set(owners[k])].join(", ")})`).join("; "));
if (unknown.length) console.log("cfg_ without a main.xml key: " + unknown.join(" "));
const fail = lost.length || unknown.length || (strict && dups.length);
console.log(fail ? "settings keys FAIL" : `settings keys OK (${edited.length} keys${dups.length ? ", duplicates allowed" : ""})`);
process.exit(fail ? 1 : 0);
```

- [ ] **Step 3: baseline, then mark the read-only declarations**

Run: `node tests/test_settings_keys.js --baseline && node tests/test_settings_keys.js`
The duplicate line must end up listing exactly the five known duplicates: `accentColor (ConfigColours.qml, ConfigLook.qml)`,
`accentMode (…)`, `colorMode (…)`, `taskbarLayout (ConfigStyle.qml, ConfigTaskbar.qml)`, `taskbarMagnetic (…)`. Every
other listed key is a read-only declaration: on the page that only reads it (no `property alias`, no `cfg_<key> =`
assignment: check with `grep -n 'cfg_<key>' <file>`), append `   // read only` to the declaration line. Known today:
`ConfigMenu.qml` `property string cfg_menuStyle`, `ConfigColours.qml` `property string cfg_menuStyle`,
`ConfigTaskbar.qml` `property bool cfg_taskbarFollowsStyle`; the multi-monitor work may have added more. Re-run until
the output is the five duplicates + `settings keys OK (N keys, duplicates allowed)`, then `--baseline` once more and
note N. If a sixth genuine duplicate exists (both pages assign the key), note it: it is an extra merge for the page task
that owns it.

- [ ] **Step 4: the dialog screenshot procedure, measured once**

```bash
S=/tmp/claude-1000/-home-kalaminator-Documents-Start12/b88e81db-1485-4a3f-ae50-6c8c3972125b/scratchpad/settings; mkdir -p "$S"
kscreen-doctor --dpms on >/dev/null 2>&1
sh -c 'win12-settings; sleep 4; spectacle -b -n -f -o '"$S"'/dialog-full.png'
```
`Read` the full screenshot. Write down (screen pixels): the dialog box `DX0,DY0,DX1,DY1`; the sidebar column centre
`SX`; the centre `SY0` of the first sidebar row and the row pitch `SP` (row k is at `SY0 + k*SP`); the Cancel button
`CX,CY`. Then prove the click path with one blocking call:
`sh -c 'python3 tests/tools/w16mouse.py drag SX,SY6 SX,SY6 1; sleep 1; spectacle -b -n -f -o '"$S"'/dialog-row6.png; sleep 1; python3 tests/tools/w16mouse.py drag CX,CY CX,CY 1'`
(row 6 = Taskbar today) and `Read` the crop of `dialog-row6.png`: the Taskbar page is showing, and afterwards
`spectacle -b -n -f -o "$S"/closed.png` shows no dialog. If `win12-settings` is missing: `ls ~/.local/bin/win12-settings`;
`package/contents/ui/code/setup.sh` installs it (run `sh install.sh` once). If the click does not land, note it and
fall back to `-a` (active window) screenshots of the first page only; later tasks then reorder nothing and check only
what the first page shows — write that fallback in `notes:`.

- [ ] **Step 5: standard checks and commit**

Standard checks 1-2 (`qmllint` on the files whose comments changed; key guard expected: the five duplicates + OK).

```bash
git add tests/test_settings_keys.js tests/settings-keys.txt package/contents/ui/config/*.qml
git commit -m "settings: key guard (tests/test_settings_keys.js) and read-only markers before the page reorganisation"
```
Log entry `notes:` must carry: base commit, N baseline keys, island key names + control facts (Step 1), the
screenshot coordinates, the `config.qml` page order at this point (row index -> page).

---

### Task 2: `StyleKinds.qml` + offscreen unit test; the three VERIFY sets; Style page uses the helper

Model: sonnet
Depends on: 1

**Files:**
- Create: `package/contents/ui/config/StyleKinds.qml`, `tests/test_stylekinds.qml`
- Modify: `package/contents/ui/config/ConfigStyle.qml` (its four `readonly property … menuKind/taskbarKind/windowKind/anyXp` lines, ~42-45, and their users)

**Interfaces (produces):** type `StyleKinds` (same directory as the pages, no import needed) with inputs `menuStyle`,
`menuLook`, `taskbarLook`, `taskbarFollowsStyle`, `windowLook`, `windowTheme`, `taskbarTheme` and outputs `menuKind`,
`taskbarKind`, `windowKind`, `anyXp`, `taskbarGenerated`, `win7Family`, `anyKind(k)`; the three verified style sets in
this task's `notes:` for Tasks 4 and 5.

- [ ] **Step 1: the helper**

`package/contents/ui/config/StyleKinds.qml`:

```qml
import QtQuick

// One rule set for style gating on every settings page (proposal section 4). A page binds its cfg_ values in
// and tests only these; never cfg_menuStyle directly, so mix and match (menuLook etc.) is honoured everywhere.
QtObject {
    property string menuStyle
    property string menuLook
    property string taskbarLook
    property bool taskbarFollowsStyle
    property string windowLook
    property bool windowTheme
    property string taskbarTheme

    readonly property string menuKind: menuLook || menuStyle
    readonly property string taskbarKind: taskbarFollowsStyle ? (taskbarLook || menuStyle) : ""   // "" = the user's own Plasma Style
    readonly property string windowKind: windowTheme ? (windowLook || menuStyle) : ""             // "" = the user's own title bars
    readonly property bool anyXp: menuKind === "xp" || taskbarKind === "xp" || windowKind === "xp"
    readonly property bool taskbarGenerated: taskbarFollowsStyle || taskbarTheme === "custom"
    // The Modern style is the Windows 7 file with a flat look (main.qml styleFile): one family for the 7 options.
    readonly property bool win7Family: menuKind === "windows7" || menuKind === "modern"
    function anyKind(k) { return menuKind === k || taskbarKind === k || windowKind === k; }
}
```

- [ ] **Step 2: the unit test (fails before Step 1's file exists; run it once before to see the failure)**

`tests/test_stylekinds.qml`:

```qml
import QtQuick
import "../package/contents/ui/config"

// Gating rules of the settings pages (proposal section 4). Run:
//   QT_QPA_PLATFORM=offscreen /usr/lib/qt6/bin/qml tests/test_stylekinds.qml 2>&1 | grep stylekinds   -> stylekinds OK
QtObject {
    // Mix and match: Windows 12 card, Windows 7 menu, the user's own Plasma taskbar, Windows 12 title bars.
    property StyleKinds mix: StyleKinds { menuStyle: "windows12"; menuLook: "windows7"; taskbarFollowsStyle: false; windowTheme: true }
    // Plain XP card: everything follows it.
    property StyleKinds xp: StyleKinds { menuStyle: "xp"; taskbarFollowsStyle: true; windowTheme: true }
    // Windows 10 card, own title bars, custom taskbar colour (Override on).
    property StyleKinds custom: StyleKinds { menuStyle: "windows10"; taskbarFollowsStyle: false; windowTheme: false; taskbarTheme: "custom" }
    Component.onCompleted: {
        const checks = [
            mix.menuKind === "windows7", mix.win7Family, mix.taskbarKind === "", mix.windowKind === "windows12",
            !mix.anyXp, mix.anyKind("windows12"), mix.anyKind("windows7"), !mix.anyKind("xp"), !mix.taskbarGenerated,
            xp.menuKind === "xp", xp.taskbarKind === "xp", xp.windowKind === "xp", xp.anyXp, xp.taskbarGenerated, !xp.win7Family,
            custom.menuKind === "windows10", custom.taskbarKind === "", custom.windowKind === "", !custom.anyXp, custom.taskbarGenerated
        ];
        const bad = checks.map((ok, i) => ok ? -1 : i).filter(i => i >= 0);
        console.log(bad.length ? "stylekinds FAIL at " + bad.join(",") : "stylekinds OK");
        Qt.exit(bad.length ? 1 : 0);
    }
}
```
Run: `QT_QPA_PLATFORM=offscreen /usr/lib/qt6/bin/qml tests/test_stylekinds.qml 2>&1 | grep stylekinds` -> `stylekinds OK`.
If `qml` refuses a non-window root, wrap the QtObject in `Item { }` (same contents) — note it.

- [ ] **Step 3: resolve the three VERIFY items by grep (record the sets; they are the rule, the proposal's guesses are not)**

```bash
cd /home/kalaminator/Documents/Start12/package/contents/ui
grep -n 'treeExtras: true' styles/*.qml win10/*.qml        # expected: styles/windows7.qml, styles/xp.qml
grep -n 'styleFile' main.qml | head -3                      # expected: menuKind === "modern" ? "windows7" (modern uses the 7 file)
grep -rl 'Recommended {' styles/ win10/ common/             # expected: styles/windows11.qml, styles/windows12.qml
grep -rln 'smallPlaces' styles/ win10/ common/              # expected: styles/windows7.qml, styles/xp.qml, styles/pro.qml
grep -rln 'powerButtonIcon' styles/ win10/ common/          # expected: styles/windows7.qml
```
Expected sets (a file `windows7.qml` means {windows7, modern}): All Programs group (`autoExpand`, `desktopInTree`,
`allAppsAsMenu`) = `kinds.win7Family || kinds.menuKind === "xp"`; `recentHideFolders` = `kinds.menuKind === "windows11"
|| kinds.menuKind === "windows12"` (wider than the proposal's {windows11}: `common/Recommended.qml` is used by the 12
style too); `smallPlaces` = `kinds.win7Family || kinds.menuKind === "xp" || kinds.menuKind === "pro"`;
`powerButtonIcon` = `kinds.win7Family`. If a grep shows another style file, add that style to the set (rule: show the row
for every style that reads the key). Write the final three expressions in `notes:`.

- [ ] **Step 4: ConfigStyle uses the helper**

In `ConfigStyle.qml` replace the four `readonly property` lines (`menuKind`, `taskbarKind`, `windowKind`, `anyXp`) with

```qml
    StyleKinds {
        id: kinds
        menuStyle: page.cfg_menuStyle; menuLook: page.cfg_menuLook; taskbarLook: page.cfg_taskbarLook
        taskbarFollowsStyle: page.cfg_taskbarFollowsStyle; windowLook: page.cfg_windowLook; windowTheme: page.cfg_windowTheme
    }
```
(placed after the `cfg_` declarations, as a child of the page like the existing dialogs) and
`sed -i 's/page\.\(menuKind\|taskbarKind\|windowKind\|anyXp\)/kinds.\1/g' package/contents/ui/config/ConfigStyle.qml`.
`grep -n 'menuKind\|taskbarKind\|windowKind\|anyXp' package/contents/ui/config/ConfigStyle.qml package/contents/ui/config/StylePreview.qml`
must show only `kinds.` uses (and the helper block). On Style these six keys are EDITED (the cards and the Mix combos
write them), so their declarations keep no `// read only` marker.

- [ ] **Step 5: standard checks, screenshot, commit**

Standard checks (qmllint `StyleKinds.qml ConfigStyle.qml`; key guard: still the five duplicates + OK; stylekinds OK;
`sh install.sh`; screenshot row 0 = Style: the XP / Windows 7 groups show or hide as before for the user's style).

```bash
git add package/contents/ui/config/StyleKinds.qml tests/test_stylekinds.qml package/contents/ui/config/ConfigStyle.qml
git commit -m "settings: StyleKinds helper (one visibility rule set) with an offscreen test; Style page uses it"
```

---

### Task 3: Start menu page (new `ConfigStartMenu.qml`)

Model: sonnet
Depends on: 2

**Files:**
- Create: `package/contents/ui/config/ConfigStartMenu.qml`
- Modify: `ConfigStyle.qml` (the seven size/position rows go), `ConfigMenu.qml` (tiles, design mode, favicons, lock,
  reset positions, the Windows 12 group go), `ConfigLook.qml` (the four Tile rows go), `package/contents/config/config.qml`

**Interfaces (produces):** page 2 "Start menu" owning keys `menuSize menuHeight menuPosition centerTaskbar fullScreen
keepPanelVisible uiScale tileColumns tileGap tileIconScale hideTileText tileColorMode tileColor tileOpacity tileImage
tileImageOpacity webFavicons tilesLocked resetRequest("positions") win12ListSide win12Greeting win12Chips win12Clock`.

- [ ] **Step 1: find every block to move (grep, then read small ranges)**

```bash
cd /home/kalaminator/Documents/Start12/package/contents/ui/config
grep -n 'cfg_menuSize\|cfg_menuHeight\|cfg_menuPosition\|cfg_centerTaskbar\|cfg_fullScreen\|cfg_keepPanelVisible\|cfg_uiScale\|vertical\|sizeBox\|positionBox\|heightSpin\|centerTaskbarBox\|fullScreenBox\|keepPanelBox' ConfigStyle.qml
grep -n 'cfg_tileColumns\|cfg_tileGap\|cfg_tileIconScale\|cfg_hideTileText\|cfg_tilesLocked\|cfg_webFavicons\|cfg_resetRequest\|designRequest\|cfg_win12\|listSide\|greeting\|chips\|clock\|Section dividers\|positions' ConfigMenu.qml
grep -n 'cfg_tile\|tileMode\|tileColor\|tileOpacity\|tileImage\|Tile ' ConfigLook.qml
```
A block = the control bound to the key plus its id, `Kirigami.FormData.label`, dialogs (tile image picker, reset
positions dialog), helper properties (`vertical`, the `P5Support` runner behind "Edit menu layout…"), and imports.

- [ ] **Step 2: write the page**

```qml
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtQuick.Dialogs as Dialogs
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM
import org.kde.kquickcontrols as KQuickControls
import org.kde.plasma.plasma5support as P5Support
import "../code/tiles.js" as Tiles

// Start menu page: size and position of the menu, the tiles, the Windows 12 extras (proposal 3.2).
KCM.SimpleKCM {
    id: page
    property string cfg_menuStyle   // read only
    property string cfg_menuLook   // read only
    property string cfg_taskbarLook   // read only
    property bool cfg_taskbarFollowsStyle   // read only
    property string cfg_windowLook   // read only
    property bool cfg_windowTheme   // read only
    StyleKinds {
        id: kinds
        menuStyle: page.cfg_menuStyle; menuLook: page.cfg_menuLook; taskbarLook: page.cfg_taskbarLook
        taskbarFollowsStyle: page.cfg_taskbarFollowsStyle; windowLook: page.cfg_windowLook; windowTheme: page.cfg_windowTheme
    }
    // <the moved cfg_ declarations, verbatim from their old pages>

    Kirigami.FormLayout {
        Kirigami.Separator { Kirigami.FormData.isSection: true; Kirigami.FormData.label: i18n("Size and position") }
        // Size:, Custom height:, Position:, Also align…, Full screen menu, Keep the panel visible, Menu scale:
        Kirigami.Separator { Kirigami.FormData.isSection: true; Kirigami.FormData.label: i18n("Tiles") }
        RowLayout {
            QQC2.Button { text: i18n("Edit menu layout…"); onClicked: /* the moved designRequest runner */ }
            QQC2.Label { text: i18n("(applies at once)"); enabled: false }
        }
        // Tile columns:, Space between tiles:, Tile icon size:, Hide tile captions…, Tile colour:, Tile background:,
        // Tile image:, Tile image opacity:, Fetch favicons…, Lock tile and pin positions, Reset tile and pin positions…
        Kirigami.Separator { Kirigami.FormData.isSection: true; Kirigami.FormData.label: i18n("Windows 12"); visible: kinds.menuKind === "windows12" }
        // Apps list side:, Greeting with picture:, Show Documents…, Show the clock… (each `visible: kinds.menuKind === "windows12"`)
    }
    // <the moved dialogs: tile image picker, reset positions confirmation>
}
```
Labels (exact): `Size:`, `Custom height:`, `Position:`, `Also align the Start button and taskbar icons with the menu`
(the default text; the Right-position and vertical-panel variants keep today's strings), `Full screen menu`,
`Keep the panel visible` (`enabled: fullScreenBox.checked`), `Menu scale:`, `Tile columns:`, `Space between tiles:`,
`Tile icon size:`, `Hide tile captions (show them as tooltips)`, `Tile colour:`, `Tile background:`, `Tile image:`,
`Tile image opacity:` (`enabled` when an image is set, as today), `Fetch favicons for pinned web links`,
`Lock tile and pin positions`, `Reset tile and pin positions…`, `Apps list side:`, `Greeting with picture:`,
`Show Documents, Pictures and Music above the cards`, `Show the clock and date above the cards`. The centre-taskbar
row keeps `visible: positionBox.currentIndex !== 0`. `webFavicons` loses any `enabled: !hideList.checked` it had
(it is a pin option). Drop the `Section dividers:` label and the `Windows 12 style` title text.

- [ ] **Step 3: remove the moved blocks from the three source pages**

Delete the moved controls, their `cfg_` declarations, dialogs and helper properties from `ConfigStyle.qml`,
`ConfigMenu.qml`, `ConfigLook.qml`; keep imports they still need. `ConfigMenu.qml` keeps `cfg_hideAppList` and the
list rows (Task 4 handles them). `config.qml`: insert after the Style category

```qml
    ConfigCategory {
        name: i18n("Start menu")
        icon: "view-grid"
        source: "config/ConfigStartMenu.qml"
    }
```
and change the "Layout" category's icon to `view-list-details` (its rename is Task 4).

- [ ] **Step 4: standard checks, screenshot, commit**

qmllint `ConfigStartMenu.qml ConfigStyle.qml ConfigMenu.qml ConfigLook.qml`; key guard expected: the same five
duplicates, no LOST, `settings keys OK`; `sh install.sh`; screenshot row 1 (Start menu): three groups, the Windows 12
group only when the user's menu is Windows 12. `grep -c 'cfg_' ConfigStartMenu.qml` is 6 + 23.

```bash
git add package/contents/ui/config/ConfigStartMenu.qml package/contents/ui/config/ConfigStyle.qml package/contents/ui/config/ConfigMenu.qml package/contents/ui/config/ConfigLook.qml package/contents/config/config.qml
git commit -m "settings: Start menu page (size, position, tiles, Windows 12 extras in one place)"
```

---

### Task 4: Apps list page (`git mv ConfigMenu.qml ConfigAppsList.qml`)

Model: sonnet
Depends on: 3

**Files:**
- Rename: `package/contents/ui/config/ConfigMenu.qml` -> `ConfigAppsList.qml`
- Modify: `ConfigAppsList.qml`, `package/contents/config/config.qml`

**Interfaces (produces):** page 3 "Apps list" owning `hideAppList listIconSize listRowHeight showRecentlyAdded
showMostUsed highlightNew categoryFolders foldersAtBottom recentHideFolders autoExpand desktopInTree allAppsAsMenu
hiddenApps filteredFolders`.

- [ ] **Step 1: rename and gate**

`git mv package/contents/ui/config/ConfigMenu.qml package/contents/ui/config/ConfigAppsList.qml`. Replace the
`property string cfg_menuStyle   // read only` line with the six read-only declarations + the `StyleKinds { id: kinds … }`
block exactly as in Task 3 Step 2. Header comment: `// Apps list page: what the list shows, its sizes, the 7/XP All
Programs options, hidden apps, filtered folders (proposal 3.3).`

- [ ] **Step 2: groups and rules (grep each id first: `grep -n 'hideList\|listIcon\|rowHeight\|recent\b\|mostUsed\|highlightNew\|folders\b\|foldersBottom\|hideRecentFolders\|autoExpand\|desktopInTree\|allAppsAsMenu\|isSection' ConfigAppsList.qml`)**

Group `Apps list`: `Hide the apps list (tiles only)` first, then `Icon size:` (was "Shortcut icons should be sized:"),
`Row height:`, `Show recently added apps`, `Show my most used applications`, `Highlight new applications`,
`Show app categories as folders`, `Place folders at the bottom of the list` (`enabled: folders.checked && !hideList.checked`),
`Hide folders in the recent list` (`visible: kinds.menuKind === "windows11" || kinds.menuKind === "windows12"` — Task 2's
set — and `enabled: !hideList.checked`). Group `All Programs (Windows 7 and XP)` with `visible: kinds.win7Family ||
kinds.menuKind === "xp"` on the separator AND on each of its three rows: `Auto expand folders on hover`,
`Show the Desktop folder in All Programs`, `Show categories as a cascading menu` (`enabled: folders.checked && !hideList.checked`);
the other two `enabled: !hideList.checked`. Every row of both groups except the Hide box itself has `enabled: !hideList.checked`.
Groups `Hidden apps` and `Filtered folders` stay as they are (labels unchanged, the hint "None. Right-click an app…" kept).

- [ ] **Step 3: config.qml**

The former "Layout" category becomes `name: i18n("Apps list")`, `icon: "view-list-details"`,
`source: "config/ConfigAppsList.qml"`.

- [ ] **Step 4: standard checks, screenshot, commit**

qmllint `ConfigAppsList.qml`; key guard: five duplicates, no LOST, OK; `grep -c 'Windows 7, XP\|Windows 11 style' ConfigAppsList.qml`
= 0; `sh install.sh`; screenshot row 2: four groups, All Programs only for 7/Modern/XP; tick nothing (the user's config
is not changed by the agent).

```bash
git add -A package/contents/ui/config/ConfigMenu.qml package/contents/ui/config/ConfigAppsList.qml package/contents/config/config.qml
git commit -m "settings: Apps list page (list rows, All Programs hidden by style, hide-list disables every row)"
```

---

### Task 5: Start button & shortcuts (`ConfigRail` merged into `ConfigButton`; XP Start colour from Style)

Model: sonnet
Depends on: 2

**Files:**
- Modify: `package/contents/ui/config/ConfigButton.qml`, `ConfigStyle.qml` (XP Start button colour rows go), `package/contents/config/config.qml`
- Delete: `package/contents/ui/config/ConfigRail.qml` (its content moves into ConfigButton)

**Interfaces (produces):** page 4 "Start button & shortcuts" owning `buttonMode buttonIcon buttonIconScale buttonImage
squaresAccent squaresColor xpStartGreen xpStartCustom xpStartColor buttonMenuItems railMode railIconSize
railAlignBottom smallPlaces railItems powerButtonAction powerItems powerButtonIcon`.

- [ ] **Step 1: read the three sources in ranges**

`grep -n 'property\|isSection\|FormData.label\|^import\|id:' ConfigButton.qml ConfigRail.qml | head -80` and
`grep -n 'xpStart' ConfigStyle.qml`. Check id clashes between the two pages (`grep -ho 'id: [a-zA-Z0-9_]*' ConfigButton.qml ConfigRail.qml | sort | uniq -d`);
rename the ConfigRail side if any.

- [ ] **Step 2: ConfigButton gets the groups**

Add the six read-only declarations + `StyleKinds { id: kinds … }` (Task 3 Step 2 block). Union of imports (ConfigRail
brings `org.kde.iconthemes as KIconThemes`). Header: `// Start button & shortcuts: the button and its right-click menu,
the left-hand rail, the power button and menu (proposal 3.4).`

Group `Start button`: the mode control labelled `Start button:` with choices `Icon` / `Picture` / `Four squares`
(drop the question and the "Start11 button image" wording; values unchanged); `Icon:` (`visible: page.cfg_buttonMode === 2`;
the first entry's text `System default` replaces the hard-coded "Arch Linux (default)", value unchanged); `Icon size:`
(`visible` 2); `Picture:` grid (`visible` 0); `Use a custom picture…` (`visible` 0; keeps showing the file name when one
is set); hint `A custom picture holds 3 images stacked top to bottom: normal, mouse over, pressed.` (`visible` 0);
two labelled colour buttons `First square:` (`squaresAccent`) and `Other squares:` (`squaresColor`), `visible` 1;
`XP Start button colour:` = the `xpStartGreen`/`xpStartCustom`/`xpStartColor` controls moved from ConfigStyle with their
declarations and dialog, `visible: kinds.menuKind === "xp"`.
Group `Right-click menu`: the nine checks under one label `Show in the right-click menu:`.
Group `Menu shortcuts`: `Shortcut labels:` (was "Decide how shortcut icons look:"), `Icon size:`,
`Align the icons to the bottom` (`visible: kinds.menuKind === "windows10"`), `Small place rows`
(`visible: kinds.win7Family || kinds.menuKind === "xp" || kinds.menuKind === "pro"` — Task 2's set), `Top to bottom:` list,
the add combo + `Add`, `Add custom location…` — all moved verbatim from ConfigRail.
Group `Power`: `Clicking the power button:` (was "…should:"), `Show in the power menu:` (7 checks),
`Show an icon on the Shut down button` (`visible: kinds.win7Family`).

- [ ] **Step 3: delete ConfigRail, trim Style, config.qml**

`git rm package/contents/ui/config/ConfigRail.qml`. Remove the XP Start colour rows and their `cfg_xpStart*` declarations
and dialog from `ConfigStyle.qml`. `config.qml`: delete the "Shortcuts" category; the "Start Button" category becomes
`name: i18n("Start button & shortcuts")` (icon stays `start-here-kde`) and moves to right after "Apps list".

- [ ] **Step 4: standard checks, screenshot, commit**

qmllint `ConfigButton.qml ConfigStyle.qml`; key guard: five duplicates, no LOST, OK; `grep -c 'Windows 7\|Windows 10\|Arch Linux\|Start11' ConfigButton.qml` = 0;
`sh install.sh`; screenshot row 3: four groups; the XP colour row only for an XP menu.

```bash
git add -A package/contents/ui/config/ConfigButton.qml package/contents/ui/config/ConfigRail.qml package/contents/ui/config/ConfigStyle.qml package/contents/config/config.qml
git commit -m "settings: Start button & shortcuts page (button, right-click menu, rail and power together)"
```

---

### Task 6: Windows page (new `ConfigWindows.qml`) from the Taskbar page

Model: sonnet
Depends on: 2

**Files:**
- Create: `package/contents/ui/config/ConfigWindows.qml`
- Modify: `package/contents/ui/config/ConfigTaskbar.qml` (Window frames block and the XP Firefox row go), `package/contents/config/config.qml`

**Interfaces (produces):** page 6 "Windows" owning `windowOpacity windowBlur windowRadius titleHeight windowImage
windowImageMode windowImageAlpha windowImageTint xpFirefoxTitlebar`; reads `windowTheme` + the style keys.

- [ ] **Step 1: locate the block**

`grep -n 'Window frames\|winAlpha\|winBlur\|ownRadius\|cfg_windowRadius\|cfg_titleHeight\|cfg_windowImage\|winImage\|ffBox\|xpFirefox\|Dialogs\.' ConfigTaskbar.qml`
then read the ranges (the `Window frames:` group, its picture dialog, the XP Firefox row).

- [ ] **Step 2: the page**

Same skeleton as Task 3 Step 2 (imports: QtQuick, Controls, Layouts, Dialogs, Kirigami, KCM, KQuickControls if the
picture rows use it, `../code/tiles.js` if `shQuote` is used), header `// Windows page: title bars and frames; the XP
Firefox title bar (proposal 3.6).`, declarations: the six read-only keys, `property bool cfg_windowTheme   // read only`
(already in the six), the nine moved declarations.

```qml
    Kirigami.FormLayout {
        Kirigami.Separator { Kirigami.FormData.isSection: true; Kirigami.FormData.label: i18n("Title bars and frames") }
        QQC2.Label {
            visible: !page.cfg_windowTheme
            text: i18n("Pick a Windows title bar on the Style page to change these.")
            enabled: false
        }
        // the eight rows, each `enabled: page.cfg_windowTheme` (keep their own rules too):
        // Opacity: (winAlpha) | Blur behind translucent frames (enabled: page.cfg_windowTheme && winAlpha.value < 100)
        // Corner radius: (check + spin) | Title bar height: (check + spin) | Title bar picture: | Placement: | Picture opacity: | Tint with the accent colour
        Kirigami.Separator { Kirigami.FormData.isSection: true; Kirigami.FormData.label: i18n("Windows XP"); visible: kinds.windowKind === "xp" }
        QQC2.CheckBox { id: ffBox; visible: kinds.windowKind === "xp"; text: i18n("Use the system title bar in Firefox") }
    }
```
Labels: `Opacity:` (the unlabelled slider gets it), `Blur behind translucent frames`, `Corner radius:`, `Title bar height:`,
`Title bar picture:`, `Placement:`, `Picture opacity:`, `Tint with the accent colour` (the last three `enabled` only when a
picture is set, as today).

- [ ] **Step 3: trim Taskbar, register**

Remove the block, the XP Firefox row, their declarations and the picture dialog from `ConfigTaskbar.qml` (keep
`cfg_windowTheme` only if something left on the page still reads it; else delete). `config.qml`: add after "Taskbar"

```qml
    ConfigCategory {
        name: i18n("Windows")
        icon: "preferences-system-windows"
        source: "config/ConfigWindows.qml"
    }
```

- [ ] **Step 4: standard checks, screenshot, commit**

qmllint `ConfigWindows.qml ConfigTaskbar.qml`; key guard: five duplicates, no LOST, OK; `sh install.sh`; screenshot
(row index of "Windows" in the current config.qml): one group, disabled with the hint when the user's title bars are
Plasma's own.

```bash
git add package/contents/ui/config/ConfigWindows.qml package/contents/ui/config/ConfigTaskbar.qml package/contents/config/config.qml
git commit -m "settings: Windows page (title bars and frames leave the Taskbar page)"
```

---

### Task 7: Appearance (`git mv ConfigLook.qml ConfigAppearance.qml`, Colours merged, 12 sliders from Taskbar)

Model: opus
Depends on: 3

**Files:**
- Rename: `package/contents/ui/config/ConfigLook.qml` -> `ConfigAppearance.qml`
- Delete: `package/contents/ui/config/ConfigColours.qml` (content merged)
- Modify: `ConfigAppearance.qml`, `ConfigTaskbar.qml` (`win12Light win12Dark win12Transparency win12Blur win12Noise` + their hint go), `package/contents/config/config.qml`

**Interfaces (produces):** page 7 "Appearance" owning `colorMode menuColor textColor accentMode accentColor recentAccents
win10AccentStart win10AccentTitles win12Light win12Dark win10Transparency solidMenu menuOpacity win12Transparency
win12Blur win12Noise cornerRadius texture textureMode textureOpacity darkenTexture colorizeTexture fontFamily fontSize
headerFontFamily headerFontSize itemFontSize animateMenu greyIcons`. After this task the key guard lists only
`taskbarLayout` and `taskbarMagnetic` as duplicates.

- [ ] **Step 1: read the merge points**

```bash
cd /home/kalaminator/Documents/Start12/package/contents/ui/config
grep -n 'cfg_colorMode\|cfg_accentMode\|cfg_accentColor\|colorMode\b\|accentMode\b\|accentColor\b\|model:' ConfigLook.qml | head -30
grep -n 'modes\|cfg_\|pick(\|FormData.label\|isSection\|visible:' ConfigColours.qml
grep -n 'cfg_win12Light\|cfg_win12Dark\|cfg_win12Transparency\|cfg_win12Blur\|cfg_win12Noise\|How light\|How dark\|Blur strength\|Grain\|Taskbar, Start menu and title bars' ConfigTaskbar.qml
```
Read ConfigLook's `colorMode` combo (its `model` order IS the stored value: index 0..3) and ConfigColours' `modes`
array and `pick()` function in full (small ranges).

- [ ] **Step 2: rename, merge the colour block**

`git mv ConfigLook.qml ConfigAppearance.qml`. Header `// Appearance: colours, Windows 12 shade, transparency and corners,
texture, fonts, effects (proposal 3.7).` Add the six read-only declarations + `StyleKinds { id: kinds … }` (Task 3 Step 2
block). Replace ConfigLook's `property alias cfg_colorMode`, `cfg_accentMode`, `cfg_accentColor` with ConfigColours'
plain declarations (`property int cfg_colorMode`, `property int cfg_accentMode`, `property color cfg_accentColor`,
`property var cfg_recentAccents: []`, `property bool cfg_win10AccentStart`, `cfg_win10AccentTitles`, `cfg_win10Transparency`)
and move ConfigColours' `current` property, `pick()` function, swatch delegate and its rows in. The mode combo:

```qml
        QQC2.ComboBox {
            id: colorMode
            Kirigami.FormData.label: i18n("Mode:")
            // Stored values (main.xml colorMode): take the four texts from ConfigLook's combo, whose index was the value;
            // shown in the Colours page's order. modes[index] = stored value.
            readonly property var modes: [2, 1, 0, 3]
            model: [i18n("Light"), i18n("Dark"), i18n("Automatic (Plasma colours)"), i18n("Custom colours")]
            currentIndex: Math.max(0, modes.indexOf(page.cfg_colorMode))
            onActivated: index => page.cfg_colorMode = modes[index]
        }
```
Confirm `modes` against ConfigLook's model order before writing it (if ConfigLook's index 2 is not "Light", fix the
array so the TEXT keeps its old value). `Menu colour:` and `Text colour:` get `visible: page.cfg_colorMode === 3`.
`Accent colour:` = ConfigColours' combo (Automatic (Plasma accent colour) / Custom) writing `cfg_accentMode`; ConfigLook's
separate accent colour button is dropped (the swatches / `View colours` do it). Then `Recent colours:` (`visible:
page.cfg_recentAccents.length > 0`), `Windows colours:` (48 swatches, `pick()` still sets `accentMode = 1` and
`recentAccents`), `Custom colours:` `View colours`, the two `Show accent colour on …` checks, the kept hint.

- [ ] **Step 3: the groups**

```
Colours                     visible: kinds.menuKind !== "xp"; else one line: "Windows XP uses its own colour scheme (Style page)."
Windows 12 shade            visible: kinds.anyKind("windows12") -- Light mode tint: (win12Light), Dark mode shade: (win12Dark), moved from Taskbar with their slider code
Transparency and corners    Transparency effects (visible: kinds.menuKind !== "xp"), Solid Start menu, Start menu background: (enabled: !solid.checked),
                            Windows 12 transparency: / Windows 12 blur: / Windows 12 grain: (visible: kinds.anyKind("windows12")) + the moved hint
                            "Taskbar, Start menu and title bars; …" (same visible), Menu corner radius:
Texture                     Texture: (combo + Pick custom texture…), Placement:, Fade texture:, Darken texture, Recolour the texture with the menu colour, preview
Fonts                       Menu font:, Font size:, Header font:, Header font size:, Item font size:
Effects                     Fade the menu in when it opens, Show app icons in grey
```
Sliders keep their ranges and end texts (`Soft tint … Near white`, `Soft dark … Near black`, `Solid … Clear`,
`Soft … Strong`, `Smooth … Grainy`; the field labels are the new part). The five `win12*` sliders in ConfigTaskbar are
`property int` + handlers: move the declarations and the slider blocks verbatim; check with
`grep -n 'w12GapMode' ConfigTaskbar.qml` that none of them was tied to the gap mode (the proposal says they are
independent; if one is, keep that `enabled:` rule out — note it).

- [ ] **Step 4: delete Colours, trim Taskbar, config.qml**

`git rm ConfigColours.qml`; remove the five sliders, their declarations and the hint from `ConfigTaskbar.qml`.
`config.qml`: "Look" becomes `name: i18n("Appearance")`, `icon: "preferences-desktop-color"`,
`source: "config/ConfigAppearance.qml"`; delete the "Colours" category.

- [ ] **Step 5: standard checks, value round trip, screenshot, commit**

qmllint `ConfigAppearance.qml ConfigTaskbar.qml`; key guard expected: `2 duplicates: taskbarLayout (ConfigStyle.qml,
ConfigTaskbar.qml); taskbarMagnetic (…)` + OK, no LOST; `sh install.sh`; screenshot (row of "Appearance"): six groups
(Colours replaced by the XP line for an XP menu). Round trip of the merged combo without touching the user's config:
`kreadconfig6 --file plasma-org.kde.plasma.desktop-appletsrc --group Containments --group <id> --group Applets --group <id> --group Configuration --group General --key colorMode`
(find the ids with `grep -n 'org.win12.menu' ~/.config/plasma-org.kde.plasma.desktop-appletsrc -B3 | grep '\[' | head`) and
check the screenshot shows the matching text (2 Light, 1 Dark, 0 Automatic, 3 Custom).

```bash
git add -A package/contents/ui/config/ConfigLook.qml package/contents/ui/config/ConfigAppearance.qml package/contents/ui/config/ConfigColours.qml package/contents/ui/config/ConfigTaskbar.qml package/contents/config/config.qml
git commit -m "settings: Appearance page (Look + Colours merged, one colour mode combo, Windows 12 sliders moved in)"
```

---

### Task 8: Taskbar page — groups, one TaskbarLayoutBox, island settings placed, merged colour rows, shared rules

Model: opus
Depends on: 1, 6, 7

**Files:**
- Modify: `package/contents/ui/config/ConfigTaskbar.qml`, `ConfigStyle.qml` (TaskbarLayoutBox and `cfg_taskbarLayout`/`cfg_taskbarMagnetic` go), `TaskbarLayoutBox.qml` (field label only)

**Interfaces (produces):** page 5 "Taskbar" owning `taskbarLayout taskbarMagnetic <two island keys from Task 1> taskbarFloating
taskbarGap taskbarWindows menuGap taskbarPopAnim taskbarRound taskbarTheme taskbarColor taskbarAlpha taskbarRadius
taskbarLine taskbarButtons taskbarGradient taskbarGradientStrength taskbarImage taskbarImageMode taskbarImageAlpha
taskbarImageTint taskbarFocusedSolid taskbarHiding taskbarSearch taskbarSearchWidth`. From here the key guard runs
`--strict` and must print `settings keys OK (N keys)`.

- [ ] **Step 1: read what is left (after Tasks 6 and 7 the file is shorter) and Task 1's island notes**

`grep -n 'property\|isSection\|FormData.label\|visible:\|enabled:\|w12GapMode\|TaskbarLayoutBox\|Bar colour\|Custom taskbar\|Override\|Edit panel\|handled by Plasma' ConfigTaskbar.qml`
and read the groups in ranges. Read the island controls' block (Task 1 `notes:` has ids and keys).

- [ ] **Step 2: rules**

Replace the read-only `cfg_menuStyle`/`cfg_taskbarLook`/`cfg_taskbarFollowsStyle` lines with the six read-only
declarations + `StyleKinds { id: kinds; …; taskbarTheme: page.cfg_taskbarTheme }` (Task 3 Step 2 block plus the
`taskbarTheme` binding; `cfg_taskbarTheme` stays an EDITED key here — the Override check writes "custom").
`readonly property bool w12GapMode: kinds.taskbarKind === "windows12" && w12Float.checked`.
`readonly property bool customColour: <today's enabled expression of tbColor, unchanged>` (name it once; use it below).

- [ ] **Step 3: the page, top to bottom**

```
hint   visible: kinds.taskbarKind === ""   "Your own Plasma Style is in use. Pick a Windows taskbar on the Style page to change its layout and colours."  (enabled: false)
Windows 12 taskbar            visible: kinds.taskbarKind === "windows12" (separator and every row)
  Layout:                     TaskbarLayoutBox { cfgPage: page; Kirigami.FormData.label: i18n("Layout:") }  (the ONE instance; its hints, Magnetic islands and the red warning are inside it)
  Show islands on:            the multi-monitor control, enabled: page.cfg_taskbarLayout !== "single"   (keep the control as built; adjust only this rule if it lacks one)
  Open apps on other monitors: the multi-monitor control, enabled when "Show islands on" = All monitors (as built)
  Floating bar                w12Float
  Bottom spacing:             enabled: w12Float.checked
  Maximised windows:          w12Windows (texts Fill the screen / Keep a gap above the taskbar) + its hint, enabled: w12Float.checked
  Menu gap:
  Pop-up animation:           enabled: w12Float.checked; visible: !w12GapMode (as today)
  Corner roundness:           the Square … Round slider gets Kirigami.FormData.label: i18n("Corner roundness:")
Taskbar colour and texture    visible: kinds.taskbarGenerated || !page.cfg_taskbarFollowsStyle
  Use a custom taskbar colour instead of your Plasma Style   (the Override check; visible: !page.cfg_taskbarFollowsStyle)
  hint "Available with a custom taskbar colour."             visible: !customColour  (enabled: false)
  Colour: (tbColor)  Opacity: (tbAlpha)  Corner radius: (tbRadius)  Light line on the taskbar edge (tbLine)  Task buttons:   -- all enabled: customColour
  Gradient: (combo + strength)  Texture:  Placement: (mode combo, enabled when an image is set)  Texture opacity:  Tint with the accent colour  Paint the active button less transparent
Behaviour
  Auto-hide: + its hint        visible: kinds.taskbarKind !== "" && kinds.taskbarKind !== "plasma" && !w12GapMode
  Show a search box on the taskbar   (searchBox)      Search box width:   enabled: searchBox.checked
Plasma panel
  hint "Panel height, position and task grouping are set by Plasma itself."
  RowLayout { QQC2.Button { text: i18n("Edit panel…") … }  QQC2.Label { text: i18n("(applies at once)"); enabled: false } }
```
MERGE: the 12 group's `Bar colour:` row (second use of `tbColor`/`tbAlpha`) and its hint `Colour and opacity follow …`
are deleted; `Colour:`/`Opacity:` under "Taskbar colour and texture" are the only ones. The old opening label
`The taskbar and window looks are picked on the Style page.` is replaced by the conditional hint above. The old closing
label with "floating, auto-hide" is replaced by the Plasma panel hint.

- [ ] **Step 4: Style loses its TaskbarLayoutBox**

In `ConfigStyle.qml` delete the `TaskbarLayoutBox { … }` block and the `property string cfg_taskbarLayout` /
`property bool cfg_taskbarMagnetic` declarations (`grep -n 'taskbarLayout\|taskbarMagnetic\|TaskbarLayoutBox' ConfigStyle.qml`
-> no hits after). `TaskbarLayoutBox.qml`'s header comment: "shown on the Taskbar page" (one instance).

- [ ] **Step 5: standard checks (strict from now), screenshot, commit**

qmllint `ConfigTaskbar.qml ConfigStyle.qml TaskbarLayoutBox.qml`; `node tests/test_settings_keys.js --strict | tail -1`
-> `settings keys OK (N keys)`; `sh install.sh`; screenshot (row of "Taskbar"): for the user's current taskbar the
right groups show; the red patched-KDE warning appears at most once on the whole dialog.

```bash
git add package/contents/ui/config/ConfigTaskbar.qml package/contents/ui/config/ConfigStyle.qml package/contents/ui/config/TaskbarLayoutBox.qml
git commit -m "settings: Taskbar page in four groups, one layout box, island settings placed, colour rows merged"
```

---

### Task 9: Advanced page (`git mv ConfigIcons.qml ConfigAdvanced.qml`); XP icons into Style; reset/backup/restore out of Style

Model: sonnet
Depends on: 2

**Files:**
- Rename: `package/contents/ui/config/ConfigIcons.qml` -> `ConfigAdvanced.qml`
- Modify: `ConfigAdvanced.qml`, `ConfigStyle.qml`, `package/contents/config/config.qml`

**Interfaces (produces):** page 9 "Advanced" owning `resetRequest` (backup|restore|all) and the system actions (icon
theme, KNewStuff buttons, wallpaper; no keys); Style gains `xpIcons xpIconTheme`.

- [ ] **Step 1: locate**

`grep -n 'cfg_\|xpThemes\|xpBox\|XP icon\|No XP icon\|isSection\|FormData.label\|NewStuff\|wallpaper\|run(' ConfigIcons.qml`
and `grep -n 'resetRequest\|Reset\|Backup\|Restore\|MessageDialog\|FileDialog\|isSection' ConfigStyle.qml`.

- [ ] **Step 2: XP icon rows move to Style's XP block**

Move `cfg_xpIcons`, `cfg_xpIconTheme`, the `xpThemes` model (and the DataSource/`run` helper that fills it, if any),
the two rows and the hint into `ConfigStyle.qml` right after the `Colour scheme:` row, all `visible: kinds.anyXp`:
`Use an XP icon theme while XP is active` (xpBox), `XP icon theme:` (`enabled: xpBox.checked`), hint
`No XP icon theme is installed. Get one on the Advanced page (Download icon themes…, search for "XP").` (visible under
today's condition: `page.cfg_xpIconTheme === "" && page.xpThemes.length === 0`, and `kinds.anyXp`).

- [ ] **Step 3: Advanced**

`git mv ConfigIcons.qml ConfigAdvanced.qml`; header `// Advanced: system icon theme and wallpaper (apply at once), backup,
restore, reset (proposal 3.9).` Group `Icons and wallpaper (applied at once, not on Apply)`: `Icon theme:` (runs
plasma-changeicons, unchanged), `Download icon themes…` (the `Get more:` label dropped), `Download wallpapers…`,
`Wallpaper:` row = `Choose image…` + `Set as wallpaper` + the chosen path (was "Your own wallpaper:"). Group
`Backup and reset`: `Backup settings…`, `Restore settings…`, `Reset all Win12 settings…` moved from ConfigStyle with
`property string cfg_resetRequest`, their two FileDialogs and the MessageDialog, and the hint `Settings, tiles and pinned
icons go into one file. After picking the file, press OK/Apply …` (today's text, kept). Remove those from
`ConfigStyle.qml` (`grep -c 'resetRequest' ConfigStyle.qml` -> 0). `config.qml`: "Icons" becomes `name: i18n("Advanced")`,
`icon: "configure"`, `source: "config/ConfigAdvanced.qml"`, last in the list.

- [ ] **Step 4: standard checks, screenshot, commit**

qmllint `ConfigAdvanced.qml ConfigStyle.qml`; `--strict` key guard OK; `sh install.sh`; screenshot (last row): two
groups; Style row 0: XP rows only for an XP style.

```bash
git add -A package/contents/ui/config/ConfigIcons.qml package/contents/ui/config/ConfigAdvanced.qml package/contents/ui/config/ConfigStyle.qml package/contents/config/config.qml
git commit -m "settings: Advanced page (icons, wallpaper, backup and reset); XP icon rows join the Style page"
```

---

### Task 10: Style page — heading, Mix and match / Windows 7 / Windows XP groups, renamed labels

Model: sonnet
Depends on: 3, 5, 8, 9

**Files:**
- Modify: `package/contents/ui/config/ConfigStyle.qml`

**Interfaces (produces):** page 1 "Style" final: cards (`menuStyle`), Mix and match (`menuLook`, `taskbarLook`,
`taskbarFollowsStyle`, `xpTaskbarTheme`, `windowLook`, `windowTheme`), Windows 7 (`win7Substyle`, `modernSubstyle`, the
Look combo), Windows XP (`xpSubstyle`, `xpColor`, `xpCustomColor`, `xpIcons`, `xpIconTheme`).

- [ ] **Step 1: what is left**

`grep -n 'isSection\|FormData.label\|i18n(\|visible:' ConfigStyle.qml | head -70`. Expect: heading text, cards, the
Mix combos + hint, Look/Substitle rows, XP rows, nothing else (Tasks 3/5/8/9 removed the rest — if a leftover row is
found, it belongs to the page the proposal names: move it, do not delete it; the key guard tells).

- [ ] **Step 2: texts and groups**

Heading: the title `Style` and one line `A style sets the Start menu, the taskbar and the window title bars together.
Mix them below.` replace the question. Separators: `Mix and match` (rows `Start menu:`, `Taskbar:`,
`Window title bars:` (was "Windows:"), hint `KDE Plasma (default): your own Plasma Style and title bars come back.` kept);
`Windows 7` with `visible: kinds.win7Family` (rows `Menu look:` Classic / Modern (was "Look:"), `Windows 7 list:`
(`visible: kinds.menuKind === "windows7"`), `Modern list:` (`visible: kinds.menuKind === "modern"`)); `Windows XP` with
`visible: kinds.anyXp` (rows `XP theme:` (was "Substyle:"), `Colour scheme:`, the Task 9 icon rows). Remove the old
`Reset`/`Backup` separators if Task 9 left them. `grep -c 'Substyle\|Which style would you prefer' ConfigStyle.qml` -> 0.

- [ ] **Step 3: standard checks, screenshot, commit**

qmllint `ConfigStyle.qml`; `--strict` key guard OK; `sh install.sh`; screenshot row 0.

```bash
git add package/contents/ui/config/ConfigStyle.qml
git commit -m "settings: Style page in three groups with one voice for the labels"
```

---

### Task 11: Search page groups; final `config.qml`; strict guard in `install.sh`; HANDOFF note; all 9 screenshots

Model: sonnet
Depends on: 4, 10

**Files:**
- Modify: `package/contents/ui/config/ConfigSearch.qml`, `package/contents/config/config.qml`, `install.sh` (line 6), `docs/HANDOFF.md` (one short paragraph)

- [ ] **Step 1: Search groups**

`Results`: `Group results by type (Apps, Files, Settings …)`, `Classic search: dense one-line results, no Best match card`,
`Show filter icons above the results (Apps, Documents, Web …)`, `Show the "Search the internet" button` (was "Show search
the internet button"), `Search the internet with:` + `%s is replaced by what you typed.` (`enabled: web.checked`).
`History`: `Remember recent searches`, `Clear search history` (`enabled: page.cfg_searchHistory.length > 0`).
`What to search`: `Use everything KRunner can search` + new hint `Off: starts with Apps, Settings and Files; tick more
below.` (enabled: false), the 14 checks (`enabled: !all`). Texts that only move keep their msgid exactly.

- [ ] **Step 2: config.qml final order and icons**

```qml
import org.kde.plasma.configuration

// The Win12 settings window: nine pages, one topic each (docs/research/settings-reorg-proposal.md).
ConfigModel {
    ConfigCategory { name: i18n("Style");                    icon: "preferences-desktop-theme-global"; source: "config/ConfigStyle.qml" }
    ConfigCategory { name: i18n("Start menu");               icon: "view-grid";                        source: "config/ConfigStartMenu.qml" }
    ConfigCategory { name: i18n("Apps list");                icon: "view-list-details";                source: "config/ConfigAppsList.qml" }
    ConfigCategory { name: i18n("Start button & shortcuts"); icon: "start-here-kde";                   source: "config/ConfigButton.qml" }
    ConfigCategory { name: i18n("Taskbar");                  icon: "preferences-desktop-plasma";       source: "config/ConfigTaskbar.qml" }
    ConfigCategory { name: i18n("Windows");                  icon: "preferences-system-windows";       source: "config/ConfigWindows.qml" }
    ConfigCategory { name: i18n("Appearance");               icon: "preferences-desktop-color";        source: "config/ConfigAppearance.qml" }
    ConfigCategory { name: i18n("Search");                   icon: "search";                           source: "config/ConfigSearch.qml" }
    ConfigCategory { name: i18n("Advanced");                 icon: "configure";                        source: "config/ConfigAdvanced.qml" }
}
```
`ls package/contents/ui/config/` must list exactly the twelve files of the Global constraints.

- [ ] **Step 3: install.sh and HANDOFF**

`install.sh` line 6: `if command -v node >/dev/null 2>&1; then node tests/test_tiles.js && node tests/test_islands.js && node tests/test_settings_keys.js --strict; fi`.
`docs/HANDOFF.md`: one paragraph after the existing settings notes: "Settings pages (2026-10-17): nine pages in
`config/config.qml` (Style, Start menu, Apps list, Start button & shortcuts, Taskbar, Windows, Appearance, Search,
Advanced); style gating through `config/StyleKinds.qml` (`kinds.menuKind/taskbarKind/windowKind/anyXp`), never
`cfg_menuStyle` directly; read-only key declarations end with `// read only`; `tests/test_settings_keys.js --strict`
guards one control per key; `tests/test_stylekinds.qml` tests the rules (`QT_QPA_PLATFORM=offscreen /usr/lib/qt6/bin/qml`).
Dialog for screenshots: `win12-settings`, click the sidebar with `tests/tools/w16mouse.py drag X,Y X,Y 1`."
`grep -rn 'ConfigMenu\|ConfigLook\|ConfigColours\|ConfigIcons\|ConfigRail' --include='*.sh' --include='*.js' --include='*.qml' --include='*.py' . | grep -v '^./.git/\|^./.claude/'` -> nothing.

- [ ] **Step 4: standard checks, nine screenshots, commit**

qmllint `ConfigSearch.qml`; `sh install.sh | tail -3` (runs the strict guard: `settings keys OK`); screenshots of rows
0-8 in ONE blocking call (nine `drag … ; sleep 1; spectacle …` pairs, then Cancel), `Read` each crop; every page shows
its groups and no stray old label (compare with proposal section 3). Note any mismatch in `notes:` for Task 13.

```bash
git add package/contents/ui/config/ConfigSearch.qml package/contents/config/config.qml install.sh docs/HANDOFF.md
git commit -m "settings: Search groups, final nine-page list, strict key guard in install.sh"
```

---

### Task 12: Translations

Model: sonnet
Depends on: 11

**Files:**
- Modify: `po/es.po`, `po/template.pot`, `package/contents/locale/es/LC_MESSAGES/plasma_applet_org.win12.menu.mo` (built by install.sh)

- [ ] **Step 1: extract and list**

```bash
cd /home/kalaminator/Documents/Start12 && sh package/Messages.sh
msgattrib --untranslated --no-obsolete po/es.po | grep -c '^msgid'; msgattrib --only-fuzzy --no-obsolete po/es.po | grep -c '^msgid'
msgattrib --untranslated --no-obsolete po/es.po | grep '^msgid' | head -80
```
Expected ~60-90 new msgids: page names, group titles, every "(renamed)" label of proposal section 3, the reworded hints,
the island strings, "(applies at once)".

- [ ] **Step 2: translate every one (Edit on `po/es.po`, matching the existing voice)**

Glossary to keep the Spanish consistent with the existing entries (`grep -A1 '^msgid "Taskbar"$' po/es.po` etc.):
Taskbar = Barra de tareas, Style = Estilo, Search = Búsqueda, Start Button = Botón Inicio, Corner radius: = Radio de las
esquinas:. New: `Start menu` = Menú Inicio, `Apps list` = Lista de aplicaciones, `Start button & shortcuts` = Botón
Inicio y accesos directos, `Windows` = Ventanas, `Appearance` = Apariencia, `Advanced` = Avanzado, `Mix and match` =
Combinar estilos, `Size and position` = Tamaño y posición, `Tiles` = Mosaicos (reuse the word es.po already uses for
"tile"; check with `grep -i 'msgstr.*mosaico\|msgstr.*azulejo' po/es.po | head -2`), `(applies at once)` = (se aplica al
instante), `Show islands on:` = Mostrar las islas en:, `All monitors` = Todos los monitores, `Open apps on other monitors:`
= Aplicaciones abiertas en otros monitores:, `Only apps on that monitor` = Solo las aplicaciones de ese monitor,
`All apps on every monitor` = Todas las aplicaciones en todos los monitores, `Placement:` = Colocación:, `Opacity:` =
Opacidad:. Fuzzy entries: fix the msgstr and delete the `#, fuzzy` line.

- [ ] **Step 3: check, build, commit**

`msgfmt --check --statistics -o /dev/null po/es.po` -> `N translated messages.` only (no "untranslated", no "fuzzy");
`sh install.sh | tail -3` (compiles the .mo; the strict guard still OK).

```bash
git add po/es.po po/template.pot package/contents/locale/es/LC_MESSAGES/plasma_applet_org.win12.menu.mo
git commit -m "i18n: Spanish for the reorganised settings pages"
```
(If `Messages.sh` touched other `po/*.po`, add them too.)

---

### Task 13: Review against the proposal, fixes, ELI5 + hand-check list for the user

Model: opus
Depends on: 12

**Files:**
- Modify (fixes only): any `package/contents/ui/config/*.qml`, `po/es.po`; the progress log entry carries the review

- [ ] **Step 1: mechanical checks**

```bash
cd /home/kalaminator/Documents/Start12
node tests/test_settings_keys.js --strict | tail -1                                   # settings keys OK (N keys)
QT_QPA_PLATFORM=offscreen /usr/lib/qt6/bin/qml tests/test_stylekinds.qml 2>&1 | grep stylekinds
/usr/lib/qt6/bin/qmllint package/contents/ui/config/*.qml 2>&1 | grep -E '^Error|error:' | head
grep -n 'cfg_menuStyle ===\|cfg_menuStyle !==\|cfg_taskbarLook ||\|cfg_menuStyle)' package/contents/ui/config/Config*.qml   # nothing: gating goes through kinds
grep -n 'i18n(".*(Windows [0-9X]' package/contents/ui/config/*.qml                    # nothing: no style tags in labels
grep -c 'TaskbarLayoutBox {' package/contents/ui/config/*.qml | grep -v ':0'          # ConfigTaskbar.qml:1 only
grep -n '?' package/contents/ui/config/*.qml | grep 'i18n("[^"]*?")'                  # no question labels (dialog titles like "Reset all Win12 settings?" are fine)
msgfmt --check --statistics -o /dev/null po/es.po
grep -n 'Layout\b\|Shortcuts\|Look page\|Colours page\|Icons page\|Start Button page' docs/FEATURES.md | head   # follow-up list for the user
```

- [ ] **Step 2: row-by-row against proposal section 3**

For each page (3.1 … 3.9) take every `(key)` in the proposal and run `grep -n 'cfg_<key>' package/contents/ui/config/*.qml`:
the declaration is on the page the proposal names, the label is the proposal's string, the `visible:`/`enabled:` rule
matches section 3/4 (with Task 2's verified sets for the three VERIFY rows). Check the four MERGED rows (3.10) exist
once. Fix small deviations in place (label text, a missing `enabled`, a wrong set); anything larger goes to `left:` with
the exact file and rule.

- [ ] **Step 3: live look**

`sh install.sh`, open the dialog and screenshot the pages whose code you changed in Step 2 (Task 1 procedure); then
Cancel.

- [ ] **Step 4: commit fixes, write the review**

```bash
git add -A package/contents/ui/config po/es.po
git commit -m "settings: review fixes against the reorganisation proposal"
```
(skip when nothing changed). The log entry's `notes:` holds: (a) ELI5 for the user: 6-10 plain lines on what moved
where and what "one place per option" means for them; (b) a hand-check list, one line per page: "open Win12 Settings,
page X: change <one setting>, OK, reopen: it stuck" plus "with Style card Windows 12 and Start menu set to Windows 7 the
Windows 7 group shows on Style and Apps list"; (c) the FEATURES.md lines that still name old pages (follow-up, not
done here); (d) anything in `left:`. End with `next: none`, `next-model: none`.

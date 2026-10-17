# Fast islands on a patched KDE, and 3 islands with movable app icons — design

Date: 2026-10-16. Status: approved in chat (4 parts). The user then asked for unattended work ("work without asking me"),
so this spec goes straight to planning without a separate written-spec review.
Plasma version at design time: 6.8.0 (libplasma, plasma-workspace, plasma-desktop 6.8.0-1, Qt 6.12).

## 1. Intent

### What the user said
- Taskbar layout switches and Start position changes (left, center, right) must be fast and smooth, not a
  visible step-by-step shuffle. Changing KDE's own code is allowed.
- Win12 needs the patched KDE. It does not have to work on a normal KDE (decision B).
- One plan for everything (decision B): the fast switching and the movable app icons ship together.
- A new layout with 3 islands. Any app icon (pinned or open) can move from any island to any other island.
- On the island that holds the tray, the tray stays at the screen edge side; app icons go on the other side.
- Start position decides what each island holds (table in 5.2). Right = full mirror of Left.
- The layout menu keeps One bar, Split-left, Split-right and adds 3 Islands.
- An empty island stays as a small island (a drop target).
- Patch 3 KDE packages, libplasma included, so the tray can move normally (decision B).

### What we assume (not said; correct us if wrong)
- Arch Linux only. The patched packages are built with `makepkg` from Arch's own PKGBUILDs.
- The user runs the build script after each Plasma update. The script is not automatic.
- Islands are bottom-edge horizontal panels on one screen, as today. Multi-monitor and top/side islands are
  out of scope.
- Moving an icon is done by drag and drop only (no "Move to island" menu).

## 2. Background: why it is slow today (from reading the 6.8.0 sources)

- `Widget.index` get/set (plasma-workspace `shell/scripting/widget.cpp:102-110`, `:124-136`) work on the saved
  `General/AppletOrder` string, not the live layout. The panel saves that string only through
  `Qt.callLater(LayoutManager.save)` (plasma-desktop `containments/panel/main.qml:101`), which cannot run while
  `evaluateScript` blocks. So after an `addWidget` in the same script, `index` reads -1 and `setIndex` reorders
  from a stale list. Also: `widget.cpp:136` clamps to `order.length()-1` after removing the item (so
  `index = last` lands second-to-last), and an empty `AppletOrder` splits to `[""]`, a fake id 0 that
  `LayoutManager.js:109` counts. A moved widget gets an empty position (scripting `containment.cpp:213`) plus
  the old panel's `positionBeforeDeletion` (`main.qml:91-94,108`), so `LayoutManager.js:45` drops it at the
  slot under its old x.
- Tray config nesting: libplasma `src/plasma/containment.cpp:387,393-394` takes `oldConfig = applet->config()`.
  The tray is a Containment, so that is its whole `[Applets][id]` group (`applet.cpp:199`), and
  `reparent(&c)` keeps the name: `[Applets][id][id]`. `containment.cpp:401-408` also rewrites the settings
  prefix as `Containments<id>Applets` without the `\x1d` separators, and the tray's child applets keep
  their old config paths. Result: missing tray icons until a restart.
- Win12 works around both today: one move per `evaluateScript` call with a 0.3 s pause (5-8 s per switch),
  and a fresh tray instead of a moved one (commit f70da6e).
- Task managers: `launchers` is per instance (plasma-desktop `applets/taskmanager/main.xml:105`), but
  libtaskmanager has no filter by app (`taskfilterproxymodel.cpp:549`, `tasksmodel.cpp:1972`), so every
  instance shows every open window. Drops of a task button from another task manager are rejected
  (`MouseHandler.qml:109-113`).

## 3. Part 1 — Building and keeping the patched KDE

- New folder `kde/` in the repo:
  - `kde/patches/libplasma/*.patch`, `kde/patches/plasma-workspace/*.patch`, `kde/patches/plasma-desktop/*.patch`.
  - `kde/build.sh`: for libplasma, then plasma-workspace, then plasma-desktop (dependency order):
    1. Read the installed version (`pacman -Q`).
    2. `git clone` `https://gitlab.archlinux.org/archlinux/packaging/packages/<pkg>.git` into a fresh build
       dir and check out the tag of that version (e.g. `6.8.0-1`).
    3. Add our patches to `source=` and apply them in `prepare()`; add
       `provides+=("<pkg>-win12=<patch-version>")`.
    4. Import the signing keys listed in the PKGBUILD (`validpgpkeys`). Never skip signature checks.
    5. `makepkg -si`. Stop with a clear message if a patch does not apply.
  - `kde/win12-kde.hook`, shipped inside the patched plasma-workspace package as
    `/usr/share/libalpm/hooks/win12-kde.hook` (so no extra sudo step): after an upgrade of any of the 3
    packages, if `pacman -T libplasma-win12 plasma-workspace-win12 plasma-desktop-win12` reports something
    missing, print "Win12: run kde/build.sh again".
  - sudo: `makepkg -si` needs `sudo pacman`. For unattended runs the user may add a sudoers drop-in that
    allows only `/usr/bin/pacman` without a password, and removes it afterwards.
- Win12 runs the same `pacman -T` check at start (through `run()`); result in a property `kdePatched`.
  - Not patched: no island or Start-position changes are made (existing panels are left as they are). The
    Taskbar settings page shows a warning with the command to run.
- Undo: `sudo pacman -S libplasma plasma-workspace plasma-desktop` puts the stock packages back.

## 4. Part 2 — The KDE patches

### 4.1 libplasma
- `Containment::addApplet` (move case): move the applet's whole main config group
  (`*applet->d->mainConfigGroup()`) into this containment's `Applets` group under the same id, so nothing
  nests. This also fixes the upstream FIXME where `plugin=` is lost for ordinary widgets.
- Write the settings-group prefix with the proper `\x1d` separators.
- For an applet that is itself a containment (the tray): repoint each child applet's config group and
  settings prefix to the new location. Only groups that start with the old prefix are touched.

### 4.2 plasma-workspace
- `shell/scripting/widget.cpp`: split `AppletOrder` with `Qt::SkipEmptyParts` (index get and set) and clamp
  to `order.length()` after removing the item. Together with 4.3 (immediate save), one script can move and
  order several widgets.
- libtaskmanager `TasksModel`: new properties `appIdFilter` (`QStringList`) and `appIdFilterExclude`
  (`bool`), modelled on `groupingAppIdBlacklist` (`tasksmodel.h:85`). In the row filter
  (`tasksmodel.cpp:1972`, after the app id is read) a task is hidden when it is not in the list (include
  mode) or is in the list (exclude mode). Matching like `appsMatch` (`tasktools.cpp:503`): `AppId`, falling
  back to `LauncherUrlWithoutIcon`. Setter calls `invalidateFilter()`. Empty list + include mode = no filter.

### 4.3 plasma-desktop
- `containments/panel/contents/ui/main.qml:101`: call `LayoutManager.save()` directly, not through
  `Qt.callLater`.
- Task manager applet:
  - Config key `win12Island` (String, default empty = behave exactly like stock).
  - Island registry: one shared object in the applet's C++ plugin (one instance per plasmashell, used by
    every task manager). It maps app id -> island name, saved in `~/.config/win12islandsrc` (`[Apps]`
    group). Signal `changed()` makes every task manager re-filter at once.
  - Filter binding: island `open` = exclude mode with all app ids assigned to other islands; any other
    island = include mode with the app ids assigned to it. Pinned launchers of an instance always show in it.
  - Drag and drop: the drag mime data gets `text/x-win12-appid` (and the launcher URL when pinned). A task
    manager with a `win12Island` accepts a drop from another task manager (the stock code rejects it),
    assigns the app to its island in the registry, adds the launcher at the drop position when it was pinned,
    and orders the task at the drop position. The source task manager sees the registry change and removes its
    own launcher for that app. The hover-activate timer is suppressed for these drags.

## 5. Part 3 — Win12 changes

### 5.1 Fast layout engine
- `islands.js` and the Start-position script in `syncCenterPanel()` do all their moves, adds and index sets
  in one `evaluateScript` call. Magnetic offsets need the new panel lengths, which Plasma computes after the
  call, so a second call places the offsets. The retry loop stays as a safety net; the expected count is 2
  calls. Goal: every switch under 1.5 s.
- The tray is moved normally (4.1). One exception, as the one-time cleanup of configs nested before the
  patch: the first tray move after the patch uses the current fresh-tray copy, then the host panel gets the
  marker `win12TrayClean=true` and later moves are normal moves.

### 5.2 The 3 Islands layout (`taskbarLayout` value `islands`)

| Start position | Left island | Middle island | Right island |
|---|---|---|---|
| Left | Start + pinned | open apps | apps, weather, tray, clock, show desktop |
| Center | pinned | Start + open apps | apps, weather, tray, clock, show desktop |
| Right (full mirror) | show desktop, clock, tray, weather, apps | open apps | pinned + Start |

- Each island has one task manager with `win12Island` = `pinned`, `open` or `tray`. The tray island's
  task manager is empty at first and sits on the side away from the screen edge.
- The Start widget stays in the host panel (the panel that holds Win12 and the `win12Layout` key). A Start
  position change does not move widgets between panels: it re-places the panels (order, alignment, offsets),
  swaps the `win12Island` names of the host's task manager and the other app island's task manager, and swaps
  their `launchers` lists, so pinned apps stay in the pinned island.
- Going from One bar to 3 Islands: the existing task manager's launchers become the pinned island's
  launchers; the registry starts empty, so every open app shows in the open island (pinned apps that are
  running show merged with their launcher in the pinned island, as icontasks does today).
- An app never moved goes to the `open` island. A moved app stays in its island for all its windows, also
  after a restart (registry file).
- An empty island stays as a small island (a drop target).
- Magnetic mode, the bottom gap (`taskbarGap`) and the show-together reveal (KWin script + `org.win12.reveal`
  helper) cover all 3 islands.
- Settings: `TaskbarLayoutBox.qml` gets a 4th entry "3 Islands"; the patched-KDE warning shows there too.
  New strings go through `package/Messages.sh` and get Spanish translations.
- Going back to One bar, Split-left or Split-right: the extra task managers are removed and the original
  task manager gets back the full launcher list (pinned island first, then the others) and an empty
  `win12Island`, so it behaves like stock.

## 6. Error handling
- `kde/build.sh`: `set -e`; stops with the failing package and step named. A patch that no longer applies
  after a Plasma update stops the build before anything is installed for that package.
- Win12 without the patch: warning, no layout changes (see 3).
- Registry file missing or unreadable: empty registry (everything in the `open` island).
- A drop of an app with no app id and no launcher URL: rejected (nothing changes).

## 7. Part 4 — Testing
- KDE: one QTest per patched behaviour, run in `check()` during the build: several moves + index sets in one
  script give the right order; a moved containment-applet keeps one config level and its children; the
  `appIdFilter` include/exclude modes filter the right rows.
- Win12: layout logic checks in `tests/` in the style of `tests/test_tiles.js` (pure functions: which widget
  goes to which island for each layout and Start position, launcher list swaps).
- Live, on the user's desktop, with a backup of `plasma-org.kde.plasma.desktop-appletsrc` before each run:
  every layout (One bar, Split-left, Split-right, 3 Islands) x every Start position; time each switch
  (goal < 1.5 s); screenshot each result; check all tray icons show; drag an app across all 3 islands, restart
  plasmashell, check the app is still in its island.

## 8. Out of scope
- Other distributions, multi-monitor islands, top/side islands, hiding empty islands, a "Move to island" menu.

## 9. Risks and follow-ups
- Every Plasma update (every few weeks) needs a rebuild of 3 packages (about 20-40 min). A patch may stop
  applying on a new Plasma version and need an update first; until then the user runs stock KDE and Win12
  shows its warning.
- libplasma is used by all of Plasma: a mistake there can break more than Win12. The undo command in 3 always
  works.
- Follow-up idea: the `widget.cpp` index bugs, the delayed save and the tray config nesting are real KDE bugs.
  Sending those fixes upstream would shrink our patch over time.

## 10. Addendum (2026-10-17): islands on extended multi-monitor setups

Approved in chat. Mirrored screens need nothing: Plasma sees them as one screen.

- New setting **Show islands on**: one entry per connected monitor (by output name) plus **All monitors**.
  Default: the main (primary) monitor.
  - One monitor: all Win12 islands (host + island panels) live on the chosen monitor; the other monitors get
    no Win12 taskbar.
  - All monitors: every monitor gets the Start island and the app islands. The tray island (weather, tray,
    clock, show desktop) is only on the main monitor (like Windows 11). Pinned apps show on every monitor.
- New setting **Open apps on other monitors** (used with All monitors): **Only apps on that monitor**
  (default; the task managers' stock "show only tasks from the current screen" filter) or **All apps on every
  monitor**.
- Moving an app icon to another island moves it on all monitors (the registry maps app -> island role, it is
  not per monitor).
- Show-together reveal (KWin script): only the islands of the monitor where the pointer touches the bottom
  edge are revealed (today every monitor's islands are revealed; this also fixes that review finding). The
  script measures that monitor (the pointer's screen), not `workspace.activeScreen`.
- Monitor plugged or unplugged: Win12 rebuilds the islands for the new set of monitors.
- Testing: only one monitor is connected on the dev machine, so the multi-monitor logic is covered by the node
  fake-shell harness with 2-3 fake screens; the real 2-monitor check is left to the user.

## 11. Addendum (2026-10-17): Stock KDE mode

Decision A (user, 2026-10-17): one widget, two modes, picked at runtime. This replaces section 1's "does not have
to work on a normal KDE" and section 3's "Not patched: no island or Start-position changes are made".

### 11.1 Modes
- **Patched** (`pacman -T libplasma-win12 plasma-workspace-win12 plasma-desktop-win12` reports nothing missing):
  today's engine, unchanged: every add, move and index set in one `evaluateScript` pass, 2-3 passes per switch.
- **Stock** (anything else: a stock Arch, another distro without `pacman`, a Plasma update that dropped the
  patched packages until `kde/build.sh` runs again): One bar, Split-left, Split-right and the Start position
  (Left, Centre, Right; the mirrored bar) work the old way (commit f70da6e and before): **one layout step per
  pass**, a pass every 0.3 s, so a switch takes 5-10 s and icons visibly jump. Accepted.
- **3 Islands** (and dragging apps between islands) stays patched-only: the task-manager filter, the island
  registry and the drop handling live in the plasma-desktop/plasma-workspace patches and have no stock
  equivalent. On stock KDE, with `taskbarLayout = islands`, Win12 touches no panel (whatever is there stays,
  also three islands made while patched) and the Taskbar page says why.
- Win12 never restarts plasmashell (the user unit has `KillMode=control-group`, a restart kills the apps
  started from the menu, and `StartLimitBurst=3`). Live tests by agents restart it only through
  `./install.sh --restart`, as every earlier live task did, and say so in the progress log.

### 11.2 Detection
- One check per plasmashell start, the executable `Tiles.PATCH_CHECK` (code/tiles.js), used by `main.qml` and
  by the Taskbar settings page: `pacman -T <3 provides> >/dev/null 2>&1 && [ ! -e
  "${XDG_CONFIG_HOME:-$HOME/.config}/win12-force-stock" ] && echo ok`. No `pacman` (another distro) = stock.
- `main.qml`: `kdePatched` (true = fast engine) and `kdeChecked` (the check has answered). No sync runs before
  the answer; when it arrives both syncs run once (`onKdeChecked`). `islandsNeedPatch = !kdePatched &&
  cfg.taskbarLayout === "islands"` gates `syncIslands()` and `syncCenterPanel()`.
- The marker file `~/.config/win12-force-stock` forces stock mode on a patched machine. It exists for the live
  test on the dev PC (section 11.5); it is not a user setting and is not documented in the settings UI.

### 11.3 What depends on which patch (read from `kde/patches/*` and the engine at 1f009d0)
| Patch | Stock KDE behaviour it removes | Engine code that relies on it | Stock-mode rule |
|---|---|---|---|
| libplasma `Containment::addApplet` keeps the moved applet's config group | a system tray moved live ends as `[Applets][id][id]`: icons missing until a restart | `islands.js tray()`: a live move once the host has `win12TrayClean=true` | `STOCK` ignores `win12TrayClean`: every tray move is a fresh copy of the General group (f70da6e) |
| plasma-workspace `widget.cpp` index fixes (SkipEmptyParts, clamp after removal) | `index = last` lands second-to-last; an empty `AppletOrder` is a fake id 0 | `order()` sets every mismatching index in one pass; `centerLoop()` "all index sets in one pass" | one index set per pass, always the first mismatch (never the last item): the old `fix()` |
| plasma-desktop panel saves `AppletOrder` at once (not `Qt.callLater`) | inside one script `Widget.index` reads -1 for a widget added or moved in that script and a stale list after a removal | the whole one-pass design: adds, moves, removes and index reads in the same pass | the pass ends after its first layout mutation (add, move, remove or index set); the next pass reads a saved order |
| plasma-desktop `LayoutManager.js` insert fix | a widget moved past the target's widgets is in the containment but not in the layout (`index` -1) | nothing since bdd2d2a; the `lost` safety net (`islands.js` ~103) remains | the net is the cross-pass bounce: out to the host, in again next pass (lands at 0, then `order()`) |
| plasma-workspace `TasksModel.appIdFilter`, plasma-desktop `win12Island`, registry, drag/drop | per-island task managers, app moves between islands | 3 Islands roles (`setTM`, `win12Island`, `islands.js` ~108+) | none: 3 Islands locked on stock (11.1) |
| none (stock scripting API) | — | `Panel.screen`, `screenForConnector`, `reloadConfig`, `configKeys`, `widgetIds`, `lengthMode`/`offset`, `win12*` config keys, the reveal helper and KWin script | unchanged |

### 11.4 Engine rules in stock mode (`STOCK = @STOCK@` in `islands.js`, `STOCK` constant in `centerLoop()`)
- One layout mutation per pass: after the first add, move, remove or index set the pass prints `again`; the
  rest of the pass runs next time. Config writes (`writeConfig`, `reloadConfig`, panel geometry, offsets) are
  not layout mutations and may share a pass.
- `order()` sets only the first mismatching index. `tray()` always makes a fresh copy. The `lost` net stays.
- The pass loop keeps its caps (`seq 130` x 0.3 s for islands, `seq 25` for the Start position): a stock split
  needs about 15-20 passes, a mirror about 8.
- With `STOCK = false` the engine is byte-for-byte the patched one: the harness's patched cases do not change.

### 11.5 Testing
- `tests/test_islands.js`: `shell(screenW, screens, stock)`. The stock fake models what the patches remove:
  `Widget.index` reads a per-panel `saved` order snapshotted only at the end of a pass (`endPass()`), the setter
  clamps to `saved.length - 1` after removing the id, a moved widget is inserted at its old slot (`x`, the
  laid-out index; 0 right after an add or move) and goes to `lost` (in `widgets()`, index -1) when that slot is
  past the target's widgets. Cases: split-left, split-right and One bar converge in <= 40 passes and need more
  than 3; a fresh tray on every split (the clean marker is ignored); patch lost after a Plasma update (three
  islands made patched, then split-left on stock: one task manager with the merged pins, no `lost` widget); the
  Start position mirror and its undo converge on stock; the patched cases are unchanged.
- Live on the dev PC (patched): only through the marker file. It proves the stock code path runs without
  script errors, converges, never restarts plasmashell and shows the settings note; it cannot prove stock
  index semantics (the patched `Widget.index` is always fresh) or the real tray-icon loss. Those are harness-only.
- The user's live stock test: stock packages, log out/in, the checklist in the plan, patched packages back from
  `~/.cache/win12-kde/<pkg>/*.pkg.tar.zst`.

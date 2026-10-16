# Fast islands on a patched KDE + 3 Islands — implementation plan

> **For agentic workers:** this plan is executed by an unattended loop of fresh `claude -p` runs (see "Driver
> protocol"). Each run does ONE task. Inside a run use superpowers:executing-plans (checkbox steps below).

**Goal:** taskbar layout and Start-position switches land in one plasmashell script (< 1.5 s) on a patched KDE,
plus a new "3 Islands" layout whose app icons can be dragged between islands and stay there after a restart.

**Architecture:** three Arch packages (libplasma, plasma-workspace, plasma-desktop) are rebuilt from Arch's own
PKGBUILDs with small patches under `kde/patches/` (config move without nesting, scripting index fixes + immediate
panel save, a `TasksModel.appIdFilter`, a Win12 island registry + drag/drop in the task manager). Win12's
`islands.js` then does all moves/orders in one pass and gains the `islands` mode; a `kdePatched` check gates every
panel change.

**Tech stack:** KDE Plasma 6.8.0 (Qt 6.12, C++20, QML), Arch `makepkg` + `pacman`, POSIX sh/bash, node for the
Win12 logic tests, gettext for es.po.

**Spec:** `docs/superpowers/specs/2026-10-16-fast-islands-design.md` (source of truth; read it first every run).

**Base commit:** `4ad5be4` (Design: fast islands...). The whole change set = `git diff 4ad5be4..HEAD`.

## Global constraints

- Arch Linux only; installed versions at plan time: libplasma / plasma-workspace / plasma-desktop `6.8.0-1`,
  qt6-base `6.12.0-3`. Arch packaging tags are `<pkgver>-<pkgrel>` (`6.8.0-1`).
- Build dirs under `~/.cache/win12-kde/` only (never in the repo). Dev source copies: `~/.cache/win12-kde/src/<pkg>`
  (local `git clone` of the read-only v6.8.0 clones in
  `/tmp/claude-1000/-home-kalaminator-Documents-Start12/b88e81db-1485-4a3f-ae50-6c8c3972125b/scratchpad/kde/<pkg>`;
  if that folder is gone, `git clone --depth 1 --branch v6.8.0 https://invent.kde.org/plasma/<pkg>.git`).
  Arch packaging clones: `~/.cache/win12-kde/<pkg>` (made by `kde/build.sh`).
- sudo: ONLY `/usr/bin/pacman` runs without a password (`/etc/sudoers.d/99-win12-build`). The check is
  `sudo -n /usr/bin/pacman -V >/dev/null` (NOT `sudo -n true`, which fails by design). Installs go through
  `makepkg -s` (calls `sudo pacman`) and `sudo -n pacman -U --noconfirm <pkg files>`. Nothing else may use sudo:
  files for `/etc` or `/usr` ship inside a package or are not done. Never write a password anywhere.
- Never skip signature checks (`validpgpkeys` stay; keys imported from `hkps://keyserver.ubuntu.com`).
- Names: plugin `org.win12.menu`, generated names `win12-*`, config marker keys `win12*`, patch files
  `kde/patches/<pkg>/0001-win12-*.patch`, QTests named `win12*`.
- Repo style: short files, comments that say WHY, one-line `// ponytail:` notes on deliberate shortcuts; read
  `docs/HANDOFF.md` lines 1-40 and the file you touch (grep, then small ranges; never a whole huge file).
- Every task fits one run: <= ~40 tool calls / ~100k tokens. Test output through `| tail`. Long commands run
  as ONE blocking call (<= 10 min each); builds run detached (see Task 10). No polling loops in the agent.
- Live-desktop safety: back up `~/.config/plasma-org.kde.plasma.desktop-appletsrc` before any live test; restart
  only with `systemctl --user restart plasma-plasmashell`; screenshots `spectacle -b -n -f -o <file>`.
- Commits: one per task, message `<area>: <what>` + a short why, ending with the two attribution lines the
  driver prompt gives (when it gives none: the same two lines as the plan commit).

## Driver protocol (progress log format)

File: `docs/superpowers/plans/2026-10-16-fast-islands-progress.md`. Each run:

1. Reads the spec, this plan, and the progress log. The last entry's `next:` line names the task to do;
   `next-model:` names the model the driver must start for it.
2. Does that task's steps, verifies, commits, then appends ONE entry:

```
## <YYYY-MM-DD HH:MM> Task N: <title>
status: done | blocked | partial | skipped
done: <what landed, commit hash>
left: <what is not done, or "nothing">
notes: <facts the next run needs; timing numbers; files>
next: Task M — <title>            (or `next: none` when Task 15 is done)
next-model: opus | sonnet | none
```

3. Exits. The driver reads the LAST `next-model:` line to choose the model for the next run.

Rules: `status: blocked` is for tasks that need something unavailable (sudo pacman, a package that is not
installed yet). A blocked task's dependants (see each task's **Depends on**) are `skipped` with
`left: blocked by Task N`; `next:` then points at the next task that does not depend on it. `partial` =
out of budget mid-task: the entry's `next:` names the SAME task and `notes:` says the exact step to resume at
(work is committed at a safe point first, or stashed with `git stash push -m "Task N partial"` and noted).
A model name in a task header (`Model: opus|sonnet`) is what the previous entry's `next-model:` must carry.

## Task order (sudo-free first)

| # | Model | Task | Depends on |
|---|---|---|---|
| 1 | sonnet | `kde/` tooling: build.sh, pkgbuild-win12.sh, pacman hook, fixture test | - |
| 2 | opus | libplasma patch: config move keeps one level + coronatest case | 1 |
| 3 | opus | plasma-workspace patch: scripting index fixes + `appIdFilter` + QTest | 1 |
| 4 | opus | plasma-desktop patch: immediate save, island registry + QTest, task manager QML | 1 |
| 5 | opus | Win12 `islandlogic.js` + `tests/test_islands.js` (pure tests + fake-shell harness) | - |
| 6 | opus | Win12 `islands.js` fast engine + `islands` mode, `syncCenterPanel` one pass | 5 |
| 7 | opus | Win12 `kdePatched` gate, "3 Islands" entry + warning in settings | 6 |
| 8 | sonnet | Translations (Messages.sh, es.po, .mo) | 7 |
| 9 | sonnet | Live-test tools in `tests/tools/` + README/HANDOFF docs | 1 |
| 10 | opus | Build + install patched libplasma (sudo pacman) | 2 |
| 11 | opus | Build + install patched plasma-workspace | 3, 10 |
| 12 | opus | Build + install patched plasma-desktop | 4, 11 |
| 13 | opus | Live test A: every layout x Start position, timing, screenshots, tray | 7, 9, 12 |
| 14 | opus | Live test B: drag an app across the 3 islands, restart persistence, restore | 13 |
| 15 | opus | Whole-change review + ELI5 for the user | all |

## Review focus

Inputs the spec implies but no spec test names; each has its test in the owning task:

1. Host panel without a task manager and `taskbarLayout=islands`: nothing is created, script prints `done`
   (Task 6 harness case `noTaskManager`).
2. Registry file missing or unreadable: empty registry, every app in `open` (Task 4 `missingFileIsEmpty`).
3. A stock task manager (empty `win12Island`) must behave exactly as before: empty filter in include mode
   shows all rows (Task 3 `noFilterShowsAll`).
4. A drop whose `text/x-win12-appid` is empty is rejected, and a drag inside one island still reorders as stock
   (Task 4 code paths; checked live in Task 14 since QML drops have no unit harness).
5. `AppletOrder` empty parts / `index = last`: the harness models correct behaviour (Task 5), the real
   `widget.cpp` fix is observed live by the tray island order in Task 13 (`state.sh`).

## Defaults picked without asking (the user should know)

- No QTest for the `widget.cpp` index fix: it needs a running ShellCorona (kwin_wayland in `check()`); it is
  verified live (Task 13) and the logic is modelled in the node harness. The libplasma QTest covers the plain
  applet move; the tray (containment-applet) path is checked live by "all tray icons show after a move".
- Launchers are NOT exempt from `appIdFilter`. "Pinned launchers always show in their island" comes from the
  registry: a task manager with a `win12Island` registers its launchers' app ids to its island
  (`assignLaunchers`), so its include/exclude list is right by construction. This keeps the filter testable
  with launcher-only data.
- Patched PKGBUILDs build with `-DBUILD_TESTING=ON`; `check()` runs only `ctest -R 'win12|coronatest'`.
- The island registry is one process-wide object exposed as the QML singleton `TaskManagerApplet.Win12Islands`;
  file `~/.config/win12islandsrc`, group `[Apps]`, `appid=island`.
- App identity for drops: `AppId`, falling back to `LauncherUrlWithoutIcon` (both are checked by the filter).
- An island task manager with no tasks keeps one icon slot of width so it is a drop target.
- Builds run detached (`setsid nohup kde/build.sh <pkg>`); the run waits with one <= 10-min blocking
  `until grep` per call and may hand the wait to the next run (`status: partial`).
- Live tests end by putting the user's `taskbarLayout` / `menuPosition` / `taskbarLook` values back (not the
  appletsrc backup, which would need another restart). Backup kept at
  `~/.config/plasma-org.kde.plasma.desktop-appletsrc.bak-fastislands-<date>`.
- The pointer tool gets a `drag` mode (uinput press/move/release). If KWin does not turn it into a Qt drag, the
  drag check is reported for the user to do by hand.
- Spanish only for new strings (other .po files keep untranslated entries, as today).

---

### Task 1: `kde/` tooling (build.sh, pkgbuild-win12.sh, pacman hook, fixture test)

Model: sonnet
Depends on: -

**Files:**
- Create: `kde/build.sh`, `kde/pkgbuild-win12.sh`, `kde/win12-kde.hook`, `tests/test_pkgbuild.sh`
- Create (empty markers so the dirs exist): `kde/patches/libplasma/.keep`, `kde/patches/plasma-workspace/.keep`,
  `kde/patches/plasma-desktop/.keep`

**Interfaces (produces):** `kde/build.sh [pkg...]` prints `WIN12_BUILD_OK <pkg>` or `WIN12_BUILD_FAIL <pkg> <step>`
per package; `kde/pkgbuild-win12.sh <pkg> <patchdir> <hookfile>` edits `./PKGBUILD` in place. Tasks 10-12 rely on
both lines and on the `provides` name `<pkg>-win12`.

- [ ] **Step 1: write `kde/win12-kde.hook`**

```ini
# Win12: after a Plasma update, say so when a stock package replaced a patched one (spec part 1).
# Shipped by the patched plasma-workspace package as /usr/share/libalpm/hooks/win12-kde.hook.
[Trigger]
Operation = Install
Operation = Upgrade
Type = Package
Target = libplasma
Target = plasma-workspace
Target = plasma-desktop

[Action]
Description = Win12: checking the patched KDE packages...
When = PostTransaction
Exec = /bin/sh -c 'pacman -T libplasma-win12 plasma-workspace-win12 plasma-desktop-win12 >/dev/null || echo "Win12: a stock KDE package replaced a patched one. Run kde/build.sh again."'
```

- [ ] **Step 2: write `kde/pkgbuild-win12.sh`**

```bash
#!/bin/bash
# Edits the Arch PKGBUILD in the current dir for Win12: our patches in source= and a prepare() that applies them,
# tests on and a check() for our QTests, provides <pkg>-win12, the pacman hook (plasma-workspace only) in package().
# Fails (nothing written) when the PKGBUILD shape changed: fix this script, do not edit the clone by hand.
# usage: pkgbuild-win12.sh <pkg> <patch dir> <hook file>
set -eu
PKG=$1 PD=$2 HOOK=$3
grep -q '^prepare()' PKGBUILD && { echo "$PKG: PKGBUILD already has prepare(): merge by hand"; exit 1; }
grep -q -- '-DBUILD_TESTING=OFF' PKGBUILD || { echo "$PKG: no -DBUILD_TESTING=OFF in PKGBUILD"; exit 1; }
grep -q '^package() {' PKGBUILD || { echo "$PKG: no 'package() {' in PKGBUILD"; exit 1; }
sed -i -e 's/-DBUILD_TESTING=OFF/-DBUILD_TESTING=ON/' -e 's/^package() {/_win12_package() {/' PKGBUILD
FILES=()
for f in "$PD"/*.patch; do cp "$f" .; FILES+=("$(basename "$f")"); done
[ "$PKG" = plasma-workspace ] && { cp "$HOOK" .; FILES+=("$(basename "$HOOK")"); }
SUMS=(); for f in "${FILES[@]}"; do SUMS+=("'$(sha256sum "$f" | cut -c1-64)'"); done
{
  echo; echo '# --- Win12 (kde/pkgbuild-win12.sh): patches, tests, provides, hook ---'
  echo "source+=(${FILES[*]})"
  echo "sha256sums+=(${SUMS[*]})"
  echo "provides+=(\"$PKG-win12=\$pkgver-\$pkgrel\")"
  echo 'prepare() { cd "$pkgname-$pkgver"; for p in "$srcdir"/*.patch; do patch -Np1 -i "$p"; done; }'
  echo "check() { ctest --test-dir build -R 'win12|coronatest' --output-on-failure; }"
  if [ "$PKG" = plasma-workspace ]; then
    echo 'package() { _win12_package; install -Dm644 "$srcdir/win12-kde.hook" "$pkgdir/usr/share/libalpm/hooks/win12-kde.hook"; }'
  else
    echo 'package() { _win12_package; }'
  fi
} >> PKGBUILD
```

- [ ] **Step 3: write `kde/build.sh`**

```bash
#!/bin/bash
# Win12: build and install the patched KDE packages from Arch's own PKGBUILDs (spec part 1), in dependency order.
# usage: kde/build.sh [libplasma] [plasma-workspace] [plasma-desktop]     (default: all three)
# Needs git, makepkg, gpg. Installing needs `sudo pacman` (a sudoers drop-in for /usr/bin/pacman alone is enough);
# without it the packages are built and the pacman -U command is printed. Undo: sudo pacman -S libplasma plasma-workspace plasma-desktop
# Prints WIN12_BUILD_OK <pkg> or WIN12_BUILD_FAIL <pkg> <step> and stops at the first failure (a patch that no
# longer applies fails in makepkg's prepare(), before anything is installed).
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
CACHE="${XDG_CACHE_HOME:-$HOME/.cache}/win12-kde"
PKGS=${*:-libplasma plasma-workspace plasma-desktop}
if sudo -n /usr/bin/pacman -V >/dev/null 2>&1; then INSTALL=1; else INSTALL=; echo "Win12: sudo pacman needs a password: building only"; fi
PKG=; STEP=
step() { STEP=$1; echo "== $PKG: $STEP"; }
trap '[ $? -ne 0 ] && echo "WIN12_BUILD_FAIL ${PKG:-?} ${STEP:-?}"' EXIT
for PKG in $PKGS; do
  step version; VER=$(pacman -Q "$PKG" | cut -d' ' -f2)
  D="$CACHE/$PKG"; rm -rf "$D"; mkdir -p "$CACHE"
  step "clone $VER"; git clone -q --depth 1 --branch "$VER" "https://gitlab.archlinux.org/archlinux/packaging/packages/$PKG.git" "$D"
  step pkgbuild; (cd "$D" && bash "$HERE/pkgbuild-win12.sh" "$PKG" "$HERE/patches/$PKG" "$HERE/win12-kde.hook")
  step "gpg keys"; (cd "$D" && source ./PKGBUILD && gpg --keyserver hkps://keyserver.ubuntu.com --recv-keys "${validpgpkeys[@]}")
  step makepkg; (cd "$D" && makepkg -f ${INSTALL:+-s} --noconfirm)
  if [ -n "$INSTALL" ]; then step install; sudo -n pacman -U --noconfirm "$D"/*.pkg.tar.zst
  else echo "to install: sudo pacman -U $D/*.pkg.tar.zst"; fi
  echo "WIN12_BUILD_OK $PKG"
done
```

- [ ] **Step 4: write `tests/test_pkgbuild.sh`** (fixture PKGBUILD, runs the editor, validates with makepkg)

```sh
#!/bin/sh
# Run: sh tests/test_pkgbuild.sh   -- kde/pkgbuild-win12.sh against a tiny PKGBUILD shaped like Arch's plasma ones.
set -e
HERE=$(cd "$(dirname "$0")/.." && pwd); T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
mkdir -p "$T/patches"; printf 'diff --git a/x b/x\n' > "$T/patches/0001-win12-x.patch"
cat > "$T/PKGBUILD" <<'EOF'
pkgname=plasma-workspace
pkgver=6.8.0
pkgrel=1
arch=(x86_64)
source=(x.tar.xz)
sha256sums=('0')
build() {
  cmake -B build -S $pkgname-$pkgver \
    -DBUILD_TESTING=OFF
  cmake --build build
}
package() {
  DESTDIR="$pkgdir" cmake --install build
}
EOF
cd "$T" && bash "$HERE/kde/pkgbuild-win12.sh" plasma-workspace "$T/patches" "$HERE/kde/win12-kde.hook"
bash -n PKGBUILD
S=$(makepkg --printsrcinfo)
echo "$S" | grep -q 'source = 0001-win12-x.patch' && echo "$S" | grep -q 'source = win12-kde.hook' \
  && echo "$S" | grep -q 'provides = plasma-workspace-win12=6.8.0-1' && grep -q 'BUILD_TESTING=ON' PKGBUILD \
  && grep -q '^_win12_package() {' PKGBUILD && grep -c '^package() {' PKGBUILD | grep -qx 1 && echo "test_pkgbuild OK"
# second run must refuse (prepare() already there)
if bash "$HERE/kde/pkgbuild-win12.sh" plasma-workspace "$T/patches" "$HERE/kde/win12-kde.hook" 2>/dev/null; then echo "FAIL: edited twice"; exit 1; fi
```

- [ ] **Step 5: run the fixture test**

Run: `chmod +x kde/build.sh kde/pkgbuild-win12.sh tests/test_pkgbuild.sh && sh tests/test_pkgbuild.sh | tail -3`
Expected: `test_pkgbuild OK` (and no `FAIL`). `bash -n kde/build.sh` prints nothing.

- [ ] **Step 6: commit**

```bash
git add kde tests/test_pkgbuild.sh
git commit -m "kde: build tooling for the patched Plasma packages (build.sh, PKGBUILD editor, pacman hook)"
```

---

### Task 2: libplasma patch — moving an applet keeps one config level

Model: opus
Depends on: 1

**Files:**
- Create: `kde/patches/libplasma/0001-win12-containment-move-keeps-config.patch` (a `git diff` of the two edits below)
- Edit (in `~/.cache/win12-kde/src/libplasma`): `src/plasma/containment.cpp` (move branch of `Containment::addApplet`,
  lines ~380-412), `autotests/coronatest.{h,cpp}` (new case)

**Interfaces (produces):** after the patch, `Containment::addApplet(applet)` for an applet of another containment
moves `[Containments][old][Applets][id]` to `[Containments][new][Applets][id]` whole (plugin= kept, no nesting) and
re-targets the applet's and (for a containment-applet) its children's config groups.

- [ ] **Step 1: dev source copy**

```bash
S=/tmp/claude-1000/-home-kalaminator-Documents-Start12/b88e81db-1485-4a3f-ae50-6c8c3972125b/scratchpad/kde
mkdir -p ~/.cache/win12-kde/src && git clone -q "$S/libplasma" ~/.cache/win12-kde/src/libplasma && cd ~/.cache/win12-kde/src/libplasma && git describe --tags
```
Expected: `v6.8.0`. Read `src/plasma/containment.cpp` 370-420, `src/plasma/private/applet_p.h` 40-90 (check
`mainConfig` is a public member) and `src/plasma/private/applet_p.cpp` around `resetConfigurationObject` (grep).

- [ ] **Step 2: write the failing test** — in `autotests/coronatest.h` add the slot `void moveAppletKeepsConfig();`
  BEFORE `immutability()` (which must stay last); in `coronatest.cpp` add before `CoronaTest::immutability`:

```cpp
// Win12: Containment::addApplet moves the applet's whole main config group, nothing nests (the system tray
// used to end as [Applets][id][id] and show only some icons until a restart).
void CoronaTest::moveAppletKeepsConfig()
{
    Plasma::Containment *from = m_corona->containments().at(0);
    Plasma::Containment *to = m_corona->containments().count() > 1 ? m_corona->containments().at(1) : m_corona->createContainment(QStringLiteral("invalid"));
    QVERIFY(to && to != from);
    Plasma::Applet *a = from->createApplet(QStringLiteral("invalid"));
    const QString id = QString::number(a->id());
    from->config().group(QStringLiteral("Applets")).group(id).writeEntry("plugin", "invalid");
    a->config().writeEntry("win12", "kept");   // the [Configuration] subgroup

    to->addApplet(a);

    QCOMPARE(a->containment(), to);
    KConfigGroup g = to->config().group(QStringLiteral("Applets")).group(id);
    QCOMPARE(g.readEntry("plugin", QString()), QStringLiteral("invalid"));
    QCOMPARE(g.group(QStringLiteral("Configuration")).readEntry("win12", QString()), QStringLiteral("kept"));
    QVERIFY(!g.hasGroup(id));
    QVERIFY(!from->config().group(QStringLiteral("Applets")).hasGroup(id));
    QCOMPARE(a->config().readEntry("win12", QString()), QStringLiteral("kept"));   // read through the new path
}
```
If `createContainment("invalid")` returns null in this fixture, read `plasma-test-appletsrc` and use whatever second
containment / plugin name the existing tests use (`checkOrder`).

- [ ] **Step 3: run it, see it fail**

```bash
cd ~/.cache/win12-kde/src/libplasma && cmake -B build -S . -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug >/dev/null && cmake --build build --target coronatest -j"$(nproc)" 2>&1 | tail -3 && QT_QPA_PLATFORM=offscreen ctest --test-dir build -R coronatest --output-on-failure 2>&1 | tail -15
```
Expected: FAIL in `moveAppletKeepsConfig` (`g.hasGroup(id)` true or `plugin` empty). One blocking call; if the
configure needs a missing dep, note it and use `-DBUILD_TESTING=ON` only.

- [ ] **Step 4: patch `containment.cpp`** — replace the move branch body from `KConfigGroup oldConfig = applet->config();`
  down to the end of the `if (applet->configScheme()) {...}` block with:

```cpp
        // Win12: move the applet's whole main group ([Applets][id] with plugin=), not applet->config(): for a
        // containment-applet (the system tray) that IS the main group and KConfigGroup::reparent keeps the name,
        // so the tray ended as [Applets][id][id] and showed only some icons until a restart. Also fixes the
        // upstream FIXME (plugin= lost) for ordinary widgets.
        KConfigGroup oldMain = *applet->d->mainConfigGroup();
        const QChar sep(0x1d);   // KConfigGroup joins nested group names with \x1d
        const QString oldGroupPrefix = QStringLiteral("Containments") + sep + QString::number(currentContainment->id()) + sep + QStringLiteral("Applets") + sep;
        const QString newGroupPrefix = QStringLiteral("Containments") + sep + QString::number(id()) + sep + QStringLiteral("Applets") + sep;
        currentContainment->d->applets.removeAll(applet);
        applet->setParent(this);

        KConfigGroup applets = config().group(QStringLiteral("Applets"));
        oldMain.reparent(&applets);
        // forget the cached groups so they are built again under this containment; only groups under the old
        // prefix are re-targeted (a tray's children live under it too)
        const auto retarget = [&](Applet *a) {
            delete a->d->mainConfig;
            a->d->mainConfig = nullptr;
            a->d->mainConfigGroup();
            if (KConfigLoader *scheme = a->configScheme()) {
                if (scheme->currentGroup().startsWith(oldGroupPrefix)) {
                    scheme->setCurrentGroup(newGroupPrefix + scheme->currentGroup().mid(oldGroupPrefix.length()));
                }
                const auto items = scheme->items();
                for (KConfigSkeletonItem *item : items) {
                    if (item->group().startsWith(oldGroupPrefix)) {
                        item->setGroup(newGroupPrefix + item->group().mid(oldGroupPrefix.length()));
                    }
                }
            }
        };
        retarget(applet);
        if (applet->isContainment()) {
            const auto children = static_cast<Containment *>(applet)->applets();
            for (Applet *child : children) {
                retarget(child);
            }
        }

        disconnect(applet, &Applet::activated, currentContainment, &Applet::activated);
```
Keep whatever `resetConfigurationObject()` did that is still needed (read it: if it also resets `configLoader` or
emits `configChanged`, keep those calls after `retarget(applet)`). Do not keep its `deleteEntry("plugin")`.

- [ ] **Step 5: build + test**

Run: `cd ~/.cache/win12-kde/src/libplasma && cmake --build build --target KF6Plasma coronatest -j"$(nproc)" 2>&1 | grep -E 'error|warning: unused|Built target' | tail -5 && QT_QPA_PLATFORM=offscreen ctest --test-dir build -R coronatest --output-on-failure 2>&1 | tail -6`
Expected: `100% tests passed`.

- [ ] **Step 6: export the patch and check it applies**

```bash
cd ~/.cache/win12-kde/src/libplasma && git add -N . && git diff --no-color > /home/kalaminator/Documents/Start12/kde/patches/libplasma/0001-win12-containment-move-keeps-config.patch && git stash -q && git apply --check /home/kalaminator/Documents/Start12/kde/patches/libplasma/0001-win12-containment-move-keeps-config.patch && echo APPLIES && git stash pop -q && rm -f /home/kalaminator/Documents/Start12/kde/patches/libplasma/.keep
```
Expected: `APPLIES`; the patch touches only `src/plasma/containment.cpp`, `autotests/coronatest.h`, `autotests/coronatest.cpp`
(`grep '^diff --git' <patch>`).

- [ ] **Step 7: commit**

```bash
cd /home/kalaminator/Documents/Start12 && git add kde/patches/libplasma && git commit -m "kde/libplasma: moving an applet keeps one config level (tray icons survive a move)"
```

---

### Task 3: plasma-workspace patch — scripting index fixes + `TasksModel.appIdFilter`

Model: opus
Depends on: 1

**Files:**
- Create: `kde/patches/plasma-workspace/0001-win12-scripting-index-and-appidfilter.patch`
- Edit (in `~/.cache/win12-kde/src/plasma-workspace`): `shell/scripting/widget.cpp` (index/setIndex),
  `libtaskmanager/tasksmodel.{h,cpp}`, `libtaskmanager/autotests/CMakeLists.txt`,
  new `libtaskmanager/autotests/win12appidfiltertest.cpp`

**Interfaces (produces):** QML `TaskManager.TasksModel` properties `appIdFilter: list<string>` and
`appIdFilterExclude: bool` (Task 4 binds them); scripting `Widget.index` set/get work several times in one script.

- [ ] **Step 1: dev copy + read anchors**

```bash
S=/tmp/claude-1000/-home-kalaminator-Documents-Start12/b88e81db-1485-4a3f-ae50-6c8c3972125b/scratchpad/kde
git clone -q "$S/plasma-workspace" ~/.cache/win12-kde/src/plasma-workspace && cd ~/.cache/win12-kde/src/plasma-workspace && git describe --tags
```
Read `shell/scripting/widget.cpp` 95-145; `libtaskmanager/tasksmodel.h` 80-90 and 615-640 (how
`groupingAppIdBlacklist` is declared); `tasksmodel.cpp` 1330-1350 (its setter), 1970-1995 (`filterAcceptsRow`), and
the `class TasksModel::Private` member list (grep `groupInline` to find it); `libtaskmanager/autotests/CMakeLists.txt`
1-25; `tasksmodeltest.cpp` for how a `TasksModel` is made usable from C++ (grep `componentComplete\|classBegin`).

- [ ] **Step 2: write the failing test** `libtaskmanager/autotests/win12appidfiltertest.cpp`

```cpp
/*
    Win12: TasksModel::appIdFilter include/exclude modes (launcher rows carry an AppId, no window system needed).
    SPDX-License-Identifier: LGPL-2.1-or-later
*/
#include <QSignalSpy>
#include <QTest>

#include "abstracttasksmodel.h"
#include "tasksmodel.h"

using namespace TaskManager;

class Win12AppIdFilterTest : public QObject
{
    Q_OBJECT
    QStringList m_urls;
    static QString appId(TasksModel &m, int row) { return m.data(m.index(row, 0), AbstractTasksModel::AppId).toString(); }
    void setup(TasksModel &m) { m.componentComplete(); m.setLauncherList(m_urls); QCOMPARE(m.rowCount(), 2); }   // mirror tasksmodeltest if it differs

private Q_SLOTS:
    void initTestCase()
    {
        qputenv("XDG_DATA_DIRS", QFINDTESTDATA("data").toLocal8Bit());
        m_urls = {QUrl::fromLocalFile(QFINDTESTDATA("data/applications/org.kde.dolphin.desktop")).toString(),
                  QUrl::fromLocalFile(QFINDTESTDATA("data/applications/org.kde.konversation.desktop")).toString()};
    }
    void noFilterShowsAll()
    {
        TasksModel m; setup(m);
        m.setAppIdFilterExclude(true);   // exclude nothing = everything
        QCOMPARE(m.rowCount(), 2);
    }
    void includeMode()
    {
        TasksModel m; setup(m);
        const QString dolphin = appId(m, 0); QVERIFY(!dolphin.isEmpty());
        QSignalSpy spy(&m, &TasksModel::appIdFilterChanged);
        m.setAppIdFilter({dolphin});
        QCOMPARE(spy.count(), 1); QCOMPARE(m.rowCount(), 1); QCOMPARE(appId(m, 0), dolphin);
        m.setAppIdFilter({QStringLiteral("nothing.desktop")}); QCOMPARE(m.rowCount(), 0);
        m.setAppIdFilter({}); QCOMPARE(m.rowCount(), 2);
    }
    void excludeMode()
    {
        TasksModel m; setup(m);
        const QString dolphin = appId(m, 0);
        m.setAppIdFilterExclude(true); m.setAppIdFilter({dolphin});
        QCOMPARE(m.rowCount(), 1); QVERIFY(appId(m, 0) != dolphin);
    }
    void launcherUrlFallback()
    {
        TasksModel m; setup(m);
        const QString url = m.data(m.index(0, 0), AbstractTasksModel::LauncherUrlWithoutIcon).toUrl().toString();
        QVERIFY(!url.isEmpty());
        m.setAppIdFilter({url}); QCOMPARE(m.rowCount(), 1);
    }
};

QTEST_GUILESS_MAIN(Win12AppIdFilterTest)
#include "win12appidfiltertest.moc"
```
Register it: add `win12appidfiltertest.cpp` to the `ecm_add_tests(` list in `libtaskmanager/autotests/CMakeLists.txt`
(same link libraries as `launchertasksmodeltest`).

- [ ] **Step 3: configure + see it fail to compile**

```bash
cd ~/.cache/win12-kde/src/plasma-workspace && cmake -B build -S . -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug 2>&1 | tail -3 && cmake --build build --target win12appidfiltertest -j"$(nproc)" 2>&1 | grep -E 'error' | head -3
```
Expected: errors `no member named 'setAppIdFilter'`. (If configure fails on a test-only dependency, note the
missing package in the progress log; Task 11 will see the same, so add it to the task's `notes:`.)

- [ ] **Step 4: implement `appIdFilter`** — `tasksmodel.h` next to `groupingAppIdBlacklist`:

```cpp
    /**
     * Win12: show only the tasks of these apps (appIdFilterExclude false) or all but them (true). Entries are
     * app ids (AbstractTasksModel::AppId) or launcher URLs without icon. An empty list in include mode = no filter.
     */
    Q_PROPERTY(QStringList appIdFilter READ appIdFilter WRITE setAppIdFilter NOTIFY appIdFilterChanged)
    Q_PROPERTY(bool appIdFilterExclude READ appIdFilterExclude WRITE setAppIdFilterExclude NOTIFY appIdFilterExcludeChanged)
```
plus public `QStringList appIdFilter() const; void setAppIdFilter(const QStringList &list); bool appIdFilterExclude() const;
void setAppIdFilterExclude(bool exclude);` and signals `void appIdFilterChanged() const; void appIdFilterExcludeChanged() const;`.
`tasksmodel.cpp` Private members `QStringList appIdFilter; bool appIdFilterExclude = false;`, methods:

```cpp
QStringList TasksModel::appIdFilter() const
{
    return d->appIdFilter;
}

void TasksModel::setAppIdFilter(const QStringList &list)
{
    if (d->appIdFilter == list) {
        return;
    }
    d->appIdFilter = list;
    invalidateFilter();
    Q_EMIT appIdFilterChanged();
}

bool TasksModel::appIdFilterExclude() const
{
    return d->appIdFilterExclude;
}

void TasksModel::setAppIdFilterExclude(bool exclude)
{
    if (d->appIdFilterExclude == exclude) {
        return;
    }
    d->appIdFilterExclude = exclude;
    invalidateFilter();
    Q_EMIT appIdFilterExcludeChanged();
}
```
and in `filterAcceptsRow`, right after `const QString &appName = ...;`:

```cpp
    // Win12 islands: hide a task that is not in the list (include mode) or is in it (exclude mode).
    if (!d->appIdFilter.isEmpty()) {
        const QString url = sourceIndex.data(AbstractTasksModel::LauncherUrlWithoutIcon).toUrl().toString();
        const bool listed = (!appId.isEmpty() && d->appIdFilter.contains(appId)) || (!url.isEmpty() && d->appIdFilter.contains(url));
        if (listed == d->appIdFilterExclude) {
            return false;
        }
    }
```

- [ ] **Step 5: `widget.cpp`** — both `split(u';')` calls become `split(u';', Qt::SkipEmptyParts)` (an empty
  `AppletOrder` gave a fake id 0), and the clamp becomes `order.insert(std::min(std::max(0, index), int(order.length())), d->applet->id());`
  (the item was removed first, so `length()` is the last slot). Add one comment line above each: `// Win12: ...`.

- [ ] **Step 6: build + test**

Run: `cd ~/.cache/win12-kde/src/plasma-workspace && cmake --build build --target win12appidfiltertest -j"$(nproc)" 2>&1 | grep -E 'error|Built target' | tail -3 && QT_QPA_PLATFORM=offscreen ctest --test-dir build -R win12 --output-on-failure 2>&1 | tail -6`
Expected: `100% tests passed`. Then one more blocking call (<= 10 min, skip if it does not finish) to compile the shell:
`cmake --build build --target plasmashell -j"$(nproc)" 2>&1 | grep -E ' error |Built target plasmashell' | tail -3`.

- [ ] **Step 7: export + check**

```bash
cd ~/.cache/win12-kde/src/plasma-workspace && git add -N . && git diff --no-color > /home/kalaminator/Documents/Start12/kde/patches/plasma-workspace/0001-win12-scripting-index-and-appidfilter.patch && git stash -q && git apply --check /home/kalaminator/Documents/Start12/kde/patches/plasma-workspace/0001-win12-scripting-index-and-appidfilter.patch && echo APPLIES && git stash pop -q && rm -f /home/kalaminator/Documents/Start12/kde/patches/plasma-workspace/.keep && grep '^diff --git' /home/kalaminator/Documents/Start12/kde/patches/plasma-workspace/*.patch
```
Expected: `APPLIES`, 5 files (widget.cpp, tasksmodel.h, tasksmodel.cpp, autotests/CMakeLists.txt, the test).

- [ ] **Step 8: commit**

```bash
cd /home/kalaminator/Documents/Start12 && git add kde/patches/plasma-workspace && git commit -m "kde/plasma-workspace: several Widget.index sets per script, TasksModel.appIdFilter for islands"
```

---

### Task 4: plasma-desktop patch — immediate panel save, island registry, task manager drag/drop

Model: opus
Depends on: 1

**Files:**
- Create: `kde/patches/plasma-desktop/0001-win12-panel-save-and-islands.patch`
- Edit (in `~/.cache/win12-kde/src/plasma-desktop`): `containments/panel/main.qml` (~line 101),
  `applets/taskmanager/CMakeLists.txt`, `applets/taskmanager/main.xml`, `applets/taskmanager/qml/main.qml`,
  `qml/Task.qml` (~512), `qml/MouseHandler.qml` (`onPositionChanged`, `onDropped`);
  new `applets/taskmanager/win12islandregistry.{h,cpp}`, `applets/taskmanager/autotests/CMakeLists.txt`,
  `applets/taskmanager/autotests/win12islandregistrytest.cpp`

**Interfaces:**
- Consumes: `TasksModel.appIdFilter` / `appIdFilterExclude` (Task 3; QML property names only, no compile dependency).
- Produces: task manager config key `win12Island` (String: `pinned` | `open` | `tray` | empty = stock), file
  `~/.config/win12islandsrc` `[Apps] <appid>=<island>`, drag mime `text/x-win12-appid` + `text/x-win12-launcher`.
  Task 6 writes `win12Island` and `launchers` through scripting.

- [ ] **Step 1: dev copy + anchors**

```bash
S=/tmp/claude-1000/-home-kalaminator-Documents-Start12/b88e81db-1485-4a3f-ae50-6c8c3972125b/scratchpad/kde
git clone -q "$S/plasma-desktop" ~/.cache/win12-kde/src/plasma-desktop && cd ~/.cache/win12-kde/src/plasma-desktop && git describe --tags
```
Read: `containments/panel/main.qml` 88-104; `applets/taskmanager/CMakeLists.txt` whole (~70 lines); `backend.h`
30-50 (how `Backend` is exposed to QML: `QML_ELEMENT`, imported in QML as `TaskManagerApplet`); `qml/main.qml`
130-240 (tasksModel block, `launcherList`/`launchers` handlers) and 360-400 (`dragHelper`, `onUrlsDropped`);
`qml/MouseHandler.qml` 1-60 and 60-125; `qml/Task.qml` 500-520; where the applet's `Layout.preferredWidth`/
`minimumWidth` is computed (grep `Layout.minimumWidth` in `qml/main.qml`).

- [ ] **Step 2: registry test first** — `applets/taskmanager/autotests/CMakeLists.txt`:

```cmake
# Win12: the island registry (app id -> island) shared by every task manager in a plasmashell
ecm_add_test(win12islandregistrytest.cpp ../win12islandregistry.cpp
    TEST_NAME win12islandregistrytest
    LINK_LIBRARIES Qt::Test Qt::Qml KF6::ConfigCore PW::LibTaskManager)
```
and in `applets/taskmanager/CMakeLists.txt` (end): `if(BUILD_TESTING)\n    add_subdirectory(autotests)\nendif()`.
`win12islandregistrytest.cpp`:

```cpp
// Win12: island registry — assign/query/persist, missing file = empty.  SPDX-License-Identifier: GPL-2.0-or-later
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "../win12islandregistry.h"

class Win12IslandRegistryTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void assignQueryPersist()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath(QStringLiteral("win12islandsrc"));
        Win12IslandRegistry r(file);
        QSignalSpy spy(&r, &Win12IslandRegistry::changed);
        QCOMPARE(r.islandOf(QStringLiteral("a.desktop")), QString());
        r.assign(QStringLiteral("a.desktop"), QStringLiteral("tray"));
        r.assign(QStringLiteral("b.desktop"), QStringLiteral("pinned"));
        r.assign(QStringLiteral("b.desktop"), QStringLiteral("pinned"));   // same value: no signal
        QCOMPARE(spy.count(), 2);
        QCOMPARE(r.appsFor(QStringLiteral("tray")), QStringList{QStringLiteral("a.desktop")});
        QCOMPARE(r.appsNotFor(QStringLiteral("open")).size(), 2);
        QCOMPARE(r.appsNotFor(QStringLiteral("tray")), QStringList{QStringLiteral("b.desktop")});
        r.assignLaunchers({QStringLiteral("applications:c.desktop")}, QStringLiteral("pinned"));
        QVERIFY(r.appsFor(QStringLiteral("pinned")).size() >= 1);   // c resolves to an app id when installed, else is skipped
        r.assign(QStringLiteral("a.desktop"), QString());
        QCOMPARE(r.islandOf(QStringLiteral("a.desktop")), QString());
        Win12IslandRegistry again(file);
        QCOMPARE(again.islandOf(QStringLiteral("b.desktop")), QStringLiteral("pinned"));
    }
    void missingFileIsEmpty()
    {
        Win12IslandRegistry r(QStringLiteral("/nonexistent-win12/dir/win12islandsrc"));
        QCOMPARE(r.appsNotFor(QStringLiteral("open")), QStringList());
        QCOMPARE(r.islandOf(QStringLiteral("x")), QString());
    }
};

QTEST_GUILESS_MAIN(Win12IslandRegistryTest)
#include "win12islandregistrytest.moc"
```

- [ ] **Step 3: configure + see it fail**

Run: `cd ~/.cache/win12-kde/src/plasma-desktop && cmake -B build -S . -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug 2>&1 | tail -3`
Expected: configure error (missing `win12islandregistry.cpp`) or compile error on the test target.

- [ ] **Step 4: registry class** — `win12islandregistry.h`:

```cpp
// Win12 islands: which app lives in which island, shared by every task manager of this plasmashell
// (one QML singleton = one process-wide object). File ~/.config/win12islandsrc, group [Apps], appid=island.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QStringList>

#include <KSharedConfig>

class Win12IslandRegistry : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Win12Islands)
    QML_SINGLETON
public:
    explicit Win12IslandRegistry(const QString &file = QString(), QObject *parent = nullptr);
    static Win12IslandRegistry *create(QQmlEngine *, QJSEngine *);

    Q_INVOKABLE QString islandOf(const QString &appId) const;
    Q_INVOKABLE void assign(const QString &appId, const QString &island);   // empty island = forget the app
    Q_INVOKABLE QStringList appsFor(const QString &island) const;
    Q_INVOKABLE QStringList appsNotFor(const QString &island) const;   // assigned to any other island
    Q_INVOKABLE QString appIdOf(const QString &launcherUrl) const;   // TaskManager::appDataFromUrl(url).id
    Q_INVOKABLE void assignLaunchers(const QStringList &launcherUrls, const QString &island);

Q_SIGNALS:
    void changed();

private:
    KConfigGroup apps() const;
    KSharedConfig::Ptr m_config;
};
```
`win12islandregistry.cpp`:

```cpp
#include "win12islandregistry.h"

#include <QUrl>

#include <KConfigGroup>

#include <taskmanager/tasktools.h>

Win12IslandRegistry::Win12IslandRegistry(const QString &file, QObject *parent)
    : QObject(parent)
    , m_config(KSharedConfig::openConfig(file.isEmpty() ? QStringLiteral("win12islandsrc") : file, KConfig::SimpleConfig))
{
}

Win12IslandRegistry *Win12IslandRegistry::create(QQmlEngine *, QJSEngine *)
{
    // ponytail: one plasmashell = one registry; a static keeps it shared even with several QML engines
    static Win12IslandRegistry *instance = new Win12IslandRegistry();
    QJSEngine::setObjectOwnership(instance, QJSEngine::CppOwnership);
    return instance;
}

KConfigGroup Win12IslandRegistry::apps() const
{
    return m_config->group(QStringLiteral("Apps"));
}

QString Win12IslandRegistry::islandOf(const QString &appId) const
{
    return appId.isEmpty() ? QString() : apps().readEntry(appId, QString());
}

void Win12IslandRegistry::assign(const QString &appId, const QString &island)
{
    if (appId.isEmpty() || islandOf(appId) == island) {
        return;
    }
    KConfigGroup g = apps();
    if (island.isEmpty()) {
        g.deleteEntry(appId);
    } else {
        g.writeEntry(appId, island);
    }
    m_config->sync();
    Q_EMIT changed();
}

QStringList Win12IslandRegistry::appsFor(const QString &island) const
{
    QStringList out;
    const auto map = apps().entryMap();
    for (auto it = map.cbegin(); it != map.cend(); ++it) {
        if (it.value() == island) {
            out << it.key();
        }
    }
    return out;
}

QStringList Win12IslandRegistry::appsNotFor(const QString &island) const
{
    QStringList out;
    const auto map = apps().entryMap();
    for (auto it = map.cbegin(); it != map.cend(); ++it) {
        if (it.value() != island && !it.value().isEmpty()) {
            out << it.key();
        }
    }
    return out;
}

QString Win12IslandRegistry::appIdOf(const QString &launcherUrl) const
{
    return TaskManager::appDataFromUrl(QUrl(launcherUrl)).id;
}

void Win12IslandRegistry::assignLaunchers(const QStringList &launcherUrls, const QString &island)
{
    for (const QString &url : launcherUrls) {
        assign(appIdOf(url), island);
    }
}
```
Add `win12islandregistry.cpp` to `CPP_SOURCES` in `applets/taskmanager/CMakeLists.txt`. Check the include path of
`tasktools.h` against how `backend.cpp` includes libtaskmanager headers and the `AppData` field name for the id
(grep `struct AppData` in the plasma-workspace clone's `libtaskmanager/tasktools.h`; use that field).

- [ ] **Step 5: build + run the registry test**

Run: `cd ~/.cache/win12-kde/src/plasma-desktop && cmake -B build -S . -DBUILD_TESTING=ON >/dev/null 2>&1; cmake --build build --target win12islandregistrytest -j"$(nproc)" 2>&1 | grep -E 'error|Built target' | tail -3 && ctest --test-dir build -R win12 --output-on-failure 2>&1 | tail -5`
Expected: `100% tests passed`.

- [ ] **Step 6: panel immediate save** — `containments/panel/main.qml` in `Containment.onAppletAdded`: replace
  `Qt.callLater(LayoutManager.save);` with `LayoutManager.save();   // Win12: scripting reads AppletOrder in the same script` (only this site).

- [ ] **Step 7: task manager config + QML** — `main.xml` after the `launchers` entry:

```xml
    <entry name="win12Island" type="String">
      <label>Win12: the island this task manager is (pinned | open | tray); empty = behave like stock</label>
      <default></default>
    </entry>
```
`qml/main.qml`, in the applet root (`tasks`), near `dragSource`:

```qml
    // Win12 islands (config key win12Island): the shared registry (TaskManagerApplet.Win12Islands) says which app
    // shows where. open = every app not given to another island; pinned/tray = the apps given to it. This instance's
    // launchers are given to its island, so a pinned app always shows next to its launcher. Empty key = stock.
    readonly property string win12Island: Plasmoid.configuration.win12Island
    function win12Register() {
        if (win12Island) {
            TaskManagerApplet.Win12Islands.assignLaunchers(tasksModel.launcherList, win12Island);
        }
    }
    function win12Refilter() {
        const r = TaskManagerApplet.Win12Islands;
        tasksModel.appIdFilterExclude = win12Island === "open";
        tasksModel.appIdFilter = !win12Island ? [] : win12Island === "open" ? r.appsNotFor("open") : r.appsFor(win12Island);
        if (!win12Island) {
            return;
        }
        // a launcher the registry now gives to another island leaves this one (the drop target added its own)
        for (const url of tasksModel.launcherList) {
            const isl = r.islandOf(r.appIdOf(url));
            if (isl && isl !== win12Island) {
                tasksModel.requestRemoveLauncher(url);
            }
        }
    }
    onWin12IslandChanged: { win12Register(); win12Refilter(); }
    Connections {
        target: TaskManagerApplet.Win12Islands
        function onChanged(): void { tasks.win12Refilter(); }
    }
```
In the `tasksModel` block add `onLauncherListChanged: tasks.win12Register()` next to the existing launcherList
handler (merge if one exists), and in the applet's `Component.onCompleted` (after the model is complete) call
`win12Register(); win12Refilter();`. Empty island drop target: `Layout.minimumWidth` (main.qml ~57) already
returns `LayoutMetrics.preferredMinWidth()` unless `shouldShrinkToZero`; find that property's definition (grep
`shouldShrinkToZero:`) and add `&& !tasks.win12Island` to its expression with the comment
`// Win12: an empty island task manager keeps one slot so it stays a drop target`.

- [ ] **Step 8: drag mime + drop** — `qml/Task.qml` `dragHelper.Drag.mimeData` gets two more entries:

```qml
                            // Win12: islands move the app (and its launcher when pinned) on a drop
                            "text/x-win12-appid": model.AppId || backend.tryDecodeApplicationsUrl(model.LauncherUrlWithoutIcon).toString(),
                            "text/x-win12-launcher": tasksModel.launcherPosition(model.LauncherUrlWithoutIcon) !== -1
                                ? backend.tryDecodeApplicationsUrl(model.LauncherUrlWithoutIcon).toString() : "",
```
`qml/MouseHandler.qml`: at the top of `onPositionChanged` (before the manual-sort block):

```qml
        if (!tasks.dragSource && drag.formats.indexOf("text/x-win12-appid") >= 0) {   // Win12: an island drag never hover-activates
            activationTimer.stop();
            return;
        }
```
and at the top of `onDropped`:

```qml
        // Win12: a task from another island task manager: give the app to this island (the registry re-filters
        // every instance), add its launcher when it was pinned, put it at the drop position.
        if (!tasks.dragSource && tasks.win12Island && event.formats.indexOf("text/x-win12-appid") >= 0) {
            const appId = event.getDataAsString("text/x-win12-appid");
            if (!appId) {
                event.accepted = false;
                return;
            }
            const above = target.itemAt ? target.itemAt(event.x - rtlSpacerAdjustment, event.y) : target.childAt(event.x - rtlSpacerAdjustment, event.y);   // same lookup as onPositionChanged
            const insertAt = above && above.index !== undefined ? above.index : tasksModel.count;
            TaskManagerApplet.Win12Islands.assign(appId, tasks.win12Island);
            const url = event.getDataAsString("text/x-win12-launcher");
            if (url) {
                tasksModel.requestAddLauncher(url);
            }
            if (tasksModel.sortMode === TaskManager.TasksModel.SortManual) {
                for (let i = 0; i < tasksModel.count; ++i) {
                    const idx = tasksModel.index(i, 0);
                    if (tasksModel.data(idx, TaskManager.AbstractTasksModel.AppId) === appId
                        || String(tasksModel.data(idx, TaskManager.AbstractTasksModel.LauncherUrlWithoutIcon)) === appId) {
                        if (i !== insertAt) {
                            tasksModel.move(i, Math.min(insertAt, tasksModel.count - 1));
                        }
                        break;
                    }
                }
            }
            event.accepted = true;
            return;
        }
```
Check the file's imports (`TaskManager`, `TaskManagerApplet`) and the enum spelling the file already uses for
`SortManual`.

- [ ] **Step 9: QML syntax check + build the plugin**

Run: `cd ~/.cache/win12-kde/src/plasma-desktop && cmake --build build --target org.kde.plasma.taskmanager -j"$(nproc)" 2>&1 | grep -E 'error|Built target org.kde.plasma.taskmanager' | tail -3; for f in applets/taskmanager/qml/main.qml applets/taskmanager/qml/MouseHandler.qml applets/taskmanager/qml/Task.qml containments/panel/main.qml; do /usr/lib/qt6/bin/qmllint "$f" 2>&1 | grep -E 'Error|error' | head -2; done; echo LINT_DONE`
Expected: `Built target`, no `Error` lines from qmllint beyond unresolved-import noise (the build also runs the
module's qmllint when enabled).

- [ ] **Step 10: export + check + commit**

```bash
cd ~/.cache/win12-kde/src/plasma-desktop && git add -N . && git diff --no-color > /home/kalaminator/Documents/Start12/kde/patches/plasma-desktop/0001-win12-panel-save-and-islands.patch && git stash -q && git apply --check /home/kalaminator/Documents/Start12/kde/patches/plasma-desktop/0001-win12-panel-save-and-islands.patch && echo APPLIES && git stash pop -q && rm -f /home/kalaminator/Documents/Start12/kde/patches/plasma-desktop/.keep
cd /home/kalaminator/Documents/Start12 && git add kde/patches/plasma-desktop && git commit -m "kde/plasma-desktop: immediate panel save, Win12 island registry, task manager drag between islands"
```

---

### Task 5: Win12 `islandlogic.js` + `tests/test_islands.js` (pure tests + fake-shell harness)

Model: opus
Depends on: -

**Files:**
- Create: `package/contents/ui/code/islandlogic.js`, `tests/test_islands.js`
- Modify: `install.sh` (run the new test next to `test_tiles.js`)

**Interfaces (produces):** plain functions (no `.pragma library`, no exports; the file is concatenated in front of
`islands.js` by `sed` in Task 6): `roles(pos)`, `trayOrder(pos, tm, weather, moved)`,
`listsFor(cur, wantHost, hostList, otherList)`, `mergeLists(pinned, open, tray)`. Test helpers `shell()`,
`runIslands(sh, subs)`, `passes(sh, subs, max)` used again in Task 6.

- [ ] **Step 1: write `tests/test_islands.js`** (pure tests first; the harness section runs the CURRENT islands.js)

```js
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
```

- [ ] **Step 2: run it, see it fail**

Run: `node tests/test_islands.js 2>&1 | tail -3`
Expected: FAIL `Cannot find module`/ENOENT for `islandlogic.js`.

- [ ] **Step 3: write `package/contents/ui/code/islandlogic.js`**

```js
// Pure helpers for code/islands.js (main.qml syncIslands puts this file in front of it) and tests/test_islands.js.
// No .pragma library, no exports: plain functions, read by the plasmashell script engine and by node.
// roles(pos): pos 0 Left, 1 Center, 2 Right (Start position). The host panel holds Start (and the win12Layout key);
// A = the other app island, T = the tray island. Table in the spec (5.2): Center puts Start with the open apps.
function roles(pos) {
    return { host: pos === 1 ? "open" : "pinned", other: pos === 1 ? "pinned" : "open",
             row: pos === 2 ? ["T", "host", "A"] : pos === 1 ? ["A", "host", "T"] : ["host", "A", "T"] };
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
```

- [ ] **Step 4: run; the pure part passes, the harness part exercises today's islands.js**

Run: `node tests/test_islands.js 2>&1 | tail -3`
Expected: `test_islands OK`. If the harness part fails on today's script, fix the FAKE (it must model Plasma: a
removed widget lingers in `widgets()`, `addWidget(widget)` moves to the end, `index` set is live) until it passes;
do not touch islands.js in this task. Note any fake behaviour you had to add in the progress log.

- [ ] **Step 5: `install.sh`** — change the node line to
  `if command -v node >/dev/null 2>&1; then node tests/test_tiles.js && node tests/test_islands.js; fi`.

- [ ] **Step 6: commit**

```bash
git add package/contents/ui/code/islandlogic.js tests/test_islands.js install.sh
git commit -m "islands: pure layout helpers + a node harness that runs islands.js against a fake plasmashell"
```

---

### Task 6: Win12 `islands.js` fast engine + `islands` mode; `syncCenterPanel` in one pass

Model: opus
Depends on: 5

**Files:**
- Modify: `package/contents/ui/code/islands.js` (rewrite, ~90 lines), `package/contents/ui/main.qml`
  (`syncIslands` ~684-691: `@POS@` + both files; new `islandPos`; `syncCenterPanel` ~706-744: all index sets in one
  pass), `package/contents/config/main.xml` (`taskbarLayout` label lists `islands`), `tests/test_islands.js` (new cases)

**Interfaces:**
- Consumes: `roles/trayOrder/listsFor/mergeLists` (Task 5); task manager config key `win12Island` + `launchers`
  (Task 4); patched KDE behaviour (all moves/index sets in one script).
- Produces: `taskbarLayout` value `islands`; host panel key `win12Layout` JSON gains `A`, `T`, `atm`, `ttm`, `weather`;
  host key `win12TrayClean`. Task 7 adds the settings entry.

- [ ] **Step 1: add the failing harness cases** to `tests/test_islands.js` before the final `console.log`:

```js
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
assert.strictEqual(cfgOf(tmT, "win12Island"), "tray");
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
assert.ok(T.offset < sh.host.offset && sh.host.offset < A.offset, "magnetic row T, host, A");
// back to One bar: one panel, old order, the pins merged back, stock task manager again
tmT.writeConfig("launchers", "z.desktop");   // a pin dropped into the tray island comes back too (pinned first)
assert.ok(passes(sh, sub(sh, "single")) <= 3, "single converges fast");
assert.strictEqual(sh.panels().length, 1);
assert.deepStrictEqual(types(sh.host), ["w12:menu", "icontasks", "marginsseparator", "systemtray", "digitalclock", "showdesktop"]);
assert.strictEqual(cfgOf(sh.tm, "win12Island"), ""); assert.strictEqual(cfgOf(sh.tm, "launchers"), "a.desktop,b.desktop,z.desktop");
// splits still converge fast, and islands -> split-left goes through the restore
sh = shell(); assert.ok(passes(sh, sub(sh, "split-left")) <= 3); assert.ok(passes(sh, sub(sh, "islands", 0)) <= 5); assert.ok(passes(sh, sub(sh, "split-left")) <= 6);
// no task manager in the host (taken out for real, not a lingering removal): islands does nothing
sh = shell(); sh.host.ws.splice(sh.host.ws.indexOf(sh.tm), 1); sh.tm.panel = null;
assert.strictEqual(runIslands(sh, sub(sh, "islands", 0)), "done"); assert.strictEqual(sh.panels().length, 1);
```

- [ ] **Step 2: run, see them fail**

Run: `node tests/test_islands.js 2>&1 | tail -3` — Expected: AssertionError at `islands Left converges fast` (today's
script knows no `islands` mode).

- [ ] **Step 3: rewrite `package/contents/ui/code/islands.js`**

```js
// W5/W17: Windows 12 taskbar layouts (key taskbarLayout), run by main.qml syncIslands() through plasmashell
// evaluateScript until it prints "done". Needs Win12's patched KDE (kde/): moves, adds and index sets all land in
// one pass (immediate panel save, Widget.index fixes), so a switch is 2-3 passes; the loop is the safety net.
// code/islandlogic.js (roles, trayOrder, listsFor, mergeLists) is put in front of this file by syncIslands.
// @ID@ = this widget's id, @WANT@ = layout, @POS@ = Start position (0 left, 1 centre, 2 right), @MAG@ = taskbarMagnetic, @GAP@ = taskbarGap.
// The host panel (holds this widget) keeps the user's lengthMode/alignment/order and the ids moved out in its
// win12Layout key; island panels carry win12Island = L/R (splits), A (other app island), T (tray island).
// split-left: L = fresh weather, host = Start + tasks, R = the widgets after the last Task Manager (tray, clock, ...).
// split-right: host left, R = weather + those widgets. islands: see roles() (table in the spec, 5.2): a Start
// position change re-places the panels and swaps the two app task managers' win12Island roles + launchers.
// single (or another look): widgets moved back, islands removed, order/length/alignment/offset put back.
// magnetic: all panels 'custom' length (fits content, the only mode where Plasma 6.8 uses offset), left aligned,
// offsets so the group sits centred with GAP px between islands; each panel carries an org.win12.reveal helper
// (0 px) that the win12islandreveal KWin script uses to show all the islands together.
const ID = @ID@, WANT = '@WANT@', POS = @POS@, MAG = @MAG@, GAP = @GAP@, HELPER = 'org.win12.reveal'; let out = 'done';
const get = (o, g, k, d) => { o.currentConfigGroup = [g]; return String(o.readConfig(k, d)); };
const put = (o, k, v) => { o.currentConfigGroup = ['General']; o.writeConfig(k, v); };
let host = null, gone = []; const live = q => q.widgets().filter(w => gone.indexOf(w.id) < 0);
const isTM = w => /icontasks|taskmanager/.test(w.type), byId = (q, id) => live(q).find(w => w.id === id);
// live widgets of q in this order (ids not in q skipped); true when something moved. Setting index k for k = 0.. in
// order converges in one go on the patched shell (each set is saved at once).
const order = (q, ids) => { let ch = false; ids.filter(id => byId(q, id)).forEach((id, i) => { const w = byId(q, id); if (w.index !== i) { w.index = i; ch = true; } }); return ch; };
// W16/W17: a tray moved before the patch nested its config one level per move. The first tray move after the patch
// still uses a fresh tray (takes the old one's settings and its id in the layout) as the one-time cleanup; the host
// then gets win12TrayClean=true and later moves are normal moves (libplasma patch keeps one config level).
const tray = (q, w, s) => { if (!/systemtray/.test(w.type) || get(host, 'General', 'win12TrayClean', '') === 'true') return q.addWidget(w);
 const n = q.addWidget(w.type); w.currentConfigGroup = n.currentConfigGroup = ['General'];
 for (const k of w.configKeys) n.writeConfig(k, w.readConfig(k)); n.reloadConfig();
 const re = a => a.map(i => i === w.id ? n.id : i); s.moved = re(s.moved); s.order = re(s.order); put(host, 'win12Layout', JSON.stringify(s));
 gone.push(w.id); put(host, 'win12Gone', gone.filter(i => i).join(';')); put(host, 'win12TrayClean', 'true'); w.remove(); return n; };
for (const p of panels()) { const me = p.widgets().find(w => w.id === ID); if (!me) continue; host = p;
 const want = WANT;   // main.qml islandWant: the taskbar look is Windows 12, else 'single'
 // W15: wait until syncPanelLook has put the gap mode's thickness back (islands copy the panel height and layout)
 if (want !== 'single' && get(p, 'General', 'win12Saved_height', '')) { out = 'again'; continue; }
 gone = get(p, 'General', 'win12Gone', '').split(';').map(Number);
 const raw = get(p, 'General', 'win12Layout', ''); let s = raw ? JSON.parse(raw) : null;
 const isl = k => s && s[k] ? panels().find(q => q.id === s[k]) : null;
 const mode = s && s.mode !== want ? 'single' : want;
 if (mode === 'single') { if (!s) continue; out = 'again';
  // everything back in one pass: helpers out, pins merged into the original task manager (pinned island first),
  // moved widgets home in their old order, island panels removed; a switch between looks restores first.
  const isls = ['L', 'R', 'A', 'T'].map(isl).filter(q => q);
  for (const q of [p].concat(isls)) q.widgets(HELPER).forEach(w => w.remove());
  const tm = live(p).find(isTM);
  if (tm && s.atm) { const tms = [tm].concat([s.atm, s.ttm].map(id => isls.map(q => byId(q, id)).find(w => w)).filter(w => w));
   const of = r => tms.filter(w => get(w, 'General', 'win12Island', '') === r).map(w => get(w, 'General', 'launchers', '')).join(',');
   put(tm, 'launchers', mergeLists(of('pinned'), of('open'), of('tray'))); put(tm, 'win12Island', ''); tm.reloadConfig(); }
  for (const id of s.moved) { const w = isls.map(q => byId(q, id)).find(w => w); if (w) tray(p, w, s); }
  for (const q of isls) q.remove();
  const ids = live(p).map(w => w.id); order(p, s.order.filter(i => ids.indexOf(i) >= 0).concat(ids.filter(i => s.order.indexOf(i) < 0)));
  if (want !== 'single') { s.mode = want; for (const k of ['L', 'R', 'A', 'T', 'atm', 'ttm', 'weather']) delete s[k]; put(p, 'win12Layout', JSON.stringify(s)); continue; }
  p.lengthMode = s.lm; p.alignment = s.al; if (s.mx) { p.maximumLength = s.mx; p.minimumLength = s.mn; p.offset = s.off; }
  put(p, 'win12Layout', ''); continue; }
 const tm = live(p).find(isTM);
 if (mode === 'islands' && !tm) continue;   // no task manager in the bar: nothing to split into islands
 if (!s) { const ws = live(p).sort((a, b) => a.index - b.index), t = ws.map(isTM).lastIndexOf(true);
  s = { mode: want, lm: p.lengthMode, al: p.alignment, mn: p.minimumLength, mx: p.maximumLength, off: p.offset, order: ws.map(w => w.id),
   moved: ws.slice(t + 1).filter(w => !/marginsseparator|panelspacer/.test(w.type)).map(w => w.id) }; out = 'again'; }
 const mk = (k, al) => { const q = new Panel(); q.location = p.location; q.height = p.height; q.lengthMode = 'fit'; q.alignment = al; put(q, 'win12Island', k); s[k] = q.id; out = 'again'; return q; };
 if (mode === 'islands') {
  if (!isl('A')) s.atm = mk('A', 'center').addWidget(tm.type).id;
  if (!isl('T')) { const q = mk('T', 'right'); s.ttm = q.addWidget(tm.type).id; s.weather = q.addWidget('org.kde.plasma.weather').id; }
 } else for (const k of mode === 'split-left' ? ['L', 'R'] : ['R']) if (!isl(k)) { const q = mk(k, k === 'L' ? 'left' : 'right'); if (k === 'L' || mode === 'split-right') q.addWidget('org.kde.plasma.weather'); }
 put(p, 'win12Layout', JSON.stringify(s));
 const dst = mode === 'islands' ? isl('T') : isl('R');
 for (const id of s.moved) { const w = byId(p, id); if (w) { tray(dst, w, s); out = 'again'; } }
 const lost = live(dst).find(w => w.index < 0);   // safety net: an unplaced widget goes out and in again
 if (lost) { p.addWidget(lost); out = 'again'; continue; }
 const R = roles(POS);
 if (order(dst, mode === 'islands' ? trayOrder(POS, s.ttm, s.weather, s.moved) : live(dst).filter(w => s.moved.indexOf(w.id) < 0).sort((a, b) => a.index - b.index).map(w => w.id).concat(s.moved))) out = 'again';
 if (mode === 'islands') { const atm = byId(isl('A'), s.atm), ttm = byId(isl('T'), s.ttm), cur = get(tm, 'General', 'win12Island', '');
  if (atm && ttm && cur !== R.host) {   // roles (and the pins with them) follow the Start position; written, then reloaded, so each applet sees its final pair
   const [lh, la] = listsFor(cur, R.host, get(tm, 'General', 'launchers', ''), get(atm, 'General', 'launchers', ''));
   put(tm, 'launchers', lh); put(tm, 'win12Island', R.host); put(atm, 'launchers', la); put(atm, 'win12Island', R.other); put(ttm, 'win12Island', 'tray');
   tm.reloadConfig(); atm.reloadConfig(); ttm.reloadConfig(); out = 'again'; } }
 // islands look like the panel; floating/hiding from the keys (syncPanelLook may not have reached the panel yet).
 const W = { floating: get(me, 'XP', 'taskbarFloating', true) !== 'false', hiding: [get(me, 'XP', 'taskbarFloating', true) !== 'false' ? 'autohide' : p.hiding, 'dodgewindows', 'autohide'][Number(get(me, 'XP', 'taskbarHiding', 0))] || p.hiding, height: p.height };
 const others = ['L', 'R', 'A', 'T'].map(isl).filter(q => q);
 for (const q of [p].concat(others)) if (!q.widgets(HELPER).length) { q.addWidget(HELPER); out = 'again'; }
 // W16: Plasma re-checks "a non-normal panel never de-floats" only when floating changes (W7): nudge after a change.
 for (const q of others) { let ch = false; for (const k in W) if (q[k] !== W[k]) { q[k] = W[k]; ch = true; } if (ch && q.floating) { q.floating = false; q.floating = true; } }
 // placement: lengths are read back (a floating window adds 8 px padding each side); the pass after a change re-checks them
 const row = mode === 'islands' ? R.row.map(k => k === 'host' ? p : isl(k)) : mode === 'split-left' ? [isl('L'), p, isl('R')] : [p, isl('R')], sw = screenGeometry(p.screen).width;
 let x = Math.round((sw - row.reduce((t, q) => t + q.length, 0) - GAP * (row.length - 1)) / 2);
 row.forEach((q, i) => { const lm = MAG ? 'custom' : 'fit', al = MAG ? 'left' : ['left', 'center', 'right'][row.length === 2 ? i * 2 : i], o = Math.max(0, x - (q.floating ? 8 : 0));
  x += q.length + GAP;
  if (q.lengthMode !== lm || q.alignment !== al || MAG && (q.minimumLength > 0 || q.offset !== o)) { q.lengthMode = lm; q.alignment = al; if (MAG) { q.minimumLength = 0; q.offset = o; } out = 'again'; } }); }
print(out);
```

- [ ] **Step 4: `main.qml`** — in `syncIslands()` the `sed` gets `-e s/@POS@/" + cfg.menuPosition + "/` and both
  files: `... Tiles.shQuote(logic) + " " + Tiles.shQuote(file) + ")\""` where
  `const logic = decodeURIComponent(Qt.resolvedUrl("code/islandlogic.js").toString().replace("file://", ""));`.
  Add after `islandGap`:

```qml
    // W17: the 3 Islands layout follows the Start position (roles + launchers swap, panels re-placed).
    readonly property int islandPos: islandWant === "islands" ? cfg.menuPosition : -1
    onIslandPosChanged: syncIslands()
```
  Update the comment block above `panelMode` (line ~596-600: "one move per pass" is gone: all index sets land in
  one pass on the patched shell, the next pass only verifies). In `syncCenterPanel()` replace the two statements
  `const i = want.findIndex(...)` ... `if (i >= 0) { byId(want[i]).index = i;` with:

```js
            + " const bad = want.some((id, k) => byId(id).index < 0 || cur[k] !== id);"
            + " if (!bad && mode !== 2 && saved.length) { p.currentConfigGroup = ['General']; p.writeConfig('win12PanelOrder', ''); }"
            + " if (bad) { want.forEach((id, k) => { if (byId(id).index !== k) byId(id).index = k; });"
```
  (the rest of that block, the spacer `expanding` flip and `out = 'again'`, stays). `main.xml` `taskbarLayout` label:
  `Windows 12 taskbar layout: single | split-left | split-right | islands (3 islands: pinned, open apps, tray)`.

- [ ] **Step 5: tests + QML check**

Run: `node tests/test_islands.js 2>&1 | tail -3 && node tests/test_tiles.js | tail -1 && /usr/lib/qt6/bin/qmllint package/contents/ui/main.qml 2>&1 | grep -c -E '^Error|error:' ; echo QML_CHECKED`
Expected: `test_islands OK`, no new qmllint errors compared to `git stash; qmllint ...; git stash pop` (same count).
If a convergence assert needs 4 passes for a good reason, raise that one bound and say why in the progress log.

- [ ] **Step 6: commit**

```bash
git add package/contents/ui/code/islands.js package/contents/ui/main.qml package/contents/config/main.xml tests/test_islands.js
git commit -m "islands: one-pass engine on the patched shell + the 3 Islands layout (roles follow the Start position)"
```

---

### Task 7: Win12 `kdePatched` gate + "3 Islands" entry and warning in settings

Model: opus
Depends on: 6

**Files:**
- Modify: `package/contents/ui/main.qml` (near `syncIslands`/`syncCenterPanel`, `Component.onCompleted` ~1538-1544),
  `package/contents/ui/config/TaskbarLayoutBox.qml`

**Interfaces (produces):** `root.kdePatched` (bool); new i18n strings for Task 8.

- [ ] **Step 1: main.qml gate** — add near `islandWant`:

```qml
    // W17: the layouts and the Start position need Win12's patched KDE (kde/build.sh): one script moves everything.
    // Without it no panel is touched (the settings page shows the command). Checked once per start through pacman.
    property bool kdePatched: false
    P5Support.DataSource {
        id: patchCheck
        engine: "executable"
        connectedSources: ["pacman -T libplasma-win12 plasma-workspace-win12 plasma-desktop-win12 >/dev/null 2>&1 && echo ok"]
        onNewData: (sourceName, data) => {
            disconnectSource(sourceName);
            root.kdePatched = String(data.stdout).trim() === "ok";
        }
    }
    onKdePatchedChanged: if (kdePatched) { syncCenterPanel(); syncIslands(); }
```
and `if (!configReady || !kdePatched) return;` as the first line of `syncIslands()` and `syncCenterPanel()` (keep the
existing comment on the latter). The direct calls in `Component.onCompleted` stay (they return early until the check
answers; the check then runs them).

- [ ] **Step 2: `TaskbarLayoutBox.qml`** — `values: ["single", "split-left", "split-right", "islands"]`, a 4th model
  entry `i18n("3 Islands: pinned apps, open apps, tray")`, the help label text becomes
  `i18n("Split layouts put the tray, clock and weather into separate floating panels (islands). 3 Islands also splits pinned and open apps; drag an app icon to another island to move it there. Touching the bottom edge shows all of them.")`,
  and a warning (imports `org.kde.plasma.plasma5support as P5Support` and `org.kde.kirigami as Kirigami`):

```qml
    // W17: the layouts need the patched KDE packages (same pacman check as main.qml kdePatched).
    property bool kdePatched: true   // until the check answers
    P5Support.DataSource {
        engine: "executable"
        connectedSources: ["pacman -T libplasma-win12 plasma-workspace-win12 plasma-desktop-win12 >/dev/null 2>&1 && echo ok"]
        onNewData: (sourceName, data) => { disconnectSource(sourceName); box.kdePatched = String(data.stdout).trim() === "ok"; }
    }
    QQC2.Label {
        visible: !box.kdePatched
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        color: Kirigami.Theme.negativeTextColor
        text: i18n("The taskbar layouts and the Start position need Win12's patched KDE packages. Run kde/build.sh from the Win12 source folder, then log out and in.")
    }
```

- [ ] **Step 3: check**

Run: `/usr/lib/qt6/bin/qmllint package/contents/ui/config/TaskbarLayoutBox.qml 2>&1 | grep -E 'Error|error:' | head -3; node tests/test_islands.js | tail -1; grep -c 'kdePatched' package/contents/ui/main.qml`
Expected: no errors (unresolved-import warnings are fine), `test_islands OK`, count >= 4.

- [ ] **Step 4: commit**

```bash
git add package/contents/ui/main.qml package/contents/ui/config/TaskbarLayoutBox.qml
git commit -m "settings: 3 Islands entry, patched-KDE gate and warning"
```

---

### Task 8: Translations

Model: sonnet
Depends on: 7

**Files:** Modify `po/template.pot`, `po/es.po`, `package/contents/locale/es/LC_MESSAGES/plasma_applet_org.win12.menu.mo`
(compiled by install.sh; commit it as the repo does).

- [ ] **Step 1:** `sh package/Messages.sh && grep -c '^msgstr ""$' po/es.po` — then `grep -n -B2 'msgstr ""$' po/es.po | grep msgid | tail -5` to see the new untranslated ids.
- [ ] **Step 2:** fill the three new entries in `po/es.po` (edit the `msgstr` after each `msgid`):
  - `3 Islands: pinned apps, open apps, tray` -> `3 islas: apps ancladas, apps abiertas, bandeja`
  - `Split layouts put the tray, clock and weather into separate floating panels (islands). 3 Islands also splits pinned and open apps; drag an app icon to another island to move it there. Touching the bottom edge shows all of them.` ->
    `Los diseños divididos ponen la bandeja, el reloj y el tiempo en paneles flotantes separados (islas). 3 islas separa también las apps ancladas de las abiertas; arrastra el icono de una app a otra isla para moverla. Tocar el borde inferior las muestra todas.`
  - `The taskbar layouts and the Start position need Win12's patched KDE packages. Run kde/build.sh from the Win12 source folder, then log out and in.` ->
    `Los diseños de la barra de tareas y la posición de Inicio necesitan los paquetes de KDE parcheados de Win12. Ejecuta kde/build.sh desde la carpeta del código de Win12 y vuelve a iniciar sesión.`
  Remove any `#, fuzzy` marker msgmerge added to a changed entry after checking its translation.
- [ ] **Step 3:** `msgfmt --check -o package/contents/locale/es/LC_MESSAGES/plasma_applet_org.win12.menu.mo po/es.po && msgfmt --statistics po/es.po 2>&1 | tail -1`
  Expected: `N translated messages.` with 0 untranslated / fuzzy.
- [ ] **Step 4:** `git add po package/contents/locale && git commit -m "i18n: Spanish for the 3 Islands layout and the patched-KDE warning"`

---

### Task 9: Live-test tools in `tests/tools/` + docs

Model: sonnet
Depends on: 1

**Files:**
- Create: `tests/tools/w16mouse.py` (copy + `drag`), `tests/tools/state.sh`, `tests/tools/setlayout.sh`
- Modify: `README.md` (new section), `docs/HANDOFF.md` (new entry at the end)

- [ ] **Step 1: copy the helpers**

```bash
S=/tmp/claude-1000/-home-kalaminator-Documents-Start12/b88e81db-1485-4a3f-ae50-6c8c3972125b/scratchpad
mkdir -p tests/tools && cp "$S/w16mouse.py" "$S/state.sh" tests/tools/ && chmod +x tests/tools/*.sh
```
Read `tests/tools/w16mouse.py` whole (small). Add a `drag` mode: `w16mouse.py drag x1,y1 x2,y2 [steps]` = move to
x1,y1, press BTN_LEFT (0x110; register it with `UI_SET_KEYBIT` if the file does not already), move in `steps`
(default 20) linear steps 25 ms apart, hold 150 ms, release, hold 200 ms. Update the usage line (first line).
Reuse the file's own helpers for the device and the absolute move; one `if sys.argv[1] == "drag":` branch.

- [ ] **Step 2: `tests/tools/setlayout.sh`** (replaces the scratch version)

```sh
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
```

- [ ] **Step 3: README section** (after the install section; keep the README's tone):

```markdown
## Patched KDE for the taskbar layouts (Arch)

The split and 3 Islands layouts and the Start position move panel widgets through Plasma's scripting. Stock Plasma
6.8 does that one step at a time (5-8 s per switch) and loses tray icons when a tray is moved, so Win12 ships small
patches for three packages and rebuilds them from Arch's own PKGBUILDs:

    kde/build.sh            # libplasma, plasma-workspace, plasma-desktop; 20-40 min; asks for sudo to install

Without the patched packages the Taskbar page shows a warning and Win12 leaves your panels alone. After a Plasma
update pacman prints "Win12: run kde/build.sh again" (a hook shipped by the patched plasma-workspace). Undo at any
time: `sudo pacman -S libplasma plasma-workspace plasma-desktop`. Patches: `kde/patches/`; what they change is in
`docs/superpowers/specs/2026-10-16-fast-islands-design.md`.
```

- [ ] **Step 4: HANDOFF entry** (append):

```markdown
## Fast islands on a patched KDE + 3 Islands (2026-10-17)
`kde/build.sh` rebuilds libplasma / plasma-workspace / plasma-desktop from Arch PKGBUILDs with `kde/patches/*`
(`provides <pkg>-win12`; hook `win12-kde.hook` warns after an update). Patches: libplasma `Containment::addApplet`
moves the whole `[Applets][id]` group (tray keeps its icons); plasma-workspace `Widget.index` get/set skip empty
parts + clamp to length, `TasksModel.appIdFilter`/`appIdFilterExclude`; plasma-desktop panel saves `AppletOrder` at
once, task manager key `win12Island` (pinned|open|tray), registry singleton `TaskManagerApplet.Win12Islands`
(`~/.config/win12islandsrc` [Apps] appid=island), drag mime `text/x-win12-appid` + `text/x-win12-launcher`, drop on
another island assigns the app (+ launcher). Win12: `main.qml kdePatched` (pacman -T) gates syncIslands/syncCenterPanel;
`code/islandlogic.js` (roles/trayOrder/listsFor/mergeLists) + `code/islands.js` do all moves in one pass (`@POS@` =
menuPosition; `islands` mode: host + A (other app island) + T (tray island), keys `win12Layout` {A,T,atm,ttm,weather},
`win12TrayClean`); `tests/test_islands.js` = pure tests + fake-shell harness (<= 3 passes per switch);
`tests/tools/` = setlayout.sh (timing), state.sh, w16mouse.py (drag). Spec + plan + progress: docs/superpowers/.
```

- [ ] **Step 5: check + commit**

Run: `python3 -c "import ast,sys; ast.parse(open('tests/tools/w16mouse.py').read())" && sh -n tests/tools/setlayout.sh && head -1 tests/tools/w16mouse.py`
Expected: no error; usage line mentions `drag`.

```bash
git add tests/tools README.md docs/HANDOFF.md
git commit -m "docs+tools: patched-KDE section, HANDOFF entry, live-test helpers (layout timing, pointer drag)"
```

---

### Task 10: Build + install patched libplasma

Model: opus
Depends on: 2 (and 1)

Needs sudo pacman for the install step. First: `sudo -n /usr/bin/pacman -V >/dev/null && echo SUDO_OK || echo SUDO_NO`.
If `SUDO_NO`: still build (build.sh builds without installing), then `status: blocked` with the printed
`pacman -U` command in `notes:`; Tasks 11-14 are then `skipped` (blocked by Task 10) and `next:` points at Task 15.

- [ ] **Step 1: start the build detached**

```bash
mkdir -p ~/.cache/win12-kde && cd /home/kalaminator/Documents/Start12 && setsid nohup bash kde/build.sh libplasma > ~/.cache/win12-kde/libplasma.log 2>&1 < /dev/null & echo started
```

- [ ] **Step 2: wait (ONE blocking call, repeat at most twice)**

Run (timeout 600000): `timeout 590 sh -c 'until grep -qE "WIN12_BUILD_(OK|FAIL)" ~/.cache/win12-kde/libplasma.log; do sleep 20; done'; grep -E 'WIN12_BUILD|== |tests passed|FAILED|error:|FAILED TO' ~/.cache/win12-kde/libplasma.log | tail -12`
Expected: `WIN12_BUILD_OK libplasma` with `100% tests passed` above it. If the run nears its budget while the build
runs: `status: partial`, `notes: build running, log ~/.cache/win12-kde/libplasma.log, resume at Step 2`, `next:` this task.

- [ ] **Step 3: on `WIN12_BUILD_FAIL`** — read the step: `pkgbuild` = fix `kde/pkgbuild-win12.sh`; `makepkg` with a
  patch error = the PKGBUILD tarball differs from the git tag: regenerate the patch against
  `~/.cache/win12-kde/libplasma/src/libplasma-6.8.0` (read the `.rej`), update the dev copy too; a test failure =
  read `~/.cache/win12-kde/libplasma/src/libplasma-6.8.0/build/Testing/Temporary/LastTest.log | tail -40`. Fix, commit
  the fix, re-run Step 1-2 once.

- [ ] **Step 4: verify the install**

Run: `pacman -Q libplasma && pacman -T libplasma-win12; echo "missing=$?"` — Expected: `6.8.0-1` and `missing=0`
(nothing missing). The running plasmashell still uses the old library; the restart happens in Task 13.

- [ ] **Step 5: commit** anything changed (`git status --short`); if nothing, no commit. Progress log entry with
  the build time (`stat -c %Y` of the log minus the start, or the `== ` timestamps if you add `date` lines).

---

### Task 11: Build + install patched plasma-workspace

Model: opus
Depends on: 3, 10

Same shape as Task 10 with `plasma-workspace` (log `~/.cache/win12-kde/plasma-workspace.log`). This is the long one
(20-40 min): the wait is up to 4 blocking calls; hand over with `status: partial` when the budget nears.
Sudo check first. Extra verify: `pacman -Ql plasma-workspace | grep -c win12-kde.hook` prints `1`;
`pacman -T plasma-workspace-win12; echo $?` prints `0`. If `check()` fails because `BUILD_TESTING=ON` pulls a
test-only dependency at configure time, install it with `sudo -n pacman -S --noconfirm <pkg>` (pacman only), note
it, and re-run; if it is a wayland-only test that cannot run headless, the `-R 'win12|coronatest'` filter already
skips it, so look for the real error in the log first.

---

### Task 12: Build + install patched plasma-desktop

Model: opus
Depends on: 4, 11

Same shape as Task 10 with `plasma-desktop` (log `~/.cache/win12-kde/plasma-desktop.log`). Sudo check first.
Extra verify: `pacman -T libplasma-win12 plasma-workspace-win12 plasma-desktop-win12; echo "missing=$?"` prints
`missing=0` (all three patched), and `pacman -Ql plasma-desktop | grep -c win12islandregistry` is `0` (the class is
inside the plugin library, no stray files) while `grep -c win12Island /usr/share/plasma/plasmoids/org.kde.plasma.taskmanager/contents/config/main.xml`
(or wherever `pacman -Ql plasma-desktop | grep taskmanager.*main.xml` says) is `1`.

---

### Task 13: Live test A — every layout x Start position, timing, screenshots, tray

Model: opus
Depends on: 7, 9, 12

Precondition: `pacman -T libplasma-win12 plasma-workspace-win12 plasma-desktop-win12; echo $?` = `0`, else `blocked`.

- [ ] **Step 1: backup + install Win12 + restart**

```bash
cp ~/.config/plasma-org.kde.plasma.desktop-appletsrc ~/.config/plasma-org.kde.plasma.desktop-appletsrc.bak-fastislands-$(date +%Y%m%d-%H%M) && cd /home/kalaminator/Documents/Start12 && ./install.sh --restart 2>&1 | tail -3 && timeout 60 sh -c 'until qdbus6 org.kde.plasmashell /PlasmaShell >/dev/null 2>&1; do sleep 1; done' && sleep 8 && journalctl --user --since "-30 s" 2>/dev/null | grep -i -E 'win12|qml.*error|TypeError' | tail -8
```
Expected: plasmashell back, no `TypeError`/`error` lines naming our files. `tests/tools/state.sh | head -8`
shows the panels.

- [ ] **Step 2: record the user's values** (to restore at the end): `qdbus6 org.kde.plasmashell /PlasmaShell evaluateScript "for (const p of panels()) for (const w of p.widgets('org.win12.menu')) { w.currentConfigGroup=['XP']; print(w.readConfig('taskbarLayout','single')+' '+w.readConfig('taskbarLook','')+' '); w.currentConfigGroup=['Style']; print(w.readConfig('menuPosition',0)+' '+w.readConfig('menuStyle','')); }"`.
  Write them into the progress log `notes:` NOW (if the run dies, Task 14's restore needs them). The islands only
  apply when the taskbar look is windows12: if neither `taskbarLook` nor `menuStyle` is `windows12`, pass
  `windows12` as the third argument of every `setlayout.sh` call below and restore the old `taskbarLook` at the end.

- [ ] **Step 3: the matrix** (12 steps; one Bash call per row, output through tail):

```bash
cd /home/kalaminator/Documents/Start12 && O=/tmp/claude-1000/-home-kalaminator-Documents-Start12/b88e81db-1485-4a3f-ae50-6c8c3972125b/scratchpad/live && mkdir -p $O && for L in single split-left split-right islands; do for P in 0 1 2; do echo "== $L pos $P: $(tests/tools/setlayout.sh $L $P)"; sleep 1.5; spectacle -b -n -f -o $O/shot-$L-$P.png 2>/dev/null; tests/tools/state.sh | grep -E 'panel|tray' | cut -c1-200; done; done 2>&1 | tail -60
```
Expected per row: `took <ms>` (goal < 1500), `islands` rows show 3 panels (`isl=A`, `isl=T` plus the host) with the
tray island's widgets in the spec order (Left/Center: tasks, weather, tray, clock, show desktop; Right reversed),
and the tray line's `extra=` count the same as in the `single` row (no icons lost). View with Read ONLY these
screenshots: `shot-single-0.png`, `shot-islands-0.png`, `shot-islands-1.png`, `shot-islands-2.png` — check 3 islands
are on screen, Start where expected, tray icons visible. Leave the others on disk for the user (path in notes).

- [ ] **Step 4: results to the progress log**: a 12-row table `layout | pos | ms | panels | tray extra | ok?`;
  any row > 1500 ms gets the pass count from `journalctl --user --since "-10 min" | grep -c evaluateScript` if
  visible, else a note. Do not fix engine issues in this task (budget): write the exact symptom for Task 15.

- [ ] **Step 5: leave the desktop in `islands` pos 0 for Task 14** (Task 14 restores the user's values). Commit
  nothing unless a tool script needed a fix (then commit it: `tools: ...`).

---

### Task 14: Live test B — drag an app across the 3 islands, restart persistence, restore

Model: opus
Depends on: 13

- [ ] **Step 1: open a test window and find the islands**

```bash
konsole --title win12-drag-test & sleep 2; qdbus6 org.kde.plasmashell /PlasmaShell evaluateScript "for (const p of panels()) { p.currentConfigGroup=['General']; print(p.readConfig('win12Island','host')+' '+JSON.stringify(p.geometry)+'\n'); }"; cat ~/.config/win12islandsrc 2>/dev/null | tail -5
```
Expected: three geometries (host, A, T) on the bottom edge; konsole shows in the `open` island (A). Screenshot
`shot-drag-0.png`.

- [ ] **Step 2: drag A -> host (pinned)**: source = centre of the first task button in A (A.x + 8 + A.height/2,
  A.y + A.height/2); target = the host's task manager area (host.x + host.width - 30, same y: the right end of the
  pinned icons). `python3 tests/tools/w16mouse.py drag SX,SY TX,TY 30` (needs /dev/uinput access; if `Permission denied`,
  note it and stop here with `status: partial` + exact command for the user). Then
  `sleep 1; grep konsole ~/.config/win12islandsrc` — Expected: `org.kde.konsole.desktop=pinned`; `state.sh` unchanged
  (no panel moves); screenshot `shot-drag-1.png` and view it: konsole's icon now in the host island.
- [ ] **Step 3: host -> T (tray island)**: target = T.x + 8 + T.height/2 (the tray island's task manager keeps one
  slot). Expected `=tray`. Screenshot. Then T -> A: Expected `=open`.
- [ ] **Step 4: persistence**: with konsole in `tray`, `systemctl --user restart plasma-plasmashell && sleep 10`;
  `grep konsole ~/.config/win12islandsrc` still `tray`; screenshot shows konsole in the tray island.
- [ ] **Step 5: restore the user's desktop**: `tests/tools/setlayout.sh <layout> <pos> [<look>]` with the values from
  Task 13's notes; `kill %1` (the konsole); remove the test entry: `kwriteconfig6 --file win12islandsrc --group Apps --key org.kde.konsole.desktop --delete`.
  Final screenshot `shot-restored.png`, view it. Progress log: what worked, what did not, screenshot folder.

---

### Task 15: Whole-change review + ELI5

Model: opus
Depends on: all (runs even when some tasks are blocked; says so)

- [ ] **Step 1:** `git log --oneline 4ad5be4..HEAD` and `git diff --stat 4ad5be4..HEAD | tail -5`. Read the progress
  log whole. Read each patch (`kde/patches/*/*.patch`, they are small), `islands.js`, `islandlogic.js`, the main.qml
  hunks (`git diff 4ad5be4..HEAD -- package/contents/ui/main.qml`), `TaskbarLayoutBox.qml`.
- [ ] **Step 2: check against the spec, section by section** (1, 3, 4.1-4.3, 5.1-5.2, 6, 7): for each requirement
  name the file that implements it or write `GAP:`. Check the Review focus list (5 items). Check: no password or
  sudo beyond pacman anywhere (`grep -rn sudo kde tests install.sh`), no file written outside the repo/cache,
  marker names `win12*`, comments explain why, tests run (`node tests/test_islands.js | tail -1`,
  `sh tests/test_pkgbuild.sh | tail -1`).
- [ ] **Step 3: fix small things inline** (typos, a missing comment, a wrong label); commit `review: ...`. Bigger
  gaps go to the progress log as `left:` with the exact next step, not fixed here.
- [ ] **Step 4: ELI5 for the user in the progress log** (<= 12 lines, plain words): what changed on the machine
  (three KDE packages rebuilt with small fixes; how to undo), what changed in Win12 (switches in one go, the new
  3 Islands layout, drag icons between islands), what was verified (numbers), what was not or is blocked, and the
  one command to run after each Plasma update. End with `next: none` / `next-model: none`.

# Win12 Explorer Phase 1 (Explorer shell) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** a standalone KF6/Qt6 file manager, `win12-explorer`, with the Windows ribbon (File · Home · Share · View · Advanced), address row, folder tabs, nav pane, split view and its own smooth file view, built and installed by `install.sh` as an Arch package, switchable in as the user's file manager from Win12 Settings.

**Architecture:** One C++ app in a new top-level `explorer/` folder. The ribbon is one data table (`ribbonspec.cpp`) rendered by `RibbonBar`; every button triggers a `QAction` owned by `ExplorerActions`, which does its work with KIO jobs recorded in `KIO::FileUndoManager`. The file view is `FolderView` (a `QListView` for icon/list/tiles/content modes and a `QTreeView` for details, one shared selection) over the model chain `KDirModel → FolderModel (KDirSortFilterProxyModel subclass: group-aware sort, previews) → TagsColumns (KExtraColumnsProxyModel: the Tags column, empty until phase 2)`; thumbnails come from our own `PreviewLoader` (`KIO::PreviewJob`, visible rows first). `ExplorerWindow` (KMainWindow) stacks ribbon, address row (`KUrlNavigator`), tab bar, `KFilePlacesView` + `TabPage`s (one or two `FolderView`s) and a status bar. Config lives in `win12-explorerrc` only. The plasmoid gets one Bool key `explorerDefault` and a shell script `win12-explorer-default on|off|status` that saves/restores the `inode/directory` default and a per-user D-Bus service override for `org.freedesktop.FileManager1`.

**Tech Stack:** C++17, Qt 6.12 (Widgets, DBus, Test), KDE Frameworks 6.31 (KIO Core/Gui/Widgets/FileWidgets, KItemModels, KXmlGui, KI18n, KConfig, KConfigWidgets, KColorScheme, KDBusAddons, Purpose), CMake + extra-cmake-modules 6.31, QtTest (`QT_QPA_PLATFORM=offscreen`), makepkg/pacman, POSIX sh, QML (plasmoid pages), node (existing settings-key test).

**Spec:** `docs/superpowers/specs/2026-10-23-win12-explorer-design.md` (sections Intent, Phases, Goals/Phase 1, Non-goals, Anti-goals, Constraints, Decided how, Left to the builder). Approved look: `docs/superpowers/specs/2026-10-23-win12-explorer-mockups/ribbon.html` and `reference-dark-explorer.png` in that folder. Phases 2-4 get their own plans; this plan only leaves them seams (listed under "Seams for later phases").

## Global Constraints

- C++17+, Qt 6, KDE Frameworks 6 (KIO, KIOFileWidgets, KFileMetaData, Baloo optional), CMake + extra-cmake-modules. All present on the user's Arch system (KF6 6.31.0, Qt 6.12.0, Dolphin 26.08.2 for reference only).
- Source in a new top-level `explorer/` folder: one app (`win12-explorer`). Binary name `win12-explorer`, desktop entry `org.win12.explorer.desktop`, config file `win12-explorerrc` (never `dolphinrc`), i18n domain `win12-explorer`.
- Ribbon layout is data: one table of tabs → groups → buttons → action names, so adding a button is one line.
- Win12 does not change Dolphin's settings, toolbars or code; no patched Dolphin. The user's Places file is never rewritten (`KFilePlacesModel` API only, no file edits).
- No Windows 11 command-bar mode; no quick-access toolbar in the title bar (KWin draws it); no new icon theme; no "fast open" daemon beyond the `--daemon` process that only holds the FileManager1 name.
- Big folders must stay smooth: async listing, no file-system work on the UI thread, a virtualised view (`QListView`/`QTreeView` over a model, never a widget per item).
- Theme follows the KDE colour scheme and `kdeglobals` AccentColor through `QPalette`/`KColorScheme`; no state file from the plasmoid.
- Settings keys go in `package/contents/config/main.xml` and (keys with a page control) `tests/settings-keys.txt`; `node tests/test_settings_keys.js --strict` must pass. Never act before `configReady`; respect the `isMirror` guard; any system setting Win12 changes is saved first and restored when the switch goes off, like `syncWindowDeco`.
- Build fixes in: `install.sh` builds Win12 Explorer as an Arch package and installs it with `sudo pacman -U` the way `kde/build.sh` does; it installs missing build tools itself (`makepkg -s`, `base-devel` if `makepkg` is missing). A non-Arch or compiler-less install skips the package with one printed line and the rest of Win12 still installs.
- Turning "Use Win12 Explorer" off must leave the user's old default file manager in place (the exact previous `inode/directory` default, or no key if there was none).
- Translations through the existing `po/` flow (`package/Messages.sh` runs the extraction).
- Commit message prefix `explorer:` (repo style: `islands:`, `colours:`, `docs:`). Each task is one fresh agent of at most ~100k tokens / ~40 tool calls; build and test output always through `| tail`.

## Review Focus

1. A folder with tens of thousands of files on the slow NTFS disk: the window must keep repainting while the listing streams in (no synchronous `KIO::stat`/`QDir` work on the UI thread) — pinned by `FolderModelTest::largeFolderKeepsEventLoopFree` (Task 5).
2. Inline rename to a name that already exists must refuse and keep both files, never overwrite — pinned by `ExplorerActionsTest::renameRefusesExistingName` (Task 7).
3. Turning the switch off when there was no previous `inode/directory` default must delete the key rather than write an empty one, and a second `on` must not overwrite the saved value — pinned by `tests/test_explorer_default.sh` (Task 10).
4. `ShowItems` from other apps with percent-encoded paths (spaces, `#`), several folders at once, and non-`file:` schemes (`smb://`) must open the right folders and select the right items — pinned by `FileManager1Test::foldersForGroupsByParent` (Task 10).
5. Restoring the saved tabs when one folder no longer exists must open the remaining tabs (Home when none remain), not an error box per tab — pinned by `SessionTest::missingFoldersAreDropped` (Task 9).

## Seams for later phases (do not implement here, do leave these)

- Tags column: `TagsColumns::TagsColumn` (model column 7, header "Tags", column id `"tags"` in `ViewSettings::columns`) and `TagsColumns::TagsRole` return an empty `QStringList` in phase 1. Phase 2 fills `extraColumnData()` and the delegate.
- View tab "Tags" group: present in `ribbonspec.cpp` as `Group{"Tags", {}, /*phase*/ 2}`; `RibbonBar` hides groups whose `phase > Ribbon::currentPhase` (constant `1`).
- Advanced tab: `Tab{"advanced", "Advanced", {}, false, /*phase*/ 3}`; the tab is shown and its page says i18n("Advanced tools arrive in a later Win12 version.") until a later phase adds its groups.
- Right-click "Win12 ▸": `ExplorerActions::contextMenu()` creates a `QMenu` titled i18n("Win12") and emits `fillWin12Menu(QMenu *, const KFileItemList &)`; the submenu is added only when a connected slot left at least one action in it.
- Settings: "Show tag flags on icons" (phase 2 key) is not added now.

## File Structure

```
explorer/
  CMakeLists.txt            project win12-explorer; lib win12explorer (all src but main.cpp) + app + autotests
  PKGBUILD                  Arch package from a tarball build.sh makes (pkgver replaced by build.sh)
  build.sh                  content-hash pkgver, skip when installed version matches, makepkg -sf, sudo pacman -U
  org.win12.explorer.desktop  Exec=win12-explorer %U, MimeType=inode/directory;, Icon=system-file-manager
  win12-explorer-default    sh: on | off | status  (mime default + FileManager1 override, saved/restored)
  src/main.cpp              QApplication, KDBusService::Unique, i18n domain, CLI → windows / daemon
  src/cli.{h,cpp}           parseCli()
  src/filemanager1.{h,cpp}  org.freedesktop.FileManager1 adaptor + org.win12.explorer ReleaseFileManager1
  src/ribbonspec.{h,cpp}    THE ribbon table (Ribbon::spec(), Ribbon::actionIds())
  src/ribbonbar.{h,cpp}     RibbonBar + RibbonTabStrip painting, collapse, File menu
  src/viewsettings.{h,cpp}  ViewMode/SortRole/GroupRole, ViewSettings, ViewSettingsStore, zoomStep()
  src/sortgroup.{h,cpp}     SortGroup::groupInfo(), groupLess(), sortColumnFor()
  src/foldermodel.{h,cpp}   FolderModel, TagsColumns, PreviewLoader
  src/folderview.{h,cpp}    FolderView (two Qt views, one selection), FolderDelegate, NavHistory
  src/exploreractions.{h,cpp}  ExplorerActions: QActions for every ribbon id, KIO work, context menu
  src/explorerwindow.{h,cpp}   ExplorerWindow, TabPage, SidePane, RecentFolders, session save/restore
  autotests/*_test.cpp      QtTest, one file per src unit named below
tests/test_explorer_pkgbuild.sh   PKGBUILD + build.sh --version check (sh, no root)
tests/test_explorer_default.sh    win12-explorer-default against fake xdg-mime/busctl/kwriteconfig6
package/contents/config/main.xml, tests/settings-keys.txt, package/contents/ui/code/tiles.js,
package/contents/ui/config/ConfigWindows.qml, package/contents/ui/main.qml, install.sh, package/Messages.sh
```

Build and test commands used by every task (the build dir stays out of the tree):

```bash
B="${XDG_CACHE_HOME:-$HOME/.cache}/win12-explorer-build"
cmake -S explorer -B "$B" -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug >/dev/null && cmake --build "$B" -j"$(nproc)" 2>&1 | grep -E 'error|warning: unused|Linking' | tail -20
QT_QPA_PLATFORM=offscreen ctest --test-dir "$B" --output-on-failure 2>&1 | tail -15
```

---

### Task 1: Package skeleton (CMake, PKGBUILD, build.sh, install.sh hook, empty window)

**Files:**
- Create: `explorer/CMakeLists.txt`, `explorer/PKGBUILD`, `explorer/build.sh`, `explorer/org.win12.explorer.desktop`, `explorer/src/main.cpp`, `explorer/src/explorerwindow.h`, `explorer/src/explorerwindow.cpp`, `explorer/.gitignore` (`build/`)
- Create: `tests/test_explorer_pkgbuild.sh`
- Modify: `install.sh:19-21` (after `kpackagetool6`, before `setup.sh`)

**Interfaces:**
- Consumes: nothing.
- Produces: CMake target `win12explorer` (static lib of every `src/*.cpp` except `main.cpp`, PUBLIC links `Qt6::Widgets Qt6::DBus KF6::KIOCore KF6::KIOGui KF6::KIOWidgets KF6::KIOFileWidgets KF6::ItemModels KF6::XmlGui KF6::I18n KF6::ConfigCore KF6::ConfigWidgets KF6::ColorScheme KF6::DBusAddons KF6::PurposeWidgets`), executable `win12-explorer`, CMake function `explorer_add_test(<name>)` = `ecm_add_test(autotests/<name>.cpp TEST_NAME <name> LINK_LIBRARIES win12explorer Qt6::Test)` with `ENVIRONMENT QT_QPA_PLATFORM=offscreen`. `class ExplorerWindow : public KMainWindow { explicit ExplorerWindow(const QList<QUrl> &urls = {}, QWidget *parent = nullptr); }` (empty for now; window title i18n("Win12 Explorer"), `setAutoSaveSettings()`). `build.sh [--version|--force]` prints `WIN12_BUILD_OK win12-explorer` / `WIN12_BUILD_FAIL win12-explorer <step>`.

- [ ] **Step 1: Write the failing test `tests/test_explorer_pkgbuild.sh`** (same shape as `tests/test_pkgbuild.sh`: `set -e`, temp dir, trap):
  - copies `explorer/PKGBUILD` to `$T` with `sed 's/@VER@/0.1.abcdef012345/'`, `bash -n PKGBUILD`;
  - `S=$(makepkg --printsrcinfo)` must contain `pkgname = win12-explorer`, `arch = x86_64`, `depends = kio`, `depends = kitemmodels`, `depends = purpose`, `makedepends = extra-cmake-modules`, `makedepends = cmake`, and `grep -q 'BUILD_TESTING=ON' PKGBUILD`, `grep -c '^package() {' PKGBUILD | grep -qx 1`;
  - `V=$(sh explorer/build.sh --version)`; `echo "$V" | grep -Eqx '0\.1\.[0-9a-f]{12}'`; running it twice prints the same value; `grep -q "^pkgver=@VER@" explorer/PKGBUILD` (build.sh substitutes);
  - prints `test_explorer_pkgbuild OK`.
- [ ] **Step 2: Run it, expect FAIL** — `sh tests/test_explorer_pkgbuild.sh; echo rc=$?` → "No such file" for explorer/PKGBUILD, rc 1.
- [ ] **Step 3: Write `explorer/PKGBUILD`**: `pkgname=win12-explorer`, `pkgver=@VER@`, `pkgrel=1`, `pkgdesc='Win12 Explorer: ribbon file manager for KDE (Win12)'`, `arch=(x86_64)`, `url='https://github.com/Kalaminator/Win12'`, `license=(MIT)`, `depends=(qt6-base kio kitemmodels kxmlgui ki18n kconfig kconfigwidgets kcolorscheme kdbusaddons purpose kfilemetadata xdg-utils)`, `makedepends=(cmake extra-cmake-modules)`, `source=("$pkgname-$pkgver.tar.gz")`, `sha256sums=('SKIP')`, `build()` = `cmake -B build -S "$pkgname-$pkgver" -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build`, `check()` = `QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure`, `package()` = `DESTDIR="$pkgdir" cmake --install build`.
- [ ] **Step 4: Write `explorer/build.sh`** (`set -eu`, `HERE`, `CACHE="${XDG_CACHE_HOME:-$HOME/.cache}/win12-explorer"`, `step()` echo like `kde/build.sh`): version = `0.1.` + first 12 hex of `sha256sum` over `find "$HERE" -type f ! -path '*/build/*' | LC_ALL=C sort | xargs sha256sum | sha256sum`; `--version` prints it and exits; without `--force`, exit 0 with `WIN12_BUILD_OK win12-explorer (installed)` when `pacman -Q win12-explorer` reports `$VER-1`; `command -v makepkg >/dev/null || sudo pacman -S --needed --noconfirm base-devel`; `tar` of the tree into `$CACHE/win12-explorer-$VER.tar.gz` with `--transform "s,^,win12-explorer-$VER/,"` excluding `build/`; `sed "s/@VER@/$VER/" PKGBUILD > $CACHE/PKGBUILD`; `makepkg -sf --noconfirm` in `$CACHE`; install gate copied from `kde/build.sh:14` (`sudo -n /usr/bin/pacman -V || [ -t 0 ]`), `sudo pacman -U --noconfirm`, else print the `pacman -U` line. Any failing step prints `WIN12_BUILD_FAIL win12-explorer <step>` and exits 1 (`trap` on ERR).
- [ ] **Step 5: Write `explorer/CMakeLists.txt`** (`cmake_minimum_required(VERSION 3.20)`, `project(win12-explorer VERSION 0.1)`, `find_package(ECM 6.0 REQUIRED NO_MODULE)`, KDE modules `KDEInstallDirs KDECMakeSettings KDECompilerSettings ECMAddTests`, `find_package(Qt6 6.6 REQUIRED Widgets DBus Test)`, `find_package(KF6 6.0 REQUIRED KIO ItemModels XmlGui I18n Config ConfigWidgets ColorScheme DBusAddons Purpose)`, `set(CMAKE_CXX_STANDARD 20)` — KDECompilerSettings' default; the plan's `= default` comparisons need it), the targets and `explorer_add_test()` from Interfaces, `install(TARGETS win12-explorer ...)`, `install(PROGRAMS win12-explorer-default DESTINATION ${KDE_INSTALL_BINDIR})`, `install(FILES org.win12.explorer.desktop DESTINATION ${KDE_INSTALL_APPDIR})`, `ki18n_install(po)` guarded by `if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/po)`. Add an empty `explorer/win12-explorer-default` placeholder (`#!/bin/sh` + `exit 0`, filled in Task 10) so the install rule works.
- [ ] **Step 6: Write `main.cpp` + the empty `ExplorerWindow`**: `QApplication`, `KLocalizedString::setApplicationDomain("win12-explorer")`, `QCoreApplication::setOrganizationDomain("win12.org")`, `setApplicationName("explorer")`, `setDesktopFileName("org.win12.explorer")`, `KDBusService service(KDBusService::Unique)`, one `ExplorerWindow` shown. Desktop file: `Type=Application`, `Name=Win12 Explorer`, `Exec=win12-explorer %U`, `Icon=system-file-manager`, `Categories=Qt;KDE;System;FileManager;`, `MimeType=inode/directory;`, `DBusActivatable=false`, `StartupNotify=true`.
- [ ] **Step 7: Build** with the commands above; expected: `Linking CXX executable win12-explorer`, no `error`. `ctest` reports `No tests were found` (fine until Task 2).
- [ ] **Step 8: Run the pkgbuild test** — `sh tests/test_explorer_pkgbuild.sh | tail -1` → `test_explorer_pkgbuild OK`.
- [ ] **Step 9: Hook `install.sh`**: after the `kpackagetool6` block insert `if command -v pacman >/dev/null 2>&1; then sh explorer/build.sh || echo "Win12 Explorer: build failed (see above); the rest of Win12 is installed"; else echo "Win12 Explorer: skipped (needs Arch makepkg; the Windows settings page says so)"; fi`. Also add `sh tests/test_explorer_pkgbuild.sh` next to the existing `node tests/...` line, gated on `command -v makepkg`.
- [ ] **Step 10: Manual check**: `sh explorer/build.sh 2>&1 | tail -3` → `WIN12_BUILD_OK win12-explorer`; `pacman -Q win12-explorer`; `win12-explorer &` shows an empty window titled "Win12 Explorer"; `sh explorer/build.sh | tail -1` → `(installed)` skip.
- [ ] **Step 11: Commit** — `git add explorer tests/test_explorer_pkgbuild.sh install.sh && git commit -m "explorer: package skeleton (CMake, PKGBUILD, build.sh, install.sh hook)"`.

### Task 2: Ribbon spec table

**Files:**
- Create: `explorer/src/ribbonspec.h`, `explorer/src/ribbonspec.cpp`, `explorer/autotests/ribbonspec_test.cpp`
- Modify: `explorer/CMakeLists.txt` (add `explorer_add_test(ribbonspec_test)`)

**Interfaces:**
- Produces:
  ```cpp
  namespace Ribbon {
  constexpr int currentPhase = 1;
  enum class Size { Big, Small };
  struct Button { QString action; QString text; QString icon; Size size = Size::Big; bool menu = false; };
  struct Group  { QString text; QList<Button> buttons; int phase = 1; };
  struct Tab    { QString id; QString text; QList<Group> groups; bool isFileMenu = false; int phase = 1; };
  const QList<Tab> &spec();
  QStringList actionIds();          // every Button::action in table order
  const Button *button(const QString &actionId);   // nullptr when unknown
  }
  ```
  Action ids (exact strings, used by Tasks 7-9): File tab (`isFileMenu`): `file_new_window`, `file_terminal`, `file_options`, `file_help`, `file_close`. Home: Clipboard `home_pin`, `home_copy`, `home_paste`, `home_cut`, `home_copy_path`, `home_paste_shortcut`, `home_undo`, `home_redo`; Organize `home_move_to`, `home_copy_to`, `home_delete` (menu), `home_rename`; New `home_new_folder`, `home_new_item` (menu); Open `home_properties`, `home_open` (menu), `home_edit`, `home_history` (menu); Select `home_select_all`, `home_select_none`, `home_select_invert`. Share: `share_share` (menu), `share_email`, `share_zip`, `share_print`, `share_terminal`, `share_security`. View: Panes `view_nav_pane`, `view_preview_pane`, `view_details_pane`; Layout `view_layout_xlarge`, `view_layout_large`, `view_layout_medium`, `view_layout_small`, `view_layout_list`, `view_layout_details`, `view_layout_tiles`, `view_layout_content`; Current view `view_sort` (menu), `view_group` (menu), `view_columns` (menu); Tags group (phase 2, no buttons); Show/hide `view_checkboxes`, `view_extensions`, `view_hidden`; Split `view_split`. Advanced: tab `advanced`, phase 3, no groups.
  Texts are the Windows names from the spec ("Pin to Quick access", "Copy path", "Paste shortcut", "Move to", "Copy to", "Delete", "Rename", "New folder", "New item", "Properties", "Open", "Edit", "History", "Select all", "Select none", "Invert selection", "Share", "Email", "Zip", "Print", "Open terminal", "Advanced security", "Navigation pane", "Preview pane", "Details pane", "Extra large icons", "Large icons", "Medium icons", "Small icons", "List", "Details", "Tiles", "Content", "Sort by", "Group by", "Add columns", "Item check boxes", "File name extensions", "Hidden items", "Split view"), wrapped in `i18n()`. Icons: Breeze names (`edit-copy`, `edit-paste`, `edit-cut`, `edit-delete`, `edit-rename`, `folder-new`, `document-new`, `document-properties`, `document-open`, `document-edit`, `edit-select-all`, `edit-select-none`, `edit-select-invert`, `document-share`, `mail-send`, `archive-insert`, `document-print`, `utilities-terminal`, `security-high`, `view-list-icons`, `view-list-details`, `view-list-text`, `view-split-left-right`, `view-hidden`, `bookmark-new`, `edit-link`, `edit-undo`, `edit-redo`, `view-sort`, `object-group`, `view-form-table`, `window-new`, `help-contents`, `configure`, `window-close`); the implementer picks within this family. Big = the first button of Clipboard (Pin), Organize (Move to), New (New folder), Open (Properties), Select (Select all), Share (Share), Panes (Navigation pane), Current view (Sort by), Show/hide (Hidden items), Split view; the rest Small (matches `ribbon.html`).

- [ ] **Step 1: Write `ribbonspec_test.cpp`** (`QObject` + `QTEST_GUILESS_MAIN`):
  - `tabOrder`: ids of `Ribbon::spec()` == `{"file","home","share","view","advanced"}`; `spec()[0].isFileMenu == true`; `spec()[4].phase == 3` and `groups.isEmpty()`.
  - `homeGroups`: group texts of tab `home` == `{"Clipboard","Organize","New","Open","Select"}`; `view` groups == `{"Panes","Layout","Current view","Tags","Show/hide","Split view"}` and the `Tags` group has `phase == 2`.
  - `actionIdsUniqueAndNamed`: `actionIds().size() == 50`, `QSet(actionIds).size() == 50`, every id matches `^(file|home|share|view)_[a-z_]+$`.
  - `menuButtons`: `button("home_delete")->menu`, `button("view_sort")->menu`, `!button("home_copy")->menu`, `button("nope") == nullptr`.
  - `everyButtonHasTextAndIcon`: no empty `text`/`icon`.
- [ ] **Step 2: Build + run, expect FAIL** — compile error (`ribbonspec.h` missing).
- [ ] **Step 3: Implement `ribbonspec.cpp`** as one static table literal; `spec()` returns a function-local `static const QList<Tab>` (so `i18n()` runs after `QCoreApplication`).
- [ ] **Step 4: Build + run** — `ribbonspec_test ... Passed`.
- [ ] **Step 5: Commit** — `git commit -m "explorer: ribbon spec table (File/Home/Share/View/Advanced, 50 action ids)"`.

### Task 3: View settings and their store

**Files:**
- Create: `explorer/src/viewsettings.h`, `explorer/src/viewsettings.cpp`, `explorer/autotests/viewsettings_test.cpp`
- Modify: `explorer/CMakeLists.txt` (test)

**Interfaces:**
- Produces:
  ```cpp
  enum class ViewMode { ExtraLargeIcons, LargeIcons, MediumIcons, SmallIcons, List, Details, Tiles, Content };
  enum class SortRole { Name, Modified, Type, Size };
  enum class GroupRole { None, Name, Modified, Type, Size };
  struct ViewSettings {
      ViewMode mode = ViewMode::Details; SortRole sortRole = SortRole::Name; Qt::SortOrder sortOrder = Qt::AscendingOrder;
      GroupRole groupRole = GroupRole::None;
      QStringList columns = {"name", "modified", "type", "size"};   // details columns, in order; known ids below
      bool hiddenFiles = false, showExtensions = true, checkBoxes = false, previews = true;
      bool operator==(const ViewSettings &) const = default;
  };
  int iconSizeFor(ViewMode);           // 256, 96, 48, 16, 16, 16, 48, 32
  ViewMode zoomStep(ViewMode, int steps);   // along Details, List, SmallIcons, MediumIcons, LargeIcons, ExtraLargeIcons; clamps at both ends; Tiles and Content count as MediumIcons
  QStringList knownColumnIds();        // {"name","size","modified","permissions","owner","group","type","tags"} = KDirModel column order + Tags
  int modelColumnFor(const QString &id);   // KDirModel::Name..Type, "tags" → 7
  QString columnTitle(const QString &id);  // i18n: Name, Size, Date modified, Permissions, Owner, Group, Type, Tags
  class ViewSettingsStore {
  public:
      explicit ViewSettingsStore(const KSharedConfig::Ptr &config);   // app uses KSharedConfig::openConfig("win12-explorerrc")
      bool perFolder() const; void setPerFolder(bool);                // [View] PerFolder, default false
      ViewSettings load(const QUrl &folder) const;                    // folder group when perFolder && has one, else [View]
      void save(const QUrl &folder, const ViewSettings &);            // writes the folder group when perFolder, else [View]
      bool hasFolderSettings(const QUrl &) const; void clearFolder(const QUrl &);
  };
  ```
  Config layout: `[View]` keys `Mode`, `SortRole`, `SortOrder`, `GroupRole`, `Columns`, `HiddenFiles`, `ShowExtensions`, `CheckBoxes`, `Previews`, `PerFolder`; per folder `[Folders][<url.toString(QUrl::FullyEncoded)>]` with the same keys. Enums stored as ints.

- [ ] **Step 1: Write `viewsettings_test.cpp`** (`QTEST_GUILESS_MAIN`, a `QTemporaryFile` config via `KSharedConfig::openConfig(path, KConfig::SimpleConfig)`):
  - `defaultsMatchSpec`: `ViewSettings{}.mode == ViewMode::Details`, `columns == {"name","modified","type","size"}`, `previews`, `!hiddenFiles`.
  - `iconSizes`: `iconSizeFor(ExtraLargeIcons) == 256`, `LargeIcons == 96`, `MediumIcons == 48`, `SmallIcons == 16`, `Content == 32`.
  - `zoomSteps`: `zoomStep(Details, 1) == List`, `zoomStep(Details, 3) == MediumIcons`, `zoomStep(ExtraLargeIcons, 1) == ExtraLargeIcons`, `zoomStep(Details, -1) == Details`, `zoomStep(Tiles, 1) == LargeIcons`, `zoomStep(Content, -1) == SmallIcons`.
  - `columnsMap`: `modelColumnFor("tags") == 7`, `modelColumnFor("modified") == 2`, `knownColumnIds().size() == 8`, `columnTitle("modified") == "Date modified"`.
  - `globalRoundTrip`: save with `mode = LargeIcons, groupRole = Modified, columns = {"name","tags"}` for `file:///tmp/a`; `load(file:///tmp/b) == saved` (perFolder off → global).
  - `perFolderRoundTrip`: `setPerFolder(true)`; save `mode = Tiles` for `a`; `load(a).mode == Tiles`, `load(b).mode == Details`, `hasFolderSettings(a) && !hasFolderSettings(b)`; `clearFolder(a)` → `!hasFolderSettings(a)`.
  - `reopenPersists`: a second `ViewSettingsStore` on the same file reads the saved values.
- [ ] **Step 2: Build + run, expect FAIL** (header missing).
- [ ] **Step 3: Implement** with `KConfigGroup` reads/writes and `config->sync()` in `save`.
- [ ] **Step 4: Build + run** — pass.
- [ ] **Step 5: Commit** — `explorer: view settings + per-folder/global store (win12-explorerrc)`.

### Task 4: Sort and group logic

**Files:**
- Create: `explorer/src/sortgroup.h`, `explorer/src/sortgroup.cpp`, `explorer/autotests/sortgroup_test.cpp`
- Modify: `explorer/CMakeLists.txt`

**Interfaces:**
- Consumes: `SortRole`, `GroupRole` (Task 3).
- Produces:
  ```cpp
  namespace SortGroup {
  struct GroupInfo { int order; QString label; bool operator==(const GroupInfo &) const = default; };
  GroupInfo groupInfo(const KFileItem &, GroupRole, const QDateTime &now);
  bool groupLess(const GroupInfo &, const GroupInfo &);   // by order, then label (locale-aware, case-insensitive)
  int sortColumnFor(SortRole);                              // KDirModel::Name, ModifiedTime, Type, Size
  }
  ```
  Windows group buckets: **Modified** order/labels 0 "Today", 1 "Yesterday", 2 "Earlier this week", 3 "Last week", 4 "Earlier this month", 5 "Last month", 6 "Earlier this year", 7 "A long time ago" (weeks start Monday, `QLocale`-independent). **Size**: folders → `{-1, "Folders"}`; 0 "Empty (0 KB)" = 0 B, 1 "Tiny (0 - 16 KB)", 2 "Small (16 KB - 1 MB)", 3 "Medium (1 - 128 MB)", 4 "Large (128 MB - 1 GB)", 5 "Huge (1 - 4 GB)", 6 "Gigantic (> 4 GB)"; boundaries are "up to and including" (16 KiB → Tiny, 16 KiB + 1 → Small). **Name**: 0 "0-9", 1 "A-H", 2 "I-P", 3 "Q-Z", 4 "Other" on the first character, case-insensitive. **Type**: folders `{0, "File folder"}`, files `{1, item.mimeComment()}`. **None**: `{0, ""}`. Labels through `i18n()`.

- [ ] **Step 1: Write `sortgroup_test.cpp`** (`QTEST_GUILESS_MAIN`; build `KFileItem` from a `KIO::UDSEntry` with `UDS_NAME`, `UDS_SIZE`, `UDS_MODIFICATION_TIME`, `UDS_FILE_TYPE`, `UDS_MIME_TYPE` and a `QUrl("file:///t/" + name)`; `now` = `QDateTime(QDate(2026,10,23), QTime(12,0))`, a Friday):
  - `modifiedBuckets_data/modifiedBuckets`: offsets → labels: `0 days → Today`, `-1 → Yesterday`, `-3 (Tue) → Earlier this week`, `-5 (Sun) → Last week`, `-9 → Last week`, `-13 → Earlier this month`, `-30 (Sep 23) → Last month`, `-70 → Earlier this year`, `-400 → A long time ago`.
  - `sizeBuckets`: 0 → Empty; 16384 → Tiny; 16385 → Small; `1<<20` → Small; `(1<<20)+1` → Medium; `128<<20` → Medium; `1<<30` → Large; `4ull<<30` → Huge; `(4ull<<30)+1` → Gigantic; a directory → order -1 "Folders".
  - `nameBuckets`: "7up" → 0-9; "apple" → A-H; "Hat" → A-H; "iris" → I-P; "Zoo" → Q-Z; "_x" → Other; "éclair" → Other.
  - `typeGroups`: directory → `{0,"File folder"}`; `text/plain` file → order 1, label == `QMimeDatabase().mimeTypeForName("text/plain").comment()`.
  - `groupLessOrdersThenLabel`: `groupLess({0,"b"},{1,"a"})`, `groupLess({1,"a"},{1,"B"})`, `!groupLess({1,"b"},{1,"a"})`.
  - `sortColumns`: `sortColumnFor(SortRole::Modified) == KDirModel::ModifiedTime`, `Size == KDirModel::Size`, `Type == KDirModel::Type`.
- [ ] **Step 2: Build + run, expect FAIL.**
- [ ] **Step 3: Implement `sortgroup.cpp`** (date bucket algorithm: compare `item.time(KFileItem::ModificationTime).date()` with `now.date()`: same day; day-1; same ISO week; previous ISO week; same month; previous month; same year; else).
- [ ] **Step 4: Build + run** — pass.
- [ ] **Step 5: Commit** — `explorer: Windows group buckets (date, size, name, type) and sort columns`.

### Task 5: Folder model chain and preview loader

**Files:**
- Create: `explorer/src/foldermodel.h`, `explorer/src/foldermodel.cpp`, `explorer/autotests/foldermodel_test.cpp`
- Modify: `explorer/CMakeLists.txt`

**Interfaces:**
- Consumes: `SortGroup::groupInfo/groupLess/sortColumnFor` (Task 4), `GroupRole`, `SortRole` (Task 3).
- Produces:
  ```cpp
  class FolderModel : public KDirSortFilterProxyModel {      // source: its own KDirModel (KDirLister: autoUpdate on, delayedMimeTypes on)
  public:
      enum Roles { GroupOrderRole = Qt::UserRole + 100, GroupLabelRole, GroupFirstRole /*bool: first row of its group*/, PreviewRole /*QPixmap or invalid*/ };
      explicit FolderModel(QObject *parent = nullptr);
      KDirModel *dirModel() const;  KDirLister *lister() const;
      void openUrl(const QUrl &);   QUrl url() const;              // lister()->openUrl(url, KDirLister::NoFlags)
      void setGroupRole(GroupRole); GroupRole groupRole() const;   // invalidates sort
      void setSorting(SortRole, Qt::SortOrder);                    // sort(SortGroup::sortColumnFor(role), order)
      void setPreviewLoader(PreviewLoader *);                      // Qt::DecorationRole on column 0 returns the preview when one is cached
      KFileItem itemFor(const QModelIndex &proxyIndex) const;
      QModelIndex indexFor(const QUrl &) const;
  protected:
      bool lessThan(const QModelIndex &, const QModelIndex &) const override;   // groupLess first (when groupRole != None), then KDirSortFilterProxyModel::lessThan
  };
  class TagsColumns : public KExtraColumnsProxyModel {
  public:
      static constexpr int TagsColumn = KDirModel::ColumnCount;    // 7
      static constexpr int TagsRole = Qt::UserRole + 101;          // QStringList
      explicit TagsColumns(FolderModel *source, QObject *parent = nullptr);   // appendColumn(i18n("Tags"))
      FolderModel *folderModel() const;
      KFileItem itemFor(const QModelIndex &) const;                // maps through both proxies
      QVariant extraColumnData(const QModelIndex &parent, int row, int extraColumn, int role) const override;  // phase 1: DisplayRole → QString(), TagsRole → QStringList()
  };
  class PreviewLoader : public QObject {
  public:
      PreviewLoader(FolderModel *model, QObject *parent = nullptr);
      void setView(QAbstractItemView *);     // visible rows come from view->indexAt(viewport corners); nullptr = no scheduling
      void setEnabled(bool); void setSize(int px);   // size change clears the cache
      QPixmap previewFor(const KFileItem &) const;   // cache key: url + mtime; invalid pixmap when not loaded
      void requestVisible();                 // debounced 50 ms; one KIO::PreviewJob (KIO::PreviewJob::availablePlugins(), size QSize(px,px)) for visible rows first, then the rest in batches of 100
  signals:
      void previewReady(const QUrl &);       // FolderModel emits dataChanged(DecorationRole) for that row
  };
  ```

- [ ] **Step 1: Write `foldermodel_test.cpp`** (`QTEST_MAIN`; each test lists a `QTemporaryDir` and waits on `lister()->listingDirCompleted` with `QSignalSpy::wait(5000)`):
  - `listsAndSortsFoldersFirst`: create `b.txt`, `a.txt`, folder `zdir`; after listing, display names in column 0 order == `{"zdir","a.txt","b.txt"}`; `setSorting(Name, Descending)` → `{"zdir","b.txt","a.txt"}` (folders stay first).
  - `groupsBySizeThenName`: files of 0 B, 20 KiB, 2 MiB named `c`, `a`, `b`; `setGroupRole(Size)` → order `{"c","a","b"}`; `data(row0, GroupLabelRole) == "Empty (0 KB)"`, `GroupFirstRole` true for rows 0,1,2 (three groups); with `GroupRole::None` → `{"a","b","c"}`.
  - `tagsColumnIsLastAndEmpty`: `TagsColumns t(&model)`; `t.columnCount() == 8`; `t.headerData(7, Qt::Horizontal) == "Tags"`; `t.index(0, 7).data(TagsColumns::TagsRole).toStringList().isEmpty()`; `t.itemFor(t.index(0, 7)).name() == model.itemFor(model.index(0, 0)).name()`.
  - `largeFolderKeepsEventLoopFree` (Review Focus 1): create 5000 empty files (`QFile` in a loop before listing); start a `QTimer` at 10 ms that records the max gap between ticks with `QElapsedTimer`; wait for `listingDirCompleted`; `rowCount() == 5000`; max gap < 250 ms.
  - `previewLoaderCachesByUrl`: `QSKIP` when `KIO::PreviewJob::availablePlugins().isEmpty()`; write a 64x64 PNG with `QImage::save`; a shown offscreen `QListView` on the model; `PreviewLoader p(&model); p.setView(&listView); p.setSize(48); p.setEnabled(true); model.setPreviewLoader(&p)`; after listing `p.requestVisible()`; `QSignalSpy(previewReady).wait(5000)`; `p.previewFor(item).width() <= 48` and `model.index(row, 0).data(Qt::DecorationRole).value<QPixmap>()` not null.
- [ ] **Step 2: Build + run, expect FAIL.**
- [ ] **Step 3: Implement `foldermodel.cpp`**. `GroupFirstRole`: true when `row == 0` or `groupInfo(row-1) != groupInfo(row)` (groupInfo computed with `QDateTime::currentDateTime()` cached per sort pass). `lessThan` must stay stable under `KDirSortFilterProxyModel::sortFoldersFirst()` (keep folders first inside each group except Size, where folders are their own group).
- [ ] **Step 4: Build + run** — pass (`largeFolderKeepsEventLoopFree` runs under 10 s).
- [ ] **Step 5: Commit** — `explorer: folder model chain (KDirModel → FolderModel → TagsColumns) + PreviewJob loader`.

### Task 6: FolderView widget

**Files:**
- Create: `explorer/src/folderview.h`, `explorer/src/folderview.cpp`, `explorer/autotests/folderview_test.cpp`
- Modify: `explorer/CMakeLists.txt`

**Interfaces:**
- Consumes: `FolderModel`, `TagsColumns`, `PreviewLoader` (Task 5); `ViewSettings`, `ViewSettingsStore`, `iconSizeFor`, `zoomStep`, `modelColumnFor`, `columnTitle` (Task 3).
- Produces:
  ```cpp
  class FolderView : public QWidget {
      Q_OBJECT
  public:
      explicit FolderView(ViewSettingsStore *store, QWidget *parent = nullptr);
      void openUrl(const QUrl &);  QUrl url() const;           // pushes NavHistory, loads store->load(url), applies it
      bool goBack(); bool goForward(); bool goUp(); bool canGoBack() const; bool canGoForward() const; void refresh();
      void applySettings(const ViewSettings &); ViewSettings settings() const;   // also store->save(url(), s)
      void setViewMode(ViewMode); void zoom(int steps);         // zoom = applySettings with zoomStep
      KFileItemList selectedItems() const; KFileItem currentItem() const; int itemCount() const; int selectedCount() const;
      void selectAll(); void selectNone(); void invertSelection(); void selectUrls(const QList<QUrl> &);
      void renameCurrent();                                     // opens the inline editor (F2)
      void setNameFilter(const QString &);                      // FolderModel::setFilterFixedString, case-insensitive
      QAbstractItemView *currentView() const; TagsColumns *model() const;
      static QString displayName(const KFileItem &, bool showExtensions);   // the delegate's text: suffix of non-folders hidden when false; dot-files keep their name
  signals:
      void urlChanged(const QUrl &); void selectionChanged(); void countsChanged(int items, int selected);
      void fileActivated(const KFileItem &);                    // double-click/Enter on a non-folder
      void folderMiddleClicked(const QUrl &);                   // tab request
      void contextMenuRequested(const QPoint &globalPos, const KFileItemList &items);   // items empty = folder background
      void renameRequested(const KFileItem &, const QString &newName);
      void dropRequested(QDropEvent *, const QUrl &destination);  // destination: folder under the cursor, else url()
  };
  ```
  Internals (one file, the implementer's choice of helpers is fine): `QStackedWidget` with a `QListView` (icon modes: `IconMode`, `LeftToRight`, wrapping, `setUniformItemSizes(true)` unless grouped; List: `ListMode`, `TopToBottom`, wrapping; Content: `ListMode`, `TopToBottom`, no wrap) and a `QTreeView` (details: `rootIsDecorated false`, `sortingEnabled true`, header sections shown/ordered from `settings().columns`, header context menu lists `knownColumnIds()` with check marks = "Add columns"); both views share one `QItemSelectionModel`; `FolderDelegate : QStyledItemDelegate` paints icon/preview, name (`showExtensions == false` hides the suffix of non-folders for display only), the check box (when `checkBoxes`, 16 px top-left on hover/selection; clicking its rect toggles selection without moving the current index), the group header strip (when grouped: `sizeHint` grows by `fontMetrics.height()*2` for rows whose `GroupFirstRole` is true; header text = `GroupLabelRole`, palette `Highlight` text colour), Tiles (48 px icon, name + type + size beside) and Content (32 px icon, name, modified, size in one wide row). Edit triggers: `EditKeyPressed` only; the editor commits through `renameRequested` (the model is never asked to rename). Drag: `setDragEnabled(true)`, `setDragDropMode(DragDrop)`, `setDefaultDropAction(Qt::MoveAction)`; drops come out as `dropRequested`. Type-ahead: `QAbstractItemView::keyboardSearch` (default). Ctrl+wheel on the viewport → `zoom(±1)`. Middle click on a folder → `folderMiddleClicked`. `NavHistory`: `QList<QUrl>` + index, max 50.

- [ ] **Step 1: Write `folderview_test.cpp`** (`QTEST_MAIN`; offscreen; a `QTemporaryDir` with `a.txt`, `b.txt`, `sub/`; a `ViewSettingsStore` on a temp config; `view.show()` + `QTest::qWaitForWindowExposed`; wait for `view.model()->folderModel()->lister()->listingDirCompleted`):
  - `opensAndCounts`: `itemCount() == 3`, `url() == dir url`, `countsChanged` seen with `(3, 0)`.
  - `selectionHelpers`: `selectAll()` → `selectedCount() == 3`; `invertSelection()` → 0; `selectUrls({a.txt})` → `selectedItems().first().name() == "a.txt"`; `selectNone()` → 0.
  - `modeSwitchKeepsSelection`: select `a.txt`; `setViewMode(ViewMode::Details)` then `LargeIcons`; `selectedItems().first().name() == "a.txt"` both times; `qobject_cast<QTreeView*>(currentView())` non-null only in Details; `settings().mode` follows; `store.load(url()).mode == LargeIcons` (saved).
  - `zoomFollowsSteps`: from Details `zoom(1)` → `settings().mode == List`; `zoom(-1)` → Details.
  - `detailsColumnsFollowSettings`: `applySettings` with `columns = {"name","size","tags"}` in Details; the header shows exactly sections 0, 1, 7 (`!isSectionHidden`) in visual order 0,1,7; `headerData(7) == "Tags"`.
  - `navHistory`: `openUrl(sub)` → `canGoBack()`; `goBack()` → `url() == dir`, `canGoForward()`; `goUp()` from `sub` → `url() == dir`.
  - `renameGoesThroughSignal`: select `a.txt`, `renameCurrent()`, find the open `QLineEdit` editor via `currentView()->findChild<QLineEdit*>()`, set text `"c.txt"`, `QTest::keyClick(editor, Qt::Key_Return)`; `QSignalSpy(renameRequested)` has one entry with name `"c.txt"`; `a.txt` still exists on disk (the view never renames by itself).
  - `nameFilter`: `setNameFilter("b")` → `itemCount() == 1`; `setNameFilter("")` → 3.
  - `extensionsHidden`: `FolderView::displayName(a.txt, false) == "a"`, `displayName(a.txt, true) == "a.txt"`, `displayName(sub, false) == "sub"`, `displayName(".bashrc", false) == ".bashrc"`, `displayName("archive.tar.gz", false) == "archive.tar"`.
- [ ] **Step 2: Build + run, expect FAIL.**
- [ ] **Step 3: Implement `folderview.cpp`** (largest task; keep the delegate and `NavHistory` as private classes in the same `.cpp`).
- [ ] **Step 4: Build + run** — pass.
- [ ] **Step 5: Manual check** (temporary `main.cpp` hook is fine, revert before commit): `win12-explorer` showing a `FolderView` on `~`: Ctrl+wheel zooms, F2 opens the editor, rubber band and Shift/Ctrl select, dragging a file to another window's folder offers Move/Copy (KIO menu comes in Task 7), 20k-file folder scrolls without stutter.
- [ ] **Step 6: Commit** — `explorer: FolderView (icon/list/details/tiles/content, check boxes, groups, zoom, history)`.

### Task 7: ExplorerActions (ribbon actions, KIO work, context menu)

**Files:**
- Create: `explorer/src/exploreractions.h`, `explorer/src/exploreractions.cpp`, `explorer/autotests/exploreractions_test.cpp`
- Modify: `explorer/CMakeLists.txt`, `explorer/src/folderview.cpp` (connect `dropRequested` default handler: `KIO::drop(event, destination)` with `KIO::FileUndoManager::self()->recordCopyJob`; the view keeps emitting the signal for tests)

**Interfaces:**
- Consumes: `Ribbon::actionIds()`, `Ribbon::button()` (Task 2); `FolderView` (Task 6); `ViewSettings` (Task 3).
- Produces:
  ```cpp
  class ExplorerActions : public QObject {
      Q_OBJECT
  public:
      explicit ExplorerActions(QWidget *windowParent);          // one QAction per Ribbon::actionIds(); text/icon from the spec; shortcuts below
      QAction *action(const QString &id) const;                 // nullptr for unknown ids
      void setActiveView(FolderView *);                         // the view the actions operate on; updates enabled states on its selectionChanged/urlChanged
      QMenu *contextMenu(const KFileItemList &items, const QUrl &folder);   // caller owns; see below
      QMenu *newItemMenu() const;                               // KNewFileMenu's menu (home_new_item); folder follows setActiveView
      QMenu *historyMenu() const;                               // filled by ExplorerWindow (Task 9) from RecentFolders
      static bool rename(const KFileItem &item, const QString &newName, QWidget *parent);   // KIO::moveAs(item.url(), sibling(newName), KIO::HideProgressInfo) run synchronously with job->exec(); false and no change when the target exists (KIO::ERR_FILE_ALREADY_EXIST) or newName is empty/contains '/'; records KIO::FileUndoManager::Rename on success
  signals:
      void newWindowRequested(); void closeRequested(); void optionsRequested(); void helpRequested();
      void layoutRequested(ViewMode); void sortRequested(SortRole, Qt::SortOrder); void groupRequested(GroupRole); void columnsToggled(const QString &id, bool on);
      void paneToggled(const QString &pane, bool on);           // "nav" | "preview" | "details"
      void splitToggled(bool); void showToggled(const QString &what, bool on);   // "checkboxes" | "extensions" | "hidden"
      void fillWin12Menu(QMenu *, const KFileItemList &);       // seam for phases 2-4
  };
  ```
  Behaviour per id: `home_copy`/`home_cut` → `KIO::setClipboardDataCut` on `QApplication::clipboard()` with the items' `mimeData`; `home_paste` → `KIO::paste(clipboard mime, folder)` recorded in FileUndoManager; `home_copy_path` → local paths joined by `\n`; `home_paste_shortcut` → `KIO::link(urls, folder)` recorded; `home_undo`/`home_redo` → `KIO::FileUndoManager::self()->undo()/redo()` with enabled state from `undoAvailable`/`redoAvailable` (redo only when the manager offers it; otherwise hidden); `home_move_to`/`home_copy_to` → `QFileDialog::getExistingDirectoryUrl` then `KIO::move/copy` recorded; `home_delete` menu: "Move to Trash" (`KIO::trash` recorded, Delete key) and "Delete permanently" (`KIO::del` after `KIO::JobUiDelegate::askDeleteConfirmation`, Shift+Delete), the big button itself = trash; `home_rename` → `view->renameCurrent()` (F2); `home_new_folder` → `KNewFileMenu::createDirectory()`; `home_properties` → `KPropertiesDialog::showDialog(items or folder)`; `home_open` menu → `KFileItemActions::insertOpenWithActionsTo`; `home_edit` → `KIO::OpenUrlJob` with `setRunExecutables(false)`; `home_pin` → `KFilePlacesModel().addPlace(name, url)` for the current folder or the selected folder; `home_select_*` → the view; `share_share` menu → `Purpose::Menu` with `setInputData({"urls": [...], "mimeType": first item's mime})` and `model()->setPluginType("Export")`; `share_email` → `KIO::CommandLauncherJob("xdg-email", {"--attach", path...})`; `share_zip` → `KIO::CommandLauncherJob("ark", {"--add", "--dialog", "--changetofirstpath", paths...})`; `share_print` → `KIO::CommandLauncherJob("lp", paths)` (CUPS prints PDF/text/images; enabled only for local files); `share_terminal`/`file_terminal` → `KTerminalLauncherJob(QString(), workingDirectory = folder)`; `share_security` → `KPropertiesDialog` then `findChild<KPageWidget*>()` set to the page whose `name() == i18n("Permissions")`, falling back to the first page; `file_options` → `KIO::CommandLauncherJob("win12-settings")`; `file_help` → `KIO::OpenUrlJob(QUrl("https://github.com/Kalaminator/Win12"))`; view_* emit the signals. Shortcuts: Ctrl+C/X/V, Ctrl+Z, Ctrl+Shift+Z, F2, Delete, Shift+Delete, Ctrl+A, Ctrl+Shift+N (new folder), Alt+Return (properties), Ctrl+H (hidden), F4 (split), F11 (nav pane). Enabled states: selection-dependent ids disabled with an empty selection; paste enabled when the clipboard has urls.
  `contextMenu(items, folder)`: items non-empty → Open (default app via `KIO::OpenUrlJob`), `home_open` ("Open with" from `KFileItemActions::insertOpenWithActionsTo`), separator, cut/copy/paste shortcut, separator, `KFileItemActions::addActionsTo(menu, KFileItemActions::MenuActionSource::Services)`, separator, Win12 ▸ (only when `fillWin12Menu` added actions), separator, trash/delete, rename, separator, properties. Empty items (background) → view_layout submenu ("View"), sort/group submenus, Refresh, paste, paste shortcut, new folder/new item, separator, Win12 ▸, properties.

- [ ] **Step 1: Write `exploreractions_test.cpp`** (`QTEST_MAIN`):
  - `everySpecIdHasAnAction`: for each `Ribbon::actionIds()`, `actions.action(id) != nullptr`, text == spec text, `action->icon().name()` == spec icon (or not null when the theme lacks it), `action(id)->menu() != nullptr` iff spec `menu` (`home_history`, `view_sort`, `view_group`, `view_columns`, `home_new_item`, `home_delete`, `home_open`, `share_share`).
  - `shortcuts`: `action("home_copy")->shortcut() == QKeySequence::Copy`, `home_undo == QKeySequence::Undo`, `home_rename == Qt::Key_F2`, `view_split == Qt::Key_F4`.
  - `renameRefusesExistingName` (Review Focus 2): temp dir with `a.txt`, `b.txt`; `ExplorerActions::rename(a, "b.txt", nullptr) == false`; both files still exist with their original sizes; `rename(a, "c.txt", nullptr) == true` → `c.txt` exists, `a.txt` gone; `rename(c, "x/y", nullptr) == false`; `rename(c, "", nullptr) == false`.
  - `contextMenuHasWin12OnlyWhenFilled`: without a `fillWin12Menu` slot, `contextMenu(items, folder)` has no action titled "Win12"; connect a lambda that adds one action → exactly one submenu titled "Win12" with one action.
  - `enabledFollowsSelection`: `FolderView` on a temp dir with one file (wait for listing); `setActiveView(&view)`; `selectNone()` → `action("home_copy")->isEnabled() == false`, `home_new_folder` enabled; `selectAll()` → `home_copy` enabled.
- [ ] **Step 2: Build + run, expect FAIL.**
- [ ] **Step 3: Implement `exploreractions.cpp`** and the `FolderView` drop default handler.
- [ ] **Step 4: Build + run** — pass.
- [ ] **Step 5: Commit** — `explorer: actions for every ribbon id (KIO jobs, Undo, open-with, service menus, Win12 ▸ hook)`.

### Task 8: RibbonBar widget

**Files:**
- Create: `explorer/src/ribbonbar.h`, `explorer/src/ribbonbar.cpp`, `explorer/autotests/ribbonbar_test.cpp`
- Modify: `explorer/CMakeLists.txt`

**Interfaces:**
- Consumes: `Ribbon::spec()`, `Ribbon::currentPhase` (Task 2); `ExplorerActions::action()` (Task 7).
- Produces:
  ```cpp
  class RibbonBar : public QWidget {
      Q_OBJECT
  public:
      RibbonBar(ExplorerActions *actions, QWidget *parent = nullptr);
      bool isCollapsed() const; void setCollapsed(bool);      // body hidden, tab strip stays; clicking a tab while collapsed expands again (no popup overlay)
      QAction *collapseAction() const;                        // checkable, Ctrl+F1, also the ⌃/⌄ button at the strip's right edge
      QMenu *fileMenu() const;                                // the blue File tab opens it (actions file_*)
      int currentTab() const; void setCurrentTab(int);        // index into the non-File tabs: 0 Home, 1 Share, 2 View, 3 Advanced
      QList<QToolButton *> buttonsFor(const QString &actionId) const;   // for tests
      QStringList visibleGroupTitles(int tab) const;          // for tests
  signals: void collapsedChanged(bool);
  };
  ```
  Look (from `ribbon.html` + `reference-dark-explorer.png`, palette-driven so the light scheme is the twin): tab strip background `palette.base()`, strip height 28 px, tab padding 5 px × 14 px, the File tab filled with `palette.highlight()` (= kdeglobals AccentColor) and `highlightedText`, the current tab filled with the ribbon colour and a 1 px frame in `KColorScheme::shade(window, KColorScheme::MidShade)` with no bottom edge; ribbon body `palette.window()`, min height 86 px, groups separated by a 1 px `MidShade` line, group label centred below the buttons in `palette.placeholderText()` at font size − 1; big button = `QToolButton` `ToolButtonTextUnderIcon`, 52 px wide, 32 px icon, autoRaise; small = `ToolButtonTextBesideIcon`, 16 px icon, stacked in columns of up to 3; menu buttons `MenuButtonPopup` for `home_delete`, `InstantPopup` for the rest. Groups with `phase > currentPhase` are skipped; a tab with `phase > currentPhase` shows a single disabled label i18n("Advanced tools arrive in a later Win12 version.").

- [ ] **Step 1: Write `ribbonbar_test.cpp`** (`QTEST_MAIN`, `ExplorerActions a(nullptr); RibbonBar r(&a); r.show()`):
  - `tabsAndFileMenu`: `fileMenu()->actions()` texts contain "New window", "Open terminal here", "Options", "Help", "Close" in that order; `currentTab() == 0`.
  - `homeGroupsVisible`: `visibleGroupTitles(0) == {"Clipboard","Organize","New","Open","Select"}`.
  - `tagsGroupHiddenInPhase1`: `visibleGroupTitles(2)` == `{"Panes","Layout","Current view","Show/hide","Split view"}`.
  - `advancedIsPlaceholder`: `visibleGroupTitles(3).isEmpty()`; the Advanced page has a `QLabel` whose text starts with "Advanced tools".
  - `everyVisibleButtonHasItsAction`: for each id in `Ribbon::actionIds()` of groups with phase 1, `buttonsFor(id).size() == 1` and `buttonsFor(id).first()->defaultAction() == a.action(id)`; `buttonsFor("home_delete").first()->popupMode() == QToolButton::MenuButtonPopup`.
  - `collapseHidesBody`: `r.height() > 100`; `collapseAction()->trigger()` → `isCollapsed()`, body widget `!isVisible()`, strip still visible, `collapsedChanged(true)` emitted once; `collapseAction()->shortcut() == QKeySequence(Qt::CTRL | Qt::Key_F1)`.
  - `fileTabUsesAccent`: the strip's File tab rect is painted with `palette().highlight().color()` — assert via `r.grab()` pixel at the File tab centre == highlight colour (offscreen renders flat colours).
- [ ] **Step 2: Build + run, expect FAIL.**
- [ ] **Step 3: Implement `ribbonbar.cpp`** (`RibbonTabStrip : QWidget` with `paintEvent`/`mousePressEvent`, `QStackedWidget` body, groups as `QFrame` + `QGridLayout`).
- [ ] **Step 4: Build + run** — pass.
- [ ] **Step 5: Manual check**: with a dark and a light Plasma colour scheme (`plasma-apply-colorscheme BreezeDark` / `BreezeLight`) the ribbon matches `reference-dark-explorer.png` and its light twin; change the accent in Win12 Colours → the File tab follows after restart.
- [ ] **Step 6: Commit** — `explorer: RibbonBar from the spec table (tabs, groups, collapse Ctrl+F1, File menu)`.

### Task 9: ExplorerWindow (address row, tabs, split view, nav pane, side pane, status bar, session)

**Files:**
- Modify: `explorer/src/explorerwindow.h`, `explorer/src/explorerwindow.cpp` (replace Task 1's stub), `explorer/src/main.cpp` (open windows from urls; restore the session when no urls)
- Create: `explorer/autotests/session_test.cpp`
- Modify: `explorer/CMakeLists.txt`

**Interfaces:**
- Consumes: `RibbonBar` (Task 8), `ExplorerActions` and its signals (Task 7), `FolderView` (Task 6), `ViewSettingsStore` (Task 3).
- Produces:
  ```cpp
  class TabPage : public QWidget {       // QSplitter with one or two FolderViews
  public:
      explicit TabPage(ViewSettingsStore *, const QUrl &, QWidget *parent = nullptr);
      FolderView *active() const; FolderView *primary() const; FolderView *secondary() const;   // secondary nullptr when not split
      bool isSplit() const; void setSplit(bool);          // the new pane opens primary()->url()
      QUrl url() const;                                   // active()->url()
  signals: void activeChanged(FolderView *); void titleChanged(const QString &);   // last path segment or the host for remote urls
  };
  class ExplorerWindow : public KMainWindow {
      Q_OBJECT
  public:
      explicit ExplorerWindow(const QList<QUrl> &urls = {}, QWidget *parent = nullptr);   // empty → sessionUrlsToOpen(savedSession())
      void openTab(const QUrl &, bool activate = true); void closeTab(int); TabPage *currentPage() const; FolderView *activeView() const;
      void selectItems(const QList<QUrl> &);              // opens the parent folder in the current tab, then FolderView::selectUrls
      void showProperties(const QList<QUrl> &);
      static QList<QUrl> savedSession(const KSharedConfig::Ptr &);        // [Session] Tabs (url list), Active (int)
      static QList<QUrl> sessionUrlsToOpen(const QList<QUrl> &saved);    // drops local urls whose path no longer exists; empty → {QUrl::fromLocalFile(QDir::homePath())}
      static QStringList recentFolders(const KSharedConfig::Ptr &);      // [Recent] Urls, newest first, max 20
      static void addRecentFolder(const KSharedConfig::Ptr &, const QUrl &);   // moves to front, dedups, trims to 20
  protected: bool queryClose() override;                  // writes [Session] from the open tabs
  };
  ```
  Layout top to bottom (`QVBoxLayout` on `centralWidget`, spacing 0): `RibbonBar`; address row = `QToolBar` (not movable) with Back/Forward (per-view `NavHistory`; `KStandardAction::back/forward` icons), a "Recent" `QToolButton` (`InstantPopup`, menu from `recentFolders()`, same menu object as `ExplorerActions::historyMenu()`), Up, `KUrlNavigator` (`KFilePlacesModel` for its places menu, `setUrlEditable(false)`, Ctrl+L toggles editable, F6 too), Refresh, a `QLineEdit` search box (placeholder i18n("Search %1", folder name), 180 px, filters the active view through `setNameFilter`; Esc clears); a `QTabBar` (closable, movable, `setDocumentMode(true)`, `tabsClosable`, middle-click closes, "+" via Ctrl+T on the current url, Ctrl+W closes, Ctrl+Tab cycles, double-click on the empty strip = new tab on Home) + `QStackedWidget` of `TabPage`s; a horizontal `QSplitter` with `KFilePlacesView` (its own `KFilePlacesModel`, `setDropOnPlaceEnabled(true)`, `urlChanged` → `activeView()->openUrl`, 200 px default), the tab stack, and `SidePane` (hidden by default; mode Preview = big `KIO::PreviewJob` image of the current item at 256 px with name below; mode Details = name, type, size, modified; both follow `selectionChanged`); `QStatusBar` with i18np("%1 item", "%1 items", n) plus i18np(" (%1 selected)", ...) on the left and two checkable `QToolButton`s (details, large icons) on the right. Window: `setAutoSaveSettings("MainWindow")`, default 1100×700, title = active tab title + " - Win12 Explorer". Middle-click on a folder in a view → `openTab(url, false)`; `fileActivated` → `KIO::OpenUrlJob(url)` with `setRunExecutables(false)` and `KIO::JobUiDelegateFactory`; `contextMenuRequested` → `actions->contextMenu(...)->popup(pos)` (deleted on hide); `renameRequested` → `ExplorerActions::rename(item, name, this)` with a `KMessageBox::error` i18n("A file named %1 already exists.") when false; `view_split` ↔ `TabPage::setSplit`; layout/sort/group/columns/show signals → `applySettings` on the active view; pane signals → nav pane / `SidePane` visibility; `file_new_window` → `(new ExplorerWindow({activeView()->url()}))->show()`; `file_close` → `close()`. `ribbon->collapseAction()` state saved in `[MainWindow] RibbonCollapsed`. Every `urlChanged` of the active view → `addRecentFolder`, `KUrlNavigator::setLocateUrl`, title, status.
  `main.cpp`: `KDBusService::activateRequested(args, cwd)` → `parseCli` (Task 10; until then the raw positional urls) → a new `ExplorerWindow(urls)` per request (or `selectItems` when `--select`).

- [ ] **Step 1: Write `session_test.cpp`** (`QTEST_GUILESS_MAIN`, temp config):
  - `missingFoldersAreDropped` (Review Focus 5): `sessionUrlsToOpen({existingTmpDir, file:///nonexistent/x, QUrl("smb://nas/share")}) == {existingTmpDir, smb://nas/share}` (remote urls are kept untested); `sessionUrlsToOpen({file:///nonexistent}) == {home}`; `sessionUrlsToOpen({}) == {home}`.
  - `savedSessionRoundTrip`: write `[Session] Tabs=file:///a,file:///b` + `Active=1` with `KConfigGroup`; `savedSession(cfg) == {file:///a, file:///b}`.
  - `recentFoldersDedupAndTrim`: add `/a`, `/b`, `/a` → `recentFolders() == {"file:///a","file:///b"}`; add 25 distinct → size 20, newest first.
- [ ] **Step 2: Build + run, expect FAIL.**
- [ ] **Step 3: Implement `explorerwindow.cpp`** (`TabPage` and `SidePane` as classes in the same file pair) and the `main.cpp` wiring.
- [ ] **Step 4: Build + run** — all tests pass.
- [ ] **Step 5: Manual check** (`sh explorer/build.sh --force` then `win12-explorer ~`): ribbon, address row, nav pane, status bar and view-mode toggles match `ribbon.html`; Ctrl+T/Ctrl+W/middle-click tabs; F4 split; drag a file between the two panes → KIO move/copy menu, Ctrl+Z undoes; close and reopen → the same tabs; Back/Forward/Up/Recent work; "Show in folder" is still Dolphin's (Task 10 changes that).
- [ ] **Step 6: Commit** — `explorer: main window (address row, tabs, split view, places, side pane, status bar, session)`.

### Task 10: CLI, FileManager1, daemon, and the default-file-manager script

**Files:**
- Create: `explorer/src/cli.h`, `explorer/src/cli.cpp`, `explorer/src/filemanager1.h`, `explorer/src/filemanager1.cpp`, `explorer/autotests/cli_test.cpp`, `explorer/autotests/filemanager1_test.cpp`, `tests/test_explorer_default.sh`
- Modify: `explorer/win12-explorer-default` (replace Task 1's placeholder), `explorer/src/main.cpp`, `explorer/CMakeLists.txt`

**Interfaces:**
- Consumes: `ExplorerWindow::openTab/selectItems/showProperties` (Task 9).
- Produces:
  ```cpp
  struct CliOptions { bool daemon = false; bool select = false; bool newWindow = false; QList<QUrl> urls; };
  CliOptions parseCli(const QStringList &args, const QString &cwd);   // args without argv[0]; flags --daemon, --select, --new-window; relative paths resolved against cwd; "~" not expanded (the shell does); unknown "--x" ignored
  class FileManager1 : public QDBusAbstractAdaptor {   // Q_CLASSINFO("D-Bus Interface", "org.freedesktop.FileManager1"); object path /org/freedesktop/FileManager1
  public Q_SLOTS:
      void ShowFolders(const QStringList &uris, const QString &startupId);
      void ShowItems(const QStringList &uris, const QString &startupId);
      void ShowItemProperties(const QStringList &uris, const QString &startupId);
  public:
      static QMap<QUrl, QList<QUrl>> foldersFor(const QStringList &uris);   // parent folder → items (KIO::upUrl); a uri that is itself a folder with a trailing '/' maps to itself with no items
      static bool wantsRegistration();          // true when the state file <ConfigLocation>/win12-explorer/saved-default exists
      static bool registerName();               // QDBusConnection::sessionBus().registerService("org.freedesktop.FileManager1"), no replacement
      static void releaseName();                // unregisterService
  };
  class ExplorerDBus : public QDBusAbstractAdaptor {    // Q_CLASSINFO("D-Bus Interface", "org.win12.explorer"); path /org/win12/explorer
  public Q_SLOTS: void ReleaseFileManager1();           // the script's `off` calls it
  };
  ```
  `main.cpp`: `parseCli(app.arguments().mid(1), QDir::currentPath())`; `--daemon` → no window, `KDBusService::Unique`, `FileManager1::registerName()` always; otherwise register only when `wantsRegistration()`; `activateRequested(args, cwd)` → `parseCli` → `--select` → `selectItems` in a new or the last active window, else a new `ExplorerWindow(urls)`; a daemon with no windows keeps running (`setQuitOnLastWindowClosed(false)` only in daemon mode or when the name is held).
  `win12-explorer-default` (POSIX sh, `set -eu`; `C="${XDG_CONFIG_HOME:-$HOME/.config}/win12-explorer"`, `S="$C/saved-default"`, `SVC="${XDG_DATA_HOME:-$HOME/.local/share}/dbus-1/services/org.win12.explorer.FileManager1.service"`):
  - `on`: exit 0 if `$S` exists; `old=$(xdg-mime query default inode/directory || true)`; write `${old:-unset}` to `$S`; `xdg-mime default org.win12.explorer.desktop inode/directory`; write `$SVC` = `[D-BUS Service]`, `Name=org.freedesktop.FileManager1`, `Exec=/usr/bin/win12-explorer --daemon`; `busctl --user call org.freedesktop.DBus /org/freedesktop/DBus org.freedesktop.DBus ReloadConfig >/dev/null 2>&1 || true`; print `on`.
  - `off`: exit 0 if no `$S`; `old=$(cat "$S")`; `rm -f "$S" "$SVC"`; `unset` → `kwriteconfig6 --file mimeapps.list --group 'Default Applications' --key inode/directory --delete`, else `xdg-mime default "$old" inode/directory`; `busctl --user call org.win12.explorer /org/win12/explorer org.win12.explorer ReleaseFileManager1 >/dev/null 2>&1 || true`; ReloadConfig as above; print `off`.
  - `status`: print `on` if `$S` exists else `off`. Anything else: usage, exit 2.

- [ ] **Step 1: Write `cli_test.cpp`** (`QTEST_GUILESS_MAIN`):
  - `flagsAndUrls`: `parseCli({"--daemon"}, "/x").daemon` and `urls.isEmpty()`; `parseCli({"--select","/tmp/a b.txt"}, "/x")` → `select`, `urls == {file:///tmp/a%20b.txt}`; `parseCli({"docs","smb://nas/s"}, "/home/u").urls == {file:///home/u/docs, smb://nas/s}`; `parseCli({"--bogus","/tmp"}, "/").urls == {file:///tmp}`; `parseCli({"--new-window"}, "/").newWindow`.
- [ ] **Step 2: Write `filemanager1_test.cpp`** (`QTEST_GUILESS_MAIN`):
  - `foldersForGroupsByParent` (Review Focus 4): `foldersFor({"file:///home/u/a%20b/x.txt","file:///home/u/a%20b/y%23.txt","file:///tmp/z","smb://nas/share/doc.odt","file:///home/u/dir/"})` has keys `file:///home/u/a%20b` (2 items, decoded names `x.txt`, `y#.txt`), `file:///tmp` (1), `smb://nas/share` (1), `file:///home/u/dir/` (0 items).
  - `registrationFollowsStateFile`: with `QStandardPaths::setTestModeEnabled(true)`, `!wantsRegistration()`; create `<ConfigLocation>/win12-explorer/saved-default` → `wantsRegistration()`.
- [ ] **Step 3: Write `tests/test_explorer_default.sh`** (`set -e`, `T=$(mktemp -d)`, `HOME=$T XDG_CONFIG_HOME=$T/c XDG_DATA_HOME=$T/d PATH=$T/bin:$PATH`; fake `xdg-mime` that stores/reads `$T/mime`, fake `kwriteconfig6` that appends its args to `$T/kw.log`, fake `busctl` that appends to `$T/bus.log`):
  - seed `$T/mime` = `org.kde.dolphin.desktop`; `on` → `saved-default` == `org.kde.dolphin.desktop`, `$T/mime` == `org.win12.explorer.desktop`, service file exists and contains `Name=org.freedesktop.FileManager1` and `Exec=/usr/bin/win12-explorer --daemon`, `status` == `on`;
  - second `on` → saved file unchanged (Review Focus 3), mime unchanged;
  - `off` → `$T/mime` == `org.kde.dolphin.desktop`, no saved file, no service file, `bus.log` contains `ReleaseFileManager1`, `status` == `off`; second `off` → rc 0, `kw.log` empty;
  - no prior default (Review Focus 3): empty `$T/mime`; `on` → saved == `unset`; `off` → `kw.log` contains `--group Default Applications --key inode/directory --delete` and the fake `xdg-mime default` was not called again (`$T/mime` still reads `org.win12.explorer.desktop`, which only the real `kwriteconfig6 --delete` clears);
  - `bogus` → rc 2; prints `test_explorer_default OK`.
- [ ] **Step 4: Run all three, expect FAIL** (compile errors; the sh test fails on `status` output).
- [ ] **Step 5: Implement `cli.cpp`, `filemanager1.cpp`, the script, `main.cpp` wiring**; register `tests/test_explorer_default.sh` in `install.sh` next to `test_explorer_pkgbuild.sh`.
- [ ] **Step 6: Build + run** — `ctest` all pass; `sh tests/test_explorer_default.sh | tail -1` → `test_explorer_default OK`.
- [ ] **Step 7: Manual check**: `sh explorer/build.sh --force`; `win12-explorer-default on`; from Firefox "Open containing folder" → Win12 Explorer with the file selected; `xdg-open ~` → Win12 Explorer; `win12-explorer-default off` → both go back to Dolphin and `cat ~/.config/mimeapps.list` shows the old line.
- [ ] **Step 8: Commit** — `explorer: CLI, org.freedesktop.FileManager1, --daemon, win12-explorer-default on/off (saved and restored)`.

### Task 11: Plasmoid: "File Explorer" settings section, Start menu item, launcher swap, i18n extraction

**Files:**
- Modify: `package/contents/config/main.xml` (group `General`, after `configMigrated`), `tests/settings-keys.txt`, `package/contents/ui/code/tiles.js` (next to `KWIN_CHECK`), `package/contents/ui/config/ConfigWindows.qml`, `package/contents/ui/main.qml:277-278` (sync hooks), `:1487` (Start menu item), `:581-601` (launcher swap), `package/Messages.sh`

**Interfaces:**
- Consumes: `win12-explorer-default on|off` (Task 10), `/usr/bin/win12-explorer` (Task 1).
- Produces: key `explorerDefault` (Bool, default `false`); `Tiles.EXPLORER_CHECK = "test -x /usr/bin/win12-explorer && echo ok"`; `main.qml` `function syncExplorerDefault()`.

- [ ] **Step 1: Add the key** to `main.xml`: `<entry name="explorerDefault" type="Bool"><label>Use Win12 Explorer as the file manager: inode/directory default, FileManager1, Start menu item and taskbar pin (win12-explorer-default on/off saves and restores the old default)</label><default>false</default></entry>`; run `node tests/test_settings_keys.js --strict` — it passes (no control yet).
- [ ] **Step 2: Settings page** (`ConfigWindows.qml`): add imports `org.kde.plasma.plasma5support as P5Support` and `"../code/tiles.js" as Tiles`; `property alias cfg_explorerDefault: explorerBox.checked`; `property bool explorerBuilt: true` + a `P5Support.DataSource` on `Tiles.EXPLORER_CHECK` (copy the `TaskbarLayoutBox.qml:16-20` shape); at the end of the `FormLayout` (before the Windows XP separator): `Kirigami.Separator { isSection; label: i18n("File Explorer") }`, `QQC2.Label { visible: !page.explorerBuilt; wrapMode: Text.Wrap; Layout.maximumWidth: Kirigami.Units.gridUnit * 24; text: i18n("Win12 Explorer is not built. Run ./install.sh from the Win12 source folder (Arch; needs a compiler).") }`, `QQC2.CheckBox { id: explorerBox; enabled: page.explorerBuilt; text: i18n("Use Win12 Explorer as the file manager"); ToolTip: i18n("Folders, \"Show in folder\" from other apps, the Start menu File Explorer item and the taskbar pin open Win12 Explorer. Off puts your previous file manager back.") }`.
- [ ] **Step 3: Baseline the key**: `node tests/test_settings_keys.js --baseline && git diff --stat tests/settings-keys.txt` → one added line `explorerDefault`; `node tests/test_settings_keys.js --strict | tail -1` → OK (143 keys).
- [ ] **Step 4: `main.qml` sync** (next to `onWantBlurChanged` at :278): `readonly property bool explorerDefault: cfg.explorerDefault`, `onExplorerDefaultChanged: syncExplorerDefault()`, and call `root.syncExplorerDefault()` where `syncWindowDeco()` runs at startup (the brightness reader's `onNewData`, :304-305). Function: `function syncExplorerDefault() { if (!configReady || isMirror) return; root.run("win12-explorer-default " + (cfg.explorerDefault ? "on" : "off")); syncExplorerLauncher(cfg.explorerDefault); }`. `syncExplorerLauncher(on)`: one `evaluateScript` shaped like :596-601 that, for every Task Manager in this widget's panel, replaces `"applications:org.kde.dolphin.desktop"` with `"applications:org.win12.explorer.desktop"` in `launchers` when `on` and the reverse when off, writing only when the list changed.
- [ ] **Step 5: Start menu item** (:1487): `onTriggered: root.run(root.cfg.explorerDefault ? "win12-explorer" : "dolphin")`.
- [ ] **Step 6: i18n**: `package/Messages.sh` gains a second `xgettext -L C++ --from-code=UTF-8 --keyword=i18n --keyword=i18nc:1c,2 --keyword=i18np:1,2 --keyword=i18ncp:1c,2,3 -o explorer/po/template.pot $(find explorer/src -name '*.cpp' | sort)` after `mkdir -p explorer/po`; run it; commit the new `explorer/po/template.pot` (`ki18n_install(po)` from Task 1 picks up `.po` files when translators add them).
- [ ] **Step 7: Verify**: `qmllint package/contents/ui/main.qml package/contents/ui/config/ConfigWindows.qml 2>&1 | grep -c error` → 0; `node tests/test_settings_keys.js --strict | tail -1`; `./install.sh --restart 2>&1 | tail -3`.
- [ ] **Step 8: Manual check**: Win12 Settings → Windows → "File Explorer": switch on → `win12-explorer-default status` prints `on`, Start menu File Explorer opens Win12 Explorer, the pinned Dolphin on the taskbar now launches Win12 Explorer; switch off → `status` prints `off`, `xdg-mime query default inode/directory` is the old value, the pin is Dolphin again. Then `sudo pacman -R win12-explorer` → the section shows the "not built" line and a disabled switch; reinstall with `./install.sh`.
- [ ] **Step 9: Commit** — `explorer: settings switch "Use Win12 Explorer as the file manager" (saved/restored), Start menu + pin follow it, i18n extraction`.

---

## Self-review notes

- Spec coverage (Phase 1 bullets → tasks): layout (9), own view features (5, 6, 7 drop/rename), ribbon tabs/groups/buttons (2, 7, 8), collapse + Ctrl+scroll (8, 6), tabs restored (9), nav pane without rewriting Places (9, `KFilePlacesModel` API only), right-click menu with `KFileItemActions` + Win12 ▸ (7), Undo/Redo (7), theme from palette/accent (8), settings section + mime default + FileManager1 + Start menu/pin + "not built" line (10, 11), Arch package via install.sh (1), own config (3), translations (11). Non-goals and anti-goals are negative requirements enforced by the Global Constraints; the bulk-action anti-goals belong to phases 3-4.
- Known gaps left for the user: `file_options` opens Win12 Settings as a window; `win12-settings` cannot pick the Windows page (the plasmoid API has no page selector), so the spec's "on the Windows page" is approximated. "Show tag flags on icons" is deferred to phase 2 with its key. The `--daemon` process holding FileManager1 while Dolphin is also running means whichever asked first wins the name until logout (documented behaviour, not fixable without a Dolphin change).

# Win12 Explorer (design)

Approved mockups: `docs/superpowers/specs/2026-10-23-win12-explorer-mockups/` (`ribbon.html`,
`tags-look-v2.html`, `batch-tools.html`, `formulas.html`). Open them in a browser; they are the look
the user approved. The user's own references are in the same folder: `reference-dark-explorer.png`
(the dark ribbon Explorer to match) and `reference-tag-ribbons.png` (the arrow-ribbon tag shape).

## Intent

Win12 users want a file manager with the Windows Explorer ribbon look (File · Home · Share · View,
as in Windows 10 and in Windows 11 with the ribbon brought back), dark and light, plus things
Windows never had: coloured tags and an **Advanced** tab for bulk work with Excel-like formulas and
VBA-like scripts. It must keep what Dolphin does better: tabs, split view, network folders and
phones (KIO), Undo, previews, inotify refresh, drag and drop on Wayland.

Dolphin's main window cannot show a tabbed ribbon (only plain KXMLGUI toolbars), so we build our own
window, **Win12 Explorer**. Dolphin's own view (`dolphinpart`) was considered and rejected by the
user: it can only show tags as text, cannot align tag slots and groups by the whole tag set. So
Win12 Explorer has **its own file view**, built from the same KIO building blocks Dolphin uses
(`KDirLister`/`KDirModel` with inotify refresh, `KIO::PreviewJob` thumbnails, KIO copy/move/drop
jobs, `KFileItemActions`, `KIO::FileUndoManager`, `KUrlNavigator`, `KFilePlacesView`). That keeps
Dolphin's engine (network folders, phones, Undo, previews, Wayland drag and drop) while every list
can draw coloured tag ribbons. Dolphin itself is not changed.

Research (two clones in `~/Documents/Start12-stardock-assets/`):
- **itamiiru** (Rust, egui): Win11 look, fast, ~280 tests; no Wayland drag and drop, no Undo,
  polling refresh. Ideas taken: streaming everything off the UI thread; conflict dialog with "apply
  to all"; custom right-click actions (we get these free through KDE service menus).
- **RibbonFM** (Python, GTK3): Windows 7/8 ribbon, whole-folder loading, no progress or cancel.
  Ideas taken: data-driven ribbon layout (`ribbon_spec.py`), File/Home/Share/View tab contents,
  collapsible ribbon, Ctrl+scroll zoom, item check boxes, Select all/none/invert.

## Phases (build in this order; each phase ships on its own)

1. **Explorer shell**: window, ribbon, address row, nav pane, tabs, split view, theme, settings.
2. **Tags**: coloured word tags, View-tab Tags group, grouping, flags on icons.
3. **Advanced, simple**: Select by rule, bulk actions, Rename many (blocks).
4. **Advanced, formulas and scripts**: formula mode for 3, script editor, saved rules.

## Goals

### Phase 1: Explorer shell (see `ribbon.html`)

- Layout top to bottom: ribbon tabs, ribbon, address row (Back, Forward, Recent ▾, Up, breadcrumb
  `KUrlNavigator`, Refresh, search box), folder tabs, then nav pane + one or two file views
  (split view), status bar ("10 items", view-mode toggles bottom right, as in the user's screenshot).
- Own file view, at least what Dolphin's view does today: view modes (extra large / large / medium /
  small icons, list, details, tiles, content), thumbnails (KIO previews, visible items first),
  details columns (Name, Date modified, Type, Size, Tags, more via Add columns), sort, group by,
  rubber-band and Ctrl/Shift selection, item check boxes, inline rename (F2), drag and drop in and
  out (KIO drop jobs, Wayland included), type-ahead, keyboard navigation, Ctrl+scroll zoom,
  per-folder or global view settings in Win12 Explorer's own config (not `dolphinrc`). Big folders
  must stay smooth: async listing, no file-system work on the UI thread, a virtualised view.
- Ribbon tabs and groups (Windows names; each button calls a view action or a KIO job):
  - **File** (blue tab, opens a menu): New window, Open terminal here, Options (Win12 Settings on
    the Windows page), Help, Close.
  - **Home**: Clipboard (Pin to Quick access = add place, Copy, Paste, Cut, Copy path, Paste
    shortcut = link), Organize (Move to, Copy to, Delete ▾ trash/forever, Rename), New (New folder,
    New item ▾ = `new_menu`), Open (Properties, Open ▾ with, Edit, History = recent), Select (all,
    none, invert).
  - **Share**: Share ▾ (KDE Purpose), Email, Zip (compress), Print, Open terminal, Advanced
    security = Properties permissions tab.
  - **View**: Panes (Navigation pane, Preview pane, Details pane), Layout (extra large / large /
    medium / small icons, list, details, tiles, content), Current view (Sort by, Group by, Add
    columns), Tags group (phase 2), Show/hide (item check boxes, file name extensions, hidden items),
    Split view.
  - **Advanced**: phases 3 and 4 (see `ribbon.html` second window).
- Ribbon collapses (Ctrl+F1 and a ⌃ button), Ctrl+scroll zooms the view.
- Tabs: open, close, reorder, middle-click a folder = new tab; restored on next start like Dolphin.
- Nav pane: `KFilePlacesView` (the user's own Places: Quick access/Home, Desktop, Downloads, …,
  Devices = This PC, Network, Tags). The user's places file is never rewritten.
- Right-click menu built by Win12 Explorer: view actions + `KFileItemActions` (Open with, service
  menus, Win12 ▸ items from phases 2-4).
- Undo/Redo through `KIO::FileUndoManager` (Ctrl+Z), shown in Home.
- Theme follows the KDE colour scheme and `kdeglobals` AccentColor, which Win12 already sets from
  its Colours page (a Qt app gets these for free; no state file from the plasmoid). (Builder-check call.) Ribbon drawn to match the
  dark Windows screenshot the user gave (and its light twin).
- Win12 settings, **Windows** page, new section **"File Explorer"**:
  - "Use Win12 Explorer as the file manager" (default off). While on: folders open in it
    (`inode/directory` default in `mimeapps.list`, saved and restored), it answers
    `org.freedesktop.FileManager1` ("Show in folder" from other apps), and the Start menu "File
    Explorer" item (`main.qml`, today `root.run("dolphin")`) and the taskbar launcher open it.
  - "Show tag flags on icons" (phase 2, default on).
  - If Win12 Explorer is not built (store install without a compiler), the section says so in one
    line and the switch is disabled.

### Phase 2: Tags (see `tags-look-v2.html`)

- A tag = one word + one colour from the 48-colour Windows accent palette
  (`windowsColours` in `package/contents/ui/config/ConfigAppearance.qml`). Same word with another
  colour is another tag ("Xiaomi" red ≠ "Xiaomi" blue). A colour alone (no word) is allowed.
- Up to **3 tags** per file or folder. With 3, further tags are greyed out ("max 3 tags").
- Drawn as an arrow ribbon with one word (white text on the colour), like the user's reference image.
- In lists, each tag gets a slot as wide as the longest tag shown, so the same tags line up in
  columns.
- View ▸ Tags group: tag gallery, Add tag ▾, Remove tag, Manage tags… (rename, recolour, delete a
  tag everywhere), Group: tag name / Group: tag colour, Show tag flags.
- Grouping: by tag name or by tag colour; a file with two tags appears in both groups.
- Nav pane tag list: click = filter to that tag; **Ctrl+click** picks more; a switch "all picked
  tags / any picked tag". Results come from Baloo where the location is indexed, otherwise from a
  walk of the current folder (and subfolders) off the UI thread, so USB and NTFS drives work too.
  (Builder-check call.)
- Storage: the standard freedesktop tag attribute (`user.xdg.tags` via `KFileMetaData::UserMetaData`)
  so Dolphin, Baloo search and the `tags:/` places see them. In Dolphin a tag reads as
  `<ColourName>/<Word>` (e.g. `Red/Xiaomi`), so `tags:/Red/Xiaomi` browses by colour then word.
  Exact colour per tag lives in a small Win12 registry file (`$XDG_DATA_HOME/win12/tags.json`).
- In Win12 Explorer's own view the ribbons are drawn in the Tags column and next to names in the
  other modes, with aligned slots, and grouping by tag puts a file in every matching group.
- Flags on icons **in Dolphin** (Dolphin stays installed and is still used by other apps): a `KOverlayIconPlugin`
  returns one overlay icon *name* per file. KIconLoader puts the first overlay on the
  **bottom-right corner**. Win12 generates one emblem icon per tag combination (1-3 small flags
  stacked, e.g. `win12-tags-d13438-ff8c00`) into the user's icon theme folder and returns that name,
  so all of a file's flags sit together in that corner.
- Right-click ▸ Tags ▸ recent tags in one click, New tag…
- Where the file system cannot hold tags (FAT/exFAT USB, maybe NTFS), the Tags actions say "tags are
  not supported on this drive" instead of failing silently.

### Phase 3: Advanced, simple (see `batch-tools.html`)

- **Select by rule**: look-in folder + "include subfolders"; rule rows joined by AND: field (name,
  extension, size, date modified/created, tag, folder or file) · test (starts with, contains, ends
  with, equals, regex, older/newer than, bigger/smaller than, has tag) · value. Result list with
  count and total size; untick to keep an item. Actions on the ticked items: Add tag, Move to…,
  Rename…, Move to Trash. "Delete forever" is in a ⋯ menu and asks first.
- **Rename many**: the new name is built from blocks (Number with digits/start/step, Text, Current
  name, Date, Tag, Find & replace, case). Drag blocks to reorder. Extension always kept. Live
  preview "Now → After"; a name that already exists in the folder is shown red and skipped. Order =
  as shown in the view. After the run, "Undo rename" stays for that batch.
- Both open from the Advanced tab and from right-click ▸ Win12 ▸ … in Dolphin and in Win12 Explorer.

### Phase 4: Formulas and scripts (see `formulas.html`)

- Model: every file/folder is a row, its properties are columns; a formula runs once per row (the
  loop is automatic). Shown as an Excel-like grid with an extra green result column.
- **Formula** (`fx` mode of Select by rule and of Rename many): Excel syntax (`=`, `&`, `<>`,
  functions, strings in quotes), size literals like `10MB`. Columns: NAME, EXT, FULLNAME, FOLDER,
  PATH, SIZE, MODIFIED, CREATED, ISFOLDER, DEPTH, TAGS, photo WIDTH/HEIGHT/TAKEN. Functions: LEFT
  RIGHT MID LEN FIND SUBSTITUTE UPPER LOWER PROPER TRIM TEXT CONTAINS STARTSWITH ENDSWITH REGEX ·
  IF IFS AND OR NOT · TODAY DAYS YEAR MONTH DAY · HASTAG(word[,colour]) COUNTTAGS ROW ROWINFOLDER
  COUNTIN(folder). Excel names wherever Excel has one.
- **Script** (VBA-like): `For Each f In Files("~/Downloads", Subfolders:=True) … Next`,
  `If/ElseIf/Else/End If`, variables, the same functions, file members (`.Name .Ext .Size
  .Modified .Path .Folder .Tags .Contains()`) and actions (`.MoveTo .CopyTo .Rename .Tag .Untag
  .Trash`).
- Formulas and scripts only build a **plan** (dry run table). Nothing changes until "Run plan".
  Running uses KIO jobs, so progress, cancel, conflict handling and Undo work.
- Saved formulas and scripts: named, listed in Advanced ▸ My rules / My scripts and in right-click
  ▸ Win12 ▸ My rules.

## Non-goals

- Win12 does not change Dolphin's settings, toolbars or code; no patched Dolphin.
- No Windows 11 command-bar mode (the user chose the ribbon).
- No quick-access toolbar in the title bar (KWin draws the title bar).
- No rewriting of the user's Places file; no new icon theme.
- No "fast open" daemon: measured 2026-10-23, Dolphin windows map in ~130-165 ms either way.
- No Windows/macOS port.

## Anti-goals (a failure even if it works)

- Any bulk action, formula or script that changes files without showing the plan first.
- Data loss: bulk delete goes to Trash by default; renames skip existing names, never overwrite.
- A script that hangs the window (infinite loop): scripts run off the UI thread with a step limit
  and a Stop button.
- The UI freezing on a slow disk (the user's NTFS `/mnt/hdd`): listing, rule matching and formula
  evaluation run off the UI thread or through async KIO.
- Turning "Use Win12 Explorer" off leaves the user's old default file manager in place.

## Constraints

- C++17+, Qt 6, KDE Frameworks 6 (KIO, KIOFileWidgets, KFileMetaData, Baloo optional),
  CMake + extra-cmake-modules. All present on the user's Arch system.
- Win12 is a plasmoid (`package/`). Settings keys go in `package/contents/config/main.xml` and
  `tests/settings-keys.txt` (checked by `tests/test_settings_keys.js`). Never act before
  `configReady`; respect the `isMirror` guard; save/restore any system setting Win12 changes, like
  `syncWindowDeco`.
- "Build fixes in": `install.sh` builds Win12 Explorer and the overlay plugin as an Arch package and
  installs it with sudo, as `kde/build.sh` already does for the KDE patches (the overlay plugin must
  sit on Qt's system plugin path; the user allows sudo installs). It installs missing build tools
  itself. (Builder-check call.)
- Translations through the existing `po/` flow.

## Decided how

- Source in a new top-level `explorer/` folder: one app (`win12-explorer`) + the tag overlay plugin.
- Ribbon layout is data (one table of tabs → groups → buttons → action names), like RibbonFM's
  `ribbon_spec.py`, so adding a button is one line.
- Formula and script engines are our own small parser/interpreter in C++; never pass user text to a
  shell or a host-language `eval`.
- Each phase leaves runnable checks: formula/script engine unit tests (parse, evaluate, plan),
  rename planner tests (numbering, conflicts), tag string encode/decode and the 3-tag limit.

## Left to the builder

- Widget-level ribbon drawing (Qt Widgets custom painting is fine) and exact sizes/colours, matching
  the screenshot and mockups.
- View internals (QListView/QTreeView with custom delegates, or a custom item view), as long as big
  folders stay smooth. Dolphin's `src/kitemviews/` (KFileItemModel, roles updater) is a good
  reference for visible-first previews and grouping.
- Names for the 48 palette colours used in `<ColourName>/<Word>` (must be unique and readable).
- Script step limit, file layout inside `explorer/`, test framework (QtTest or plain asserts).

# Win12 Explorer Phase 2 (Tags) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** files and folders get up to three tags (one word + one of the 48 Windows accent colours), drawn as arrow ribbons in Win12 Explorer (aligned slots, Tags column, group by tag name or colour with a file in every matching group), stored in the freedesktop `user.xdg.tags` attribute as `<ColourName>/<Word>` so Dolphin, Baloo and `tags:/` see them, shown in Dolphin as stacked flags on the icon corner through a `KOverlayIconPlugin` shipped in the same Arch package, filtered from a tag list in the nav pane (Baloo where indexed, a walk off the UI thread elsewhere), and offered as a rule field / test / action in phase 3's Select by rule.

**Architecture:** A small static library `win12tags` (`explorer/src/tags/`, Qt Gui + KFileMetaData only, no Widgets) holds the pure parts: `Tag` (colour + word, encode/decode, 48 colour names), `TagRegistry` (`$XDG_DATA_HOME/win12/tags.json`: known tags, last-used order), `TagStore` (xattr read/write through `KFileMetaData::UserMetaData`, 3-tag limit, foreign tags preserved), `Emblem` (one SVG per colour combination into the user's hicolor icon folder) and `TagRibbon` (the arrow shape painter). Both `win12explorer` and the overlay plugin (`explorer/overlay/`, `kcoreaddons_add_plugin`, namespace `kf6/overlayicon`) link it. In the app: `TagsColumns` fills the Tags column from `TagStore`, `FolderDelegate` paints ribbons, a new `TagGroupProxy` between `FolderModel` and `TagsColumns` duplicates rows for tag groups, `TagSearch` (Baloo `QueryRunnable` or a `QtConcurrent` walker) feeds a new flat `ResultsModel` that `FolderView::showResults` swaps under `FolderModel`, and `ExplorerActions` gets the Tags ribbon group, the right-click ▸ Tags menu and the dialogs (`NewTagDialog`, `ManageTagsDialog`).

**Tech Stack:** C++20, Qt 6.12 (Widgets, Svg not needed: SVG is written as text, Concurrent), KF6 6.31 KIO, KFileMetaData, Baloo (required: the package targets Arch only), KCoreAddons (plugin macro), QtTest offscreen. New PKGBUILD depends: `baloo` (`kfilemetadata` is already listed).

**Spec:** `docs/superpowers/specs/2026-10-23-win12-explorer-design.md` (Phase 2, Constraints, Decided how, Left to the builder). Mockup: `docs/superpowers/specs/2026-10-23-win12-explorer-mockups/tags-look-v2.html` (ribbon: 18 px tall, white 600-weight 11 px text, padding 8 px left / 14 px right, arrow point 8 px; slot width = longest tag shown; Dolphin flags: 12x5 px arrows stacked at the icon's bottom-right, at most 3). Phase 3 conventions: `docs/superpowers/plans/2026-10-23-win12-explorer-phase3.md`; deferred items: `docs/superpowers/plans/2026-10-23-win12-explorer-phase3-followups.md`.

## Global Constraints

- Sources in `explorer/src/*.cpp` are globbed into `win12explorer`; `explorer/src/tags/*.cpp` are globbed into the new `win12tags` (Task 1). Each new test is one `explorer_add_test(<name>)` line in `explorer/CMakeLists.txt`; test files live in `explorer/autotests/`.
- Build and test only in a scratch dir, never `install.sh`, never sudo. Use **your own session's scratchpad** (the `Scratchpad directory` in your environment notes), e.g.
  `B=/tmp/claude-1000/-home-kalaminator-Documents-Start12/<session-id>/scratchpad/build; cmake -S explorer -B $B -DCMAKE_BUILD_TYPE=Debug >/dev/null && cmake --build $B -j$(nproc) 2>&1 | tail -5 && ctest --test-dir $B --output-on-failure 2>&1 | tail -15`
  One test only: `ctest --test-dir $B -R tag_test --output-on-failure 2>&1 | tail -20`.
- **Test lock:** there is none. Every test runs offscreen in its own process on its own `QTemporaryDir`; the only shared thing is the build dir, and each agent has its own. Tests that touch the registry call `QStandardPaths::setTestModeEnabled(true)` in `initTestCase` so `tags.json` goes to `~/.qttest/share/win12/`, never to the user's real file. Tests that write xattrs `QSKIP` when `UserMetaData::isSupported()` is false (tmpfs supports `user.*` xattrs since kernel 6.6; this machine runs 7.2).
- Anti-goals: never write a tag the file system refuses silently (every `TagStore` error surfaces as a message); never block the UI thread on a folder walk or a Baloo query; never touch tags Win12 did not write (foreign Dolphin tags are kept and shown grey); the overlay plugin never writes anything but SVG files under the user's icon folder.
- Rulings (given): Dolphin stays installed; flags in Dolphin come only from the overlay plugin; tags live in `user.xdg.tags`; the exact storage string is `<ColourName>/<Word>`; a colour alone is allowed; max 3 tags.
- Rulings made by this plan: see "Calls made" at the end.
- Commit prefix `explorer:`. One fresh agent per task, ≤ ~100k tokens / ~40 tool calls; build and test output always `| tail`; no agent leaves background jobs. Model per task is marked `[sonnet]` or `[opus]`.

## Review Focus

1. `Tag::decode` on strings Win12 never wrote (`"holiday"`, `"Red/"`, `"Red/a/b"`, `"red/Xiaomi"`) must yield a foreign tag or a lower-case-tolerant colour match, never throw or drop the string — `TagTest::decodeForeignAndOddStrings` (Task 1).
2. `TagStore::add` on a file with a foreign tag and 3 Win12 tags must refuse the 4th Win12 tag and keep the foreign one — `TagStoreTest::limitCountsWin12TagsOnly` (Task 3).
3. A file with two tags grouped by tag name appears in both groups and selecting either duplicate selects one item once — `TagGroupProxyTest::twoTagsTwoRows`, `FolderViewTest::selectedItemsDedupeDuplicates` (Task 8).
4. The walker search must be cancellable and keep the window repainting on `/mnt/hdd` — `TagSearchTest::stopEndsWithoutFinished` (Task 10); results arrive in batches, never one signal per file.
5. The overlay plugin must return a name whose SVG already exists on disk when it returns (no icon-cache miss race), and an empty list when flags are off — `OverlayPluginTest::emblemExistsBeforeNameIsReturned`, `flagsOffReturnsNothing` (Task 5).

## File Structure

- Create `explorer/src/tags/tag.h/.cpp` — `Tag`, `TagColours`, encode/decode, `MaxTags = 3`.
- Create `explorer/src/tags/tagregistry.h/.cpp` — `TagRegistry` (tags.json).
- Create `explorer/src/tags/tagstore.h/.cpp` — `TagStore` (xattr, limit, foreign tags, errors).
- Create `explorer/src/tags/emblem.h/.cpp` — `Emblem::iconName(colours)` writes the SVG, returns the name.
- Create `explorer/src/tags/tagribbon.h/.cpp` — `TagRibbon::width/paint/flagsPaint`.
- Create `explorer/overlay/win12tagsoverlay.h/.cpp`, `explorer/overlay/CMakeLists.txt` — the `KOverlayIconPlugin`.
- Create `explorer/src/taggroupproxy.h/.cpp` — row-duplicating proxy for tag groups.
- Create `explorer/src/tagsearch.h/.cpp` — Baloo / walker search, off the UI thread.
- Create `explorer/src/resultsmodel.h/.cpp` — flat `KFileItem` table with KDirModel's columns and roles.
- Create `explorer/src/taglist.h/.cpp` — nav pane tag list + all/any switch.
- Create `explorer/src/newtagdialog.h/.cpp`, `explorer/src/managetagsdialog.h/.cpp`.
- Modify `explorer/src/viewsettings.h/.cpp`, `sortgroup.h/.cpp`, `foldermodel.h/.cpp`, `folderview.h/.cpp`, `ribbonspec.h/.cpp`, `exploreractions.h/.cpp`, `explorerwindow.h/.cpp`, `rulematcher.h/.cpp`, `selectbyruledialog.h/.cpp`.
- Modify `explorer/CMakeLists.txt`, `explorer/PKGBUILD`, `tests/test_explorer_pkgbuild.sh`; `package/contents/config/main.xml`, `tests/settings-keys.txt`, `package/contents/ui/main.qml`, the config page that holds the `explorerDefault` switch (grep `cfg_explorerDefault` in `package/contents/ui/config/`).
- Tests: `explorer/autotests/tag_test.cpp`, `tagregistry_test.cpp`, `tagstore_test.cpp`, `emblem_test.cpp`, `overlayplugin_test.cpp`, `tagribbon_test.cpp`, `taggroupproxy_test.cpp`, `tagsearch_test.cpp`, `resultsmodel_test.cpp`, `taglist_test.cpp`, `managetagsdialog_test.cpp`; extend `sortgroup_test`, `viewsettings_test`, `foldermodel_test`, `folderview_test`, `ribbonspec_test`, `ribbonbar_test`, `exploreractions_test`, `explorerwindow_test`, `rulematcher_test`, `selectbyruledialog_test`.

---

### Task 1 [sonnet]: `Tag` value type, colour names, encode/decode, 3-tag limit (pure)

**Files:**
- Create: `explorer/src/tags/tag.h`, `explorer/src/tags/tag.cpp`
- Modify: `explorer/CMakeLists.txt` — before `add_library(win12explorer ...)`: `find_package(KF6 6.0 REQUIRED ... FileMetaData Baloo CoreAddons)` (append to the existing line), `file(GLOB TAG_SRCS CONFIGURE_DEPENDS src/tags/*.cpp)`, `add_library(win12tags STATIC ${TAG_SRCS})`, `set_property(TARGET win12tags PROPERTY POSITION_INDEPENDENT_CODE ON)`, `target_include_directories(win12tags PUBLIC src/tags)`, `target_link_libraries(win12tags PUBLIC Qt6::Gui KF6::FileMetaData KF6::ConfigCore KF6::I18n)`, `target_compile_definitions(win12tags PUBLIC TRANSLATION_DOMAIN="win12-explorer")`; `win12explorer` links `win12tags`.
- Test: `explorer/autotests/tag_test.cpp`; `explorer_add_test(tag_test)`

**Interfaces (produces):**
```cpp
namespace TagColours {
constexpr int Count = 48;
QString name(int index);            // "YellowGold", "Gold", ... (table below), index 0..47
QColor colour(int index);           // windowsColours order from ConfigAppearance.qml
int indexOfName(const QString &);   // case-insensitive; -1 when unknown
int indexOfColour(const QColor &);  // -1 when not one of the 48
}
struct Tag {
    int colour = -1;        // index into TagColours; -1 = foreign (not written by Win12)
    QString word;           // may be empty for a colour-only tag; the raw string for foreign tags
    bool isForeign() const { return colour < 0; }
    QString encode() const; // "Red/Xiaomi"; colour-only → "Red"; foreign → word unchanged
    static Tag decode(const QString &);
    QString display() const; // word, or TagColours::name(colour) for colour-only, or the raw foreign text
    QColor paintColour() const; // TagColours::colour, foreign → TagColours::colour(indexOfName("Overcast"))
    bool operator==(const Tag &) const = default;
};
constexpr int MaxTags = 3;
QList<Tag> decodeAll(const QStringList &);             // order kept
QStringList encodeAll(const QList<Tag> &);
int win12TagCount(const QList<Tag> &);                   // non-foreign only
```
Colour names, palette order (rows of 8, Windows 10 "Choose your accent colour" names, CamelCase so `tags:/` paths have no spaces): YellowGold, Gold, OrangeBright, OrangeDark, Rust, PaleRust, BrickRed, ModRed · PaleRed, Red, RoseBright, Rose, PlumLight, Plum, OrchidLight, Orchid · Blue, NavyBlue, PurpleShadow, PurpleShadowDark, IrisPastel, IrisSpring, VioletRedLight, VioletRed · CoolBlueBright, CoolBlue, Seafoam, SeafoamTeal, MintLight, MintDark, TurfGreen, SportGreen · Gray, GrayBrown, SteelBlue, MetalBlue, PaleMoss, Moss, MeadowGreen, Green · Overcast, Storm, BlueGray, GrayDark, LiddyGreen, Sage, CamouflageDesert, Camouflage. Hex values are the 48 of `windowsColours` in `package/contents/ui/config/ConfigAppearance.qml:69` in the same order (copy them; a test pins the first and last).
Decode rules: split at the **first** `/`; the part before must be a known colour name (case-insensitive) → `{index, rest}` (rest may contain further `/`); a whole string equal to a colour name → colour-only; anything else → foreign with `word = raw`. `encode` of a word containing `/` keeps it (round-trips through the first-slash rule). Words are trimmed on encode; empty word with a colour → colour-only.

- [ ] **Step 1: Tests**: `fortyEightUniqueNames` (set size 48, none contains `/` or space), `firstAndLastHex` (`colour(0) == #ffb900`, `colour(47) == #7e735f`, `name(16) == "Blue"`), `encodeDecodeRoundTrip` (every colour × word "Xiaomi"), `colourOnlyEncodesAsName`, `decodeIsCaseInsensitiveOnColour` (`"red/Xiaomi"` → Red), `decodeForeignAndOddStrings` (`"holiday"` foreign; `"Red/"` → Red colour-only; `"Red/a/b"` → Red word `a/b`; `""` foreign empty), `sameWordOtherColourDiffers`, `win12TagCountIgnoresForeign`.
- [ ] **Step 2: Run; expect** compile failure (header missing).
- [ ] **Step 3: Implement** `tag.cpp` with one `static const` table `{name, hex}[48]`.
- [ ] **Step 4: Build + ctest; expect** `tag_test` PASS, all older tests still PASS (the CMake change touches every target).
- [ ] **Step 5: Commit** `explorer: Tag type with the 48 Windows colour names, <Colour>/<Word> encode/decode (win12tags lib)`.

### Task 2 [sonnet]: `TagRegistry` (tags.json)

**Files:**
- Create: `explorer/src/tags/tagregistry.h`, `explorer/src/tags/tagregistry.cpp`
- Test: `explorer/autotests/tagregistry_test.cpp`; `explorer_add_test(tagregistry_test)`

**Interfaces (produces):**
```cpp
class TagRegistry : public QObject {
public:
    explicit TagRegistry(QObject *parent = nullptr);                      // defaultPath()
    explicit TagRegistry(const QString &file, QObject *parent = nullptr);  // tests
    static QString defaultPath(); // QStandardPaths::writableLocation(GenericDataLocation) + "/win12/tags.json"
    QList<Tag> tags() const;          // most recently used first, Win12 tags only
    void touch(const Tag &);          // insert or move to front, save
    void rename(const Tag &from, const QString &word);  // registry only (ManageTagsDialog does the files)
    void recolour(const Tag &from, int colour);
    void remove(const Tag &);
    bool contains(const Tag &) const;
Q_SIGNALS:
    void changed();
};
```
File format: `{"version":1,"tags":[{"colour":"Red","word":"Xiaomi","used":"2026-10-24T10:00:00Z"}, ...]}`; written with `QSaveFile`, folder created with `QDir::mkpath`; a missing or unreadable file = empty registry; unknown colour names on load are dropped. `// ponytail: whole file rewritten on each change; the list is tens of entries`.

- [ ] **Step 1: Tests** (`QTemporaryDir`, `TagRegistry r(dir + "/tags.json")`): `emptyWhenMissing`, `touchSavesAndReloads` (second instance on the same path sees it), `touchMovesToFront`, `renameRecolourRemove`, `unknownColourDropped` (hand-written JSON with `"colour":"Mauve"`), `changedEmittedOnce` per mutation (`QSignalSpy`).
- [ ] **Step 2: Run; expect** compile failure.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Build + ctest; expect** `tagregistry_test` PASS.
- [ ] **Step 5: Commit** `explorer: TagRegistry keeps the known tags in $XDG_DATA_HOME/win12/tags.json`.

### Task 3 [sonnet]: `TagStore` (xattr read/write, 3-tag limit, foreign tags, errors)

**Files:**
- Create: `explorer/src/tags/tagstore.h`, `explorer/src/tags/tagstore.cpp`
- Test: `explorer/autotests/tagstore_test.cpp`; `explorer_add_test(tagstore_test)`

**Interfaces (produces):**
```cpp
class TagStore : public QObject {
public:
    enum class Error { None, NotLocal, Unsupported, TooMany, WriteFailed };
    static QList<Tag> read(const QUrl &);               // empty for non-local; decodeAll(UserMetaData(path).tags())
    static bool supported(const QUrl &);                // local && UserMetaData::isSupported()
    Error add(const QList<QUrl> &, const Tag &);        // stops at the first error, items before it are tagged
    Error remove(const QList<QUrl> &, const Tag &);
    Error replace(const QUrl &, const Tag &from, const Tag &to); // Manage tags (rename/recolour)
    static QString errorText(Error);                    // i18n: "Tags are not supported on this drive" etc.
Q_SIGNALS:
    void tagsChanged(const QList<QUrl> &);              // after every successful write
};
```
`add` refuses with `TooMany` when `win12TagCount(read(url)) >= MaxTags` and the tag is not already present; adding an already-present tag is a no-op success; foreign tags are kept in the written list in their position. The emitted `tagsChanged` carries only the urls actually written.

- [ ] **Step 1: Tests** (QTemporaryDir; `initTestCase` QSKIPs the whole class when `!TagStore::supported(dirUrl)`): `addReadRoundTrip`, `addTwiceIsNoop`, `limitCountsWin12TagsOnly` (set `user.xdg.tags` to `{"holiday", "Red/a", "Blue/b", "Green/c"}` through `UserMetaData` directly; `add(Gold/d)` → `TooMany`; `read` still 4 entries), `removeKeepsOthersAndForeign`, `replaceRenames`, `nonLocalIsNotLocal` (`QUrl("sftp://h/x")`), `tagsChangedListsWrittenUrlsOnly` (two files, second made read-only with `QFile::setPermissions` → `WriteFailed`, signal has one url; QSKIP when running as root).
- [ ] **Step 2: Run; expect** compile failure.
- [ ] **Step 3: Implement** (`KFileMetaData::UserMetaData md(path); md.setTags(encodeAll(list))`, map `UserMetaData::Error` → `WriteFailed`).
- [ ] **Step 4: Build + ctest; expect** `tagstore_test` PASS.
- [ ] **Step 5: Commit** `explorer: TagStore writes user.xdg.tags through KFileMetaData, 3-tag limit, keeps foreign tags`.

### Task 4 [sonnet]: Tag group roles (pure) and settings round trip

**Files:**
- Modify: `explorer/src/viewsettings.h` (`enum class GroupRole { None, Name, Modified, Type, Size, TagName, TagColour }` — appended, saved ints stay valid), `explorer/src/sortgroup.h/.cpp`
- Test: `explorer/autotests/sortgroup_test.cpp`, `explorer/autotests/viewsettings_test.cpp`

**Interfaces (produces):**
```cpp
namespace SortGroup {
QList<GroupInfo> tagGroups(const QList<Tag> &, GroupRole);  // TagName: one per distinct display(), order 0, label = word;
                                                            // TagColour: one per distinct colour, order = colour index, label = colour name;
                                                            // empty list → {{1000, i18n("No tag")}}; foreign tags → one group {999, raw text} (TagName) / Overcast (TagColour)
}
```
`groupInfo(item, TagName/TagColour, now)` returns `tagGroups(TagStore::read(item.url()), role).first()` so the single-group callers keep working; the multi-group behaviour comes from Task 8.

- [ ] **Step 1: Tests**: `tagNameGroupsOnePerWord` (Red/Xiaomi + Blue/Xiaomi → one group "Xiaomi"), `tagColourGroupsOnePerColour` (Red/Xiaomi + Red/Urgent → one "Red"; order = 9), `noTagGroupLast` (order 1000 > any colour), `foreignTagGroup`; `viewsettings_test::groupRoleTagRoundTrip` (save `TagColour`, load equal; old int values unchanged: `int(GroupRole::Size) == 4`).
- [ ] **Step 2: Run; expect** compile failure.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Build + ctest; expect** `sortgroup_test`, `viewsettings_test` PASS.
- [ ] **Step 5: Commit** `explorer: group roles for tag name and tag colour (one file, several groups)`.

### Task 5 [opus]: Emblem SVGs, the Dolphin overlay plugin, packaging

**Files:**
- Create: `explorer/src/tags/emblem.h`, `explorer/src/tags/emblem.cpp`
- Create: `explorer/overlay/CMakeLists.txt`, `explorer/overlay/win12tagsoverlay.h`, `explorer/overlay/win12tagsoverlay.cpp`
- Modify: `explorer/CMakeLists.txt` (`add_subdirectory(overlay)` after the library; the plugin test links the plugin source file directly), `explorer/PKGBUILD` (`depends` += `baloo`), `tests/test_explorer_pkgbuild.sh` (loop += `'depends = baloo' 'depends = kfilemetadata'`), `explorer/build.sh` (no change expected: `tar` already packs the whole folder; the version hash covers `overlay/`)
- Test: `explorer/autotests/emblem_test.cpp`, `explorer/autotests/overlayplugin_test.cpp`; `explorer_add_test(emblem_test)`; the plugin test is `ecm_add_test(autotests/overlayplugin_test.cpp ../overlay/win12tagsoverlay.cpp TEST_NAME overlayplugin_test LINK_LIBRARIES win12tags KF6::KIOCore Qt6::Test)` with the same `QT_QPA_PLATFORM=offscreen` property

**Interfaces (produces):**
```cpp
namespace Emblem {
QString iconName(const QList<int> &colourIndexes);          // "win12-tags-d13438-ff8c00" (sorted by colour index, max 3, empty list → "")
QString iconDir();                                            // QStandardPaths GenericDataLocation + "/icons/hicolor/scalable/emblems"
QString ensure(const QList<int> &colourIndexes);             // writes <iconDir>/<name>.svg if missing, returns name ("" on failure)
QByteArray svg(const QList<int> &colourIndexes);             // 16x16 viewBox, 1-3 arrow flags 12x5 stacked bottom-right (mockup .mf), 1 px gap
}
```
```cpp
class Win12TagsOverlay : public KOverlayIconPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.kde.overlayicon.win12tags")
public:
    QStringList getOverlays(const QUrl &) override; // {} unless local, flags on (win12-explorerrc [Tags] showFlags, default true), and the file has Win12 tags; else {Emblem::ensure(colours)}
};
```
`explorer/overlay/CMakeLists.txt`: `kcoreaddons_add_plugin(win12tagsoverlay SOURCES win12tagsoverlay.cpp INSTALL_NAMESPACE "kf6/overlayicon")`, `target_link_libraries(win12tagsoverlay win12tags KF6::KIOCore)`. The plugin is loaded by Dolphin's `KFileItemModelRolesUpdater` (it iterates `kf6/overlayicon`); the first overlay goes to the bottom-right corner, so one emblem carrying all flags keeps them together. The plugin reads the setting with `KSharedConfig::openConfig("win12-explorerrc")` once per call (cheap; `// ponytail: a KConfigWatcher + overlaysChanged for cached urls if F5 after toggling turns out to annoy`).

- [ ] **Step 1: Tests**: `emblem_test`: `nameIsSortedAndCapped` (`{9,1}` and `{1,9}` → same name; 4 colours → 3 hexes), `svgHasOneRectPerColour`, `ensureWritesOnce` (`QStandardPaths::setTestModeEnabled(true)`; file exists after, mtime unchanged on second call). `overlayplugin_test` (test mode on, QSKIP without xattr support): `emblemExistsBeforeNameIsReturned` (tag a temp file through `UserMetaData`; `getOverlays` returns one name and `QFile::exists(iconDir()/name.svg)`), `untaggedReturnsNothing`, `foreignOnlyReturnsNothing`, `flagsOffReturnsNothing` (write `showFlags=false` to the test-mode rc first), `remoteUrlReturnsNothing`.
- [ ] **Step 2: Run; expect** compile failure.
- [ ] **Step 3: Implement** emblem + plugin + CMake + PKGBUILD + pkgbuild test line.
- [ ] **Step 4: Build + ctest; expect** `emblem_test`, `overlayplugin_test` PASS; `sh tests/test_explorer_pkgbuild.sh` prints OK; `ls $B/bin/kf6/overlayicon/` (or `find $B -name 'win12tagsoverlay.so'`) shows the plugin. **Manual check (report, do not fix in this task if it fails):** `touch /tmp/x; setfattr -n user.xdg.tags -v Red/Xiaomi /tmp/x` is not enough for Dolphin to show the flag without the plugin installed; so note in the handoff that the first `install.sh` run after merge should be followed by opening `/tmp` in Dolphin and confirming the flag at the icon's bottom-right. If hicolor `scalable/emblems` is not picked up by KIconLoader, the fallback is to also write 16/22/32 px PNGs through `QSvgRenderer` under `hicolor/<size>x<size>/emblems` (add `Qt6::Svg` then).
- [ ] **Step 5: Commit** `explorer: tag emblems (SVG per colour set) and the Dolphin overlay-icon plugin, packaged with win12-explorer`.

### Task 6 [sonnet]: `TagRibbon` painter and the Tags column filled from `TagStore`

**Files:**
- Create: `explorer/src/tags/tagribbon.h`, `explorer/src/tags/tagribbon.cpp`
- Modify: `explorer/src/foldermodel.h/.cpp` (`TagsColumns`)
- Test: `explorer/autotests/tagribbon_test.cpp`; `explorer_add_test(tagribbon_test)`; extend `explorer/autotests/foldermodel_test.cpp`

**Interfaces (produces):**
```cpp
namespace TagRibbon {
constexpr int Height = 18, PadLeft = 8, PadRight = 14, Point = 8, Gap = 6;
QFont font(const QFont &base);                                  // base, weight DemiBold, pointSize so pixel height ≈ 11
int width(const Tag &, const QFontMetrics &ribbonFm);           // PadLeft + text + PadRight
void paint(QPainter *, const QRect &slot, const Tag &, const QFontMetrics &ribbonFm); // left-aligned in the slot: polygon (0,0)(w-8,0)(w,h/2)(w-8,h)(0,h), white text
void paintFlags(QPainter *, const QRect &iconRect, const QList<Tag> &);               // Dolphin-style 12x5 stacked flags, bottom-right, ≤ 3
}
```
`TagsColumns`: `extraColumnData` returns `TagsRole` → `QVariant::fromValue(QList<Tag>)` (register with `Q_DECLARE_METATYPE(Tag)`), `Qt::DisplayRole` → the encoded tags joined with `", "` (so plain `CE_ItemViewItem` text, tooltips and header sorting have something), `Qt::ToolTipRole` same. A `QHash<QUrl, QList<Tag>> m_cache` is filled on first request from `TagStore::read` and cleared on `modelReset`/`rowsRemoved`/`dataChanged` of the source; `TagsColumns::invalidateTags(const QList<QUrl> &)` drops entries and emits `dataChanged` for column 7 and column 0 of those rows (the delegate draws ribbons next to names too). `// ponytail: one getxattr per visible row on first paint; move to the PreviewLoader thread if a network mount is slow`.

- [ ] **Step 1: Tests**: `tagribbon_test`: `widthGrowsWithText`, `paintFillsSlotColour` (paint on a 200x18 QImage, pixel at (2,9) equals `paintColour()`, pixel at (slot.right()-1, 1) is still the background → arrow point cut), `paintFlagsThreeMax`. `foldermodel_test`: replace `tagsColumnIsLastAndEmpty` with `tagsColumnReadsXattr` (QSKIP without xattr support: tag `a.txt` Red/Xiaomi via `UserMetaData`; after listing, `t.index(row,7).data(TagsRole).value<QList<Tag>>().first().word == "Xiaomi"`, DisplayRole `"Red/Xiaomi"`) and `invalidateTagsEmitsDataChanged`.
- [ ] **Step 2: Run; expect** compile failure.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Build + ctest; expect** `tagribbon_test`, `foldermodel_test` PASS.
- [ ] **Step 5: Commit** `explorer: TagRibbon painter; the Tags column reads user.xdg.tags`.

### Task 7 [opus]: Ribbons in the view delegate with aligned slots

**Files:**
- Modify: `explorer/src/folderview.h/.cpp` (`FolderDelegate::paint`, `sizeHint`, `paintRich`; `FolderView::tagSlotWidth()`)
- Test: `explorer/autotests/folderview_test.cpp`

**Interfaces (produces):**
```cpp
int FolderView::tagSlotWidth() const; // max TagRibbon::width over all rows' TagsRole, recomputed lazily (dirty flag set on model reset/rowsInserted/dataChanged); 0 when no tags
```
Drawing rules (ruling): **Details**: ribbons only in the Tags column (column 7), one slot of `tagSlotWidth()` each, `Gap` between, `DisplayRole` text suppressed there (`opt.text.clear()` before `CE_ItemViewItem`, then paint ribbons). **List / Small icons / Tiles / Content**: ribbons after the name text in column 0 (name elided to leave `n * (slot + Gap)` room; Tiles/Content on the name line in `paintRich`). **Extra large / Large / Medium icons**: `TagRibbon::paintFlags` on the icon's bottom-right (no room for ribbons; same look as Dolphin). `sizeHint` in Details/List grows to at least `Height + 4` when the row has tags. Selected rows keep the ribbon colours (white text on the colour stays readable on the highlight).

- [ ] **Step 1: Tests** (QSKIP without xattr support; render `view->currentView()->grab()` or paint the delegate into a `QImage` with a hand-made `QStyleOptionViewItem`): `detailsTagsColumnShowsRibbon` (pixel of the tag colour inside column 7's rect, none inside the name column's text area), `listModeRibbonAfterName`, `iconModeFlagsAtCorner`, `slotWidthIsLongestTag` (two files, tags "Xiaomi" and "Go": `tagSlotWidth() == TagRibbon::width(Xiaomi)`; a ribbon for "Go" starts at the same x as "Xiaomi"'s in Details), `slotWidthRecomputedAfterInvalidate`.
- [ ] **Step 2: Run; expect** FAIL.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Build + ctest; expect** `folderview_test` PASS; run `$B/bin/win12-explorer /tmp` once offscreen-free only if a display is available (optional eyeball).
- [ ] **Step 5: Commit** `explorer: tag ribbons in the Tags column and next to names, aligned slots; flags on big icons`.

### Task 8 [opus]: `TagGroupProxy` — a file in every matching group

**Files:**
- Create: `explorer/src/taggroupproxy.h`, `explorer/src/taggroupproxy.cpp`
- Modify: `explorer/src/foldermodel.h/.cpp` (`TagsColumns` source becomes `TagGroupProxy`; `itemFor` maps through it), `explorer/src/folderview.cpp` (chain `FolderModel → TagGroupProxy → TagsColumns`; `applySettings` sets `TagName/TagColour` on the proxy and `GroupRole::None` on `FolderModel`; `selectedItems`/`itemsInViewOrder` dedupe by url), `explorer/src/exploreractions.cpp:283` (group menu gets i18n("Tag name"), i18n("Tag colour"))
- Test: `explorer/autotests/taggroupproxy_test.cpp`; `explorer_add_test(taggroupproxy_test)`; extend `folderview_test.cpp`

**Interfaces (produces):**
```cpp
class TagGroupProxy : public QAbstractProxyModel {
public:
    explicit TagGroupProxy(QObject *parent = nullptr);
    void setGroupRole(GroupRole);           // TagName/TagColour duplicate rows; anything else = identity mapping
    GroupRole groupRole() const;
    // identity: rows == source rows; tag mode: one row per (source row, group) pair, ordered by GroupInfo (groupLess) then source row
    QModelIndex mapToSource(const QModelIndex &) const override; QModelIndex mapFromSource(const QModelIndex &) const override; // mapFromSource → the first duplicate
    QVariant data(const QModelIndex &, int role) const override;   // GroupOrderRole/GroupLabelRole/GroupFirstRole answered here in tag mode, else forwarded
    // rowCount/columnCount/index/parent/sibling as usual; flat model
};
```
Structure changes from the source (`rowsInserted/Removed/Moved`, `modelReset`, `layoutChanged`, and `dataChanged` on column 7) rebuild the row map inside `beginResetModel/endResetModel` in tag mode (`// ponytail: full rebuild; fine-grained row maps when folders with thousands of tagged files feel slow`); in identity mode all signals are forwarded 1:1. Tags per source row come from `TagStore::read` on the item (same cache as `TagsColumns` would be nicer; accept two reads, or move the cache into `TagGroupProxy` and let `TagsColumns` ask it — implementer's choice, state it in the commit).

- [ ] **Step 1: Tests**: `taggroupproxy_test` on a `QStandardItemModel` source whose rows carry `KDirModel::FileItemRole` items of temp files (QSKIP without xattr): `identityWhenNotTagMode`, `twoTagsTwoRows` (file with Red/Xiaomi + Orange/Urgent → 2 rows under TagName, labels "Urgent","Xiaomi" in `groupLess` order; `GroupFirstRole` true on each group's first row), `untaggedInNoTagGroupLast`, `tagColourMergesSameColour`, `sourceInsertRebuilds`. `folderview_test`: `selectedItemsDedupeDuplicates` (group by tag name, `selectAll()`, `selectedItems().size()` == number of files), `groupByTagShowsHeaders` (two `GroupFirstRole` rows).
- [ ] **Step 2: Run; expect** compile failure.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Build + ctest; expect** `taggroupproxy_test`, `folderview_test`, `foldermodel_test`, `exploreractions_test` PASS.
- [ ] **Step 5: Commit** `explorer: TagGroupProxy puts a file in every tag group (name or colour)`.

### Task 9 [sonnet]: Tags ribbon group, Add/Remove tag menus, New tag dialog, right-click ▸ Tags

**Files:**
- Modify: `explorer/src/ribbonspec.h` (`phaseBuilt`: `phase == 1 || phase == 2 || phase == 3`), `explorer/src/ribbonspec.cpp:89` (the Tags group rows), `explorer/src/exploreractions.h/.cpp`
- Create: `explorer/src/newtagdialog.h`, `explorer/src/newtagdialog.cpp`
- Test: `explorer/autotests/ribbonspec_test.cpp`, `ribbonbar_test.cpp`, `exploreractions_test.cpp`

**Interfaces:**
- Spec rows, group i18n("Tags") phase 2: `tags_add` (Big, "Add tag", icon `tag-new`, menu), `tags_remove` (Small, "Remove tag", `tag-delete`, menu), `tags_manage` (Small, "Manage tags…", `tag-properties`), `tags_group_name` (Small, "Group: tag name", `object-group`), `tags_group_colour` (Small, "Group: tag colour", `color-management`), `tags_flags` (Small, "Show tag flags", `flag`, checkable).
- `ribbonbar_test::tagsGroupHiddenInPhase1` becomes `tagsGroupShowsBuiltButtons` (`visibleGroupTitles(2)` contains "Tags").
```cpp
class NewTagDialog : public QDialog { // word QLineEdit + 6x8 swatch grid of TagColours (QToolButton per colour, exclusive), OK disabled until a colour is picked; empty word allowed
public: NewTagDialog(QWidget *parent); Tag tag() const; void setTag(const Tag &); };
// ExplorerActions
void addTag(const KFileItemList &, const Tag &);      // TagStore::add; on error KMessageBox::error(errorText); on success registry.touch(tag), view->model()->invalidateTags(urls)
void removeTag(const KFileItemList &, const Tag &);
void newTag(const KFileItemList &);                   // NewTagDialog → addTag
QMenu *tagsMenu(const KFileItemList &);               // recent tags (registry order, max 8, checkable: checked when every item has it; toggling adds/removes), separator, New tag…, Manage tags…; the Tags menu of items on an unsupported drive has one disabled entry errorText(Unsupported)
TagRegistry &registry();
void setExecNewTagDialog(std::function<bool(NewTagDialog &)>); // test seam, like setExecRenameDialog
```
Wiring: `tags_add` menu = `tagsMenu(selection())` rebuilt on `aboutToShow`; `tags_remove` menu = one entry per tag present on any selected item + i18n("All tags"); `tags_group_name/colour` → `groupRequested(GroupRole::TagName/TagColour)` (checkable, synced in `syncViewState`); `tags_flags` reads/writes `win12-explorerrc [Tags] showFlags` (checked state from the config). Context menu: inside `contextMenu` for item menus, `menu->addMenu(tagsMenu(items))` right before `addWin12()`. Ribbon icons in menus: `QIcon(QPixmap)` rendered with `TagRibbon::paint` so the menu shows the ribbon itself.

- [ ] **Step 1: Tests**: `ribbonspec_test::tagsGroupHasPhase2Buttons` (six ids, `tags_add` is Big + menu); `ribbonbar_test` as above; `exploreractions_test` (QSKIP without xattr; test mode on): `addTagWritesXattrAndRegistry`, `fourthTagShowsError` (seam: count `KMessageBox` via `activeModalWidget()` like the delete-forever test, or inject a `std::function<void(QString)> m_showError` seam — pick the seam), `tagsMenuListsRecentFirstAndChecks`, `contextMenuHasTagsSubmenu`, `removeTagClearsIt`, `newTagDialogSeamAddsTag`.
- [ ] **Step 2: Run; expect** compile failures.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Build + ctest; expect** all PASS.
- [ ] **Step 5: Commit** `explorer: Tags ribbon group, Add/Remove tag menus, New tag dialog, right-click Tags menu`.

### Task 10 [opus]: `TagSearch` — Baloo where indexed, a walker off the UI thread elsewhere

**Files:**
- Create: `explorer/src/tagsearch.h`, `explorer/src/tagsearch.cpp`
- Modify: `explorer/CMakeLists.txt` (`win12explorer` links `KF6::Baloo`, `Qt6::Concurrent`; add `Concurrent` to `find_package(Qt6 ...)`)
- Test: `explorer/autotests/tagsearch_test.cpp`; `explorer_add_test(tagsearch_test)`

**Interfaces (produces):**
```cpp
class TagSearch : public QObject {
public:
    enum class Mode { All, Any };
    enum class Engine { Baloo, Walk };
    explicit TagSearch(QObject *parent = nullptr);
    static Engine engineFor(const QUrl &folder);   // Baloo iff local && IndexerConfig().fileIndexingEnabled() && shouldBeIndexed(path); else Walk
    void start(const QUrl &folder, const QList<Tag> &tags, Mode, Engine = engineFor(folder)); // stops a running search first
    void stop();                                     // sets the cancel flag; no finished() afterwards
    bool isRunning() const;
Q_SIGNALS:
    void found(const KFileItemList &);               // batches of ≤ 200, queued from the worker
    void finished(bool ok, const QString &error, Engine used);
};
```
Baloo: `Baloo::Query q; q.setSearchString(tags joined: "tag:\"Red/Xiaomi\" AND tag:\"Orange/Urgent\"" (Mode::All) or OR); q.setIncludeFolder(path); auto *r = new Baloo::QueryRunnable(q); connect(r, &QueryRunnable::queryResult, …); connect finished; QThreadPool::globalInstance()->start(r)`; results become `KFileItem(QUrl::fromLocalFile(path))` (delayed mime types). Walk: `QtConcurrent::run` with `QDirIterator(path, QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot, QDirIterator::Subdirectories)`, `TagStore::read` per entry, compare encoded strings, batch, check `std::atomic_bool m_cancel` per entry; results delivered through a queued `QMetaObject::invokeMethod` into the object's thread. `// ponytail: the walker re-reads xattrs on every search; a result cache if the user filters the same USB stick repeatedly`.

- [ ] **Step 1: Tests** (QSKIP without xattr; temp tree `a.txt` Red/Xiaomi, `sub/b.txt` Red/Xiaomi + Orange/Urgent, `c.txt` untagged; always `Engine::Walk` so the test does not depend on Baloo's index): `walkAnyFindsBoth`, `walkAllFindsNested` (All of the two tags → `sub/b.txt` only), `batchesNotPerFile` (300 tagged files → `found` emitted ≤ 3 times), `stopEndsWithoutFinished` (start, `stop()` immediately, `isRunning()` false and no `finished` in 300 ms), `missingFolderFinishesWithError`, `engineForTmpIsWalk` (`/tmp` is excluded from Baloo by default; if this machine indexes it, QSKIP).
- [ ] **Step 2: Run; expect** compile failure.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Build + ctest; expect** `tagsearch_test` PASS.
- [ ] **Step 5: Commit** `explorer: TagSearch — Baloo query where indexed, cancellable folder walk off the UI thread elsewhere`.

### Task 11 [opus]: `ResultsModel` and `FolderView::showResults`

**Files:**
- Create: `explorer/src/resultsmodel.h`, `explorer/src/resultsmodel.cpp`
- Modify: `explorer/src/foldermodel.h/.cpp` (`FolderModel::setResultsSource(ResultsModel *)` / `clearResultsSource()`; `itemFor` and the group cache read `KDirModel::FileItemRole` from `sourceModel()` instead of `m_dirModel->itemForIndex`), `explorer/src/folderview.h/.cpp`
- Test: `explorer/autotests/resultsmodel_test.cpp`; `explorer_add_test(resultsmodel_test)`; extend `folderview_test.cpp`

**Interfaces (produces):**
```cpp
class ResultsModel : public QAbstractTableModel { // columns = KDirModel::ColumnCount, same headers/roles as KDirModel (DisplayRole per column, DecorationRole col 0, FileItemRole), rows have a stable internalPointer (heap KFileItem) for FolderModel's group cache
public:
    void clear(); void append(const KFileItemList &); KFileItem itemAt(int row) const; int count() const;
};
// FolderView
void showResults(const QString &title, TagSearch *search);  // takes ownership of the finished results: clears, appends on found(), status text "<n> items" while running; url() stays the folder being searched; urlChanged not emitted
void clearResults();                                         // back to the folder listing (openUrl(url()))
bool showingResults() const;
Q_SIGNALS: void resultsTitleChanged(const QString &);       // "" when cleared; ExplorerWindow shows it in the address row as a label
```
`openUrl` clears results first. Delegates, sorting, grouping, selection and context menus keep working because the chain above `FolderModel` is unchanged; `KDirSortFilterProxyModel::lessThan` reads `FileItemRole` from the source, which `ResultsModel` provides. `// ponytail: results are not watched for changes; a rename inside results refreshes nothing until the search reruns`.

- [ ] **Step 1: Tests**: `resultsmodel_test`: `columnsMatchKDirModel` (`columnCount() == KDirModel::ColumnCount`, header(0) == "Name"), `appendEmitsRowsInserted`, `fileItemRoleRoundTrip`. `folderview_test`: `showResultsListsFoundItems` (fake `TagSearch` via `Engine::Walk` on a temp tree; `rowCount` after `finished` equals matches, `url()` unchanged), `openUrlClearsResults`, `sortByNameWorksOnResults`.
- [ ] **Step 2: Run; expect** compile failure.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Build + ctest; expect** `resultsmodel_test`, `folderview_test`, `foldermodel_test`, `exploreractions_test`, `selectbyruledialog_test` PASS (the last two use `itemFor`).
- [ ] **Step 5: Commit** `explorer: ResultsModel — a flat result list under FolderModel; FolderView::showResults`.

### Task 12 [sonnet]: Nav pane tag list with Ctrl+click and the all/any switch

**Files:**
- Create: `explorer/src/taglist.h`, `explorer/src/taglist.cpp`
- Modify: `explorer/src/explorerwindow.h/.cpp:240` (left side of the splitter becomes a `QWidget` with `QVBoxLayout{m_places, m_tagList}`; `m_tagList` hidden when the registry is empty)
- Test: `explorer/autotests/taglist_test.cpp`; `explorer_add_test(taglist_test)`; extend `explorerwindow_test.cpp`

**Interfaces (produces):**
```cpp
class TagList : public QWidget { // header i18n("Tags (Ctrl+click = more)"), QListWidget of registry tags with ribbon icons, two exclusive QToolButtons i18n("all picked tags") / i18n("any picked tag")
public:
    explicit TagList(TagRegistry *, QWidget *parent = nullptr);
    QList<Tag> picked() const; TagSearch::Mode mode() const; void clearPicks();
Q_SIGNALS:
    void filterChanged(const QList<Tag> &picked, TagSearch::Mode); // after click, Ctrl+click, mode toggle; empty picked = cleared
};
```
`ExplorerWindow`: on `filterChanged` with picks → `activeView()->showResults(i18n("Tagged: %1", words), new TagSearch)` started on `activeView()->url()` (the folder currently shown; results include subfolders); empty picks → `clearResults()`. Navigating (`urlChanged`, a place click, address bar) calls `m_tagList->clearPicks()`. The address row shows `resultsTitleChanged` text in a `QLabel` after the navigator (hidden when empty). Plain click = that tag only; Ctrl+click toggles (QListWidget `ExtendedSelection` gives this for free). The status bar text comes from the view's counts as usual.

- [ ] **Step 1: Tests**: `taglist_test`: `listsRegistryTags`, `clickPicksOne`, `ctrlClickPicksMore` (`QTest::mouseClick` with `Qt::ControlModifier`), `modeToggleEmits`, `clearPicksEmitsEmpty`. `explorerwindow_test`: `tagPickShowsResultsAndNavigationClears` (QSKIP without xattr; temp tree, registry in test mode with the tag; pick → `activeView()->showingResults()`; `openTab(other)` → not).
- [ ] **Step 2: Run; expect** compile failure.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Build + ctest; expect** `taglist_test`, `explorerwindow_test` PASS.
- [ ] **Step 5: Commit** `explorer: nav pane tag list — click filters, Ctrl+click picks more, all/any switch`.

### Task 13 [sonnet]: Manage tags… (rename, recolour, delete everywhere)

**Files:**
- Create: `explorer/src/managetagsdialog.h`, `explorer/src/managetagsdialog.cpp`
- Modify: `explorer/src/exploreractions.cpp` (`tags_manage` and the menu entries open it)
- Test: `explorer/autotests/managetagsdialog_test.cpp`; `explorer_add_test(managetagsdialog_test)`

**Interfaces (produces):**
```cpp
class ManageTagsDialog : public QDialog { // list of registry tags (ribbon icons); buttons Rename…, Recolour…, Delete everywhere; a line below the list: i18n("Changes apply to indexed locations and to %1 (with subfolders)", folder)
public:
    ManageTagsDialog(TagRegistry *, TagStore *, const QUrl &currentFolder, QWidget *parent);
    void setExecNewTagDialog(std::function<bool(NewTagDialog &)>);  // Rename/Recolour reuse NewTagDialog preset with the tag
    void applyTo(const Tag &from, const std::optional<Tag> &to);  // runs TagSearch(Baloo on "/" when indexing is on) + TagSearch(Walk on currentFolder); per result TagStore::replace or remove; then registry.rename/recolour/remove; progress in the dialog's status label; disabled buttons while running
Q_SIGNALS: void applied(int filesChanged);
};
```
Ruling: "everywhere" = what Baloo indexes plus a walk of the current folder tree; the dialog says so (spec leaves the scope open; a whole-disk walk is not acceptable on `/mnt/hdd`).

- [ ] **Step 1: Tests** (QSKIP without xattr; test mode; temp tree with 2 tagged files; the dialog gets a seam `setEngineForTests(TagSearch::Engine::Walk)` so Baloo is not consulted): `renameRewritesFilesAndRegistry`, `recolourKeepsWord`, `deleteEverywhereRemovesTagKeepsOthers`, `buttonsDisabledWhileRunning`.
- [ ] **Step 2: Run; expect** compile failure.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Build + ctest; expect** `managetagsdialog_test`, `exploreractions_test` PASS.
- [ ] **Step 5: Commit** `explorer: Manage tags — rename, recolour, delete a tag everywhere it is indexed or under the current folder`.

### Task 14 [sonnet]: Plasmoid setting "Show tag flags on icons"

**Files:**
- Modify: `package/contents/config/main.xml` (after `explorerDefault`: `<entry name="explorerTagFlags" type="Bool"><label>Show tag flags on icons in Dolphin and Win12 Explorer (writes win12-explorerrc [Tags] showFlags)</label><default>true</default></entry>`), `tests/settings-keys.txt` (+ `explorerTagFlags`), `package/contents/ui/main.qml` (next to `explorerDefaultRunner`: on `cfg.explorerTagFlags` change, and only after `configReady` and not `isMirror`, run `kwriteconfig6 --file win12-explorerrc --group Tags --key showFlags true|false`), the config page with the `cfg_explorerDefault` switch (a second switch under it, disabled with the same "not built" hint when Win12 Explorer is not installed)
- Test: `node tests/test_settings_keys.js` (already checks the key list); extend whichever QML/JS test covers the explorer switch if one exists (grep `explorerDefault` in `tests/`)

- [ ] **Step 1: Tests**: add the key to `tests/settings-keys.txt`; run `node tests/test_settings_keys.js` → PASS only after main.xml has the entry.
- [ ] **Step 2: Implement** the three QML/XML edits.
- [ ] **Step 3: Run** `node tests/test_settings_keys.js` and the existing settings tests listed in `install.sh` lines 1-10 (the `node tests/...` lines); expect PASS.
- [ ] **Step 4: Commit** `win12: "Show tag flags on icons" switch writes the Explorer tags setting`.

### Task 15 [sonnet]: Phase 3 follow-ups — tag rule field, "has tag" test, "Add tag" action

**Files:**
- Modify: `explorer/src/rulematcher.h/.cpp` (`Field::Tag`, `Test::HasTag`; `testsFor(Field::Tag) == {HasTag}`; `fieldText` i18n("Tag"), `testText` i18n("has tag"); `matches`: `TagStore::read(item.url())` contains a tag whose `display()` equals the value case-insensitively **or** whose `encode()` equals it; empty value → compile error like the text tests), `explorer/src/selectbyruledialog.h/.cpp` (`Action::AddTag`; a ⋯ menu entry i18n("Add tag…") → `act(Action::AddTag)`; the field combo lists Tag), `explorer/src/exploreractions.cpp` (`selectByRule`: `Action::AddTag` → `newTag(ticked)`), `explorer/src/ribbonspec.cpp` (Advanced ▸ Bulk actions: `adv_tag_matches` ("Tag matches…", icon `tag-new`) → `selectByRule(Action::AddTag)`)
- Test: `explorer/autotests/rulematcher_test.cpp`, `selectbyruledialog_test.cpp`, `exploreractions_test.cpp`, `ribbonspec_test.cpp`

- [ ] **Step 1: Tests**: `rulematcher_test::hasTagByWordOrEncoded` (QSKIP without xattr: `Red/Xiaomi` matches value `"xiaomi"` and `"Red/Xiaomi"`, not `"Blue/Xiaomi"`), `hasTagEmptyValueIsError`; `selectbyruledialog_test::fieldComboOffersTag`, `addTagActionInMenu`; `exploreractions_test::selectByRuleAddTagOpensNewTagDialog` (seam from Task 9); `ribbonspec_test` adds `adv_tag_matches` to the Advanced expectations.
- [ ] **Step 2: Run; expect** compile failures.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Build + ctest; expect** all PASS.
- [ ] **Step 5: Commit** `explorer: Select by rule gets the Tag field, "has tag" test and Add tag action (phase 3 follow-ups)`.

---

## Calls made

1. **Colour names**: the Windows 10 accent names in CamelCase (`YellowGold`, `Blue` for #0078d7 instead of "Default blue"), unique, no spaces so `tags:/Red/Xiaomi` paths stay clean. Colour-only tag encodes as the bare colour name (`Red`).
2. **Foreign tags** (anything in `user.xdg.tags` that does not start with a known colour name): kept untouched on every write, drawn as a grey (Overcast) ribbon with the raw text, do not count toward the 3-tag limit, grouped under their raw text.
3. **The tag gallery** is the `Add tag ▾` menu (recent tags with ribbon icons + New tag… + Manage tags…), not a separate ribbon gallery widget; `RibbonBar` renders buttons only and the menu is the same clicks.
4. **Search results need a flat model** (`ResultsModel`, Task 11): phase 3 deferred it, but the nav pane filter "folder and subfolders" cannot be shown through `KDirModel`. It is kept KDirModel-shaped so phase 4 (search) reuses it. The tag filter searches the folder the active view shows, not the whole disk.
5. **Baloo is required** in CMake and the PKGBUILD (the package targets this Arch system only); the walker still runs where Baloo does not index.
6. **"Delete/rename a tag everywhere"** = Baloo-indexed locations plus the current folder tree; the dialog says so. No whole-disk walk.
7. **Show tag flags** lives in `win12-explorerrc [Tags] showFlags` (read by the plugin inside Dolphin and by the ribbon toggle); the plasmoid key `explorerTagFlags` writes it through `kwriteconfig6`. Dolphin applies a change on its next refresh (F5); no config watcher in the plugin unless it annoys.
8. **Emblems are SVG** in `~/.local/share/icons/hicolor/scalable/emblems/`, written before the name is returned; PNG fallback noted in Task 5 if KIconLoader does not pick them up.
9. **Icon modes** (extra large / large / medium) show Dolphin-style corner flags instead of ribbons; ribbons go next to names in List, Small icons, Tiles, Content and in the Tags column in Details.
10. **Group rebuild** in `TagGroupProxy` is a full reset on any source structure change (selection lost on refresh while grouped by tag; acceptable for now, noted with a ponytail comment).
11. Rule "has tag" matches by word (case-insensitive) or by the full `Colour/Word` string; the Advanced tab gains one button `adv_tag_matches` so the action is reachable without the ⋯ menu.

## Self-review notes

- Spec coverage: tag model + limit (1, 3), colour names (1), storage `user.xdg.tags` + registry (2, 3), ribbons with aligned slots in the Tags column and next to names (6, 7), grouping with duplicates (4, 8), View ▸ Tags group with every listed button (9), right-click ▸ Tags recent + New tag… (9), Manage tags (13), nav pane list + Ctrl+click + all/any + Baloo/walk (10, 11, 12), Dolphin overlay plugin packaged (5), unsupported-drive message (3, 9), plasmoid switch (14), phase 3 follow-ups (15).
- Type consistency: `Tag`, `TagRegistry`, `TagStore`, `TagSearch::Mode/Engine`, `GroupRole::TagName/TagColour`, `TagsColumns::TagsRole` (now `QList<Tag>`), `FolderView::showResults/clearResults/showingResults` are named identically across tasks.
- Order: pure units (1-4) before anything with a widget; the plugin (5) only needs 1 and 3; 6-8 build the view; 9 needs 2, 3, 6; 10-12 the search path; 13 needs 9-10; 14 and 15 are independent of 10-13.

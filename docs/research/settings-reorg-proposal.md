# Win12 settings reorganisation (proposal)

Input: `docs/research/settings-inventory.md` (153 rows on 9 pages), `package/contents/config/config.qml`,
section 10 of `docs/superpowers/specs/2026-10-16-fast-islands-design.md` (two new island settings, the
"3 Islands" layout). Read-only proposal: no code was changed. Config keys are never renamed here; only pages,
order, labels and visibility change.

## 1. Goals

- One place per option. Every key is edited on exactly one page; the four duplicates in the inventory
  (TaskbarLayoutBox, `colorMode`, `accentMode`/`accentColor`, `taskbarColor`/`taskbarAlpha`) are merged.
- Pages follow how a user thinks: pick a style -> the Start menu (shape, apps, button and shortcuts) -> the
  taskbar -> window title bars -> colours and look -> search -> rarely used things.
- Pages are about one object each (the menu, the list, the taskbar, the windows). No page mixes system actions
  with saved settings without saying so; everything that applies at once says "(applies at once)".
- Style-specific options are hidden, not merely tagged "(Windows 7, XP)" in the label, and every page hides
  them with the same rule set (section 4).
- Labels use one voice: a noun with a colon for fields (`Size:`), a sentence for checks, no questions, the
  same noun for the same thing everywhere (one "Corner radius:", one "Placement:", one "Opacity:").

## 2. New page list (in order)

| # | Page | KDE icon (present in Breeze) | Purpose |
|---|---|---|---|
| 1 | Style | `preferences-desktop-theme-global` | Pick the Windows style; which parts follow it (menu, taskbar, title bars); the style's own flavour (7 Classic/Modern lists, XP theme, colour scheme, icons). |
| 2 | Start menu | `view-grid` | Size, position and scale of the menu, the tiles, the Windows 12 extras. |
| 3 | Apps list | `view-list-details` | The list of programs: what it shows, its sizes, the 7/XP All Programs options, hidden apps, filtered folders. |
| 4 | Start button & shortcuts | `start-here-kde` | The Start button look and its right-click menu, the left-hand shortcuts, the power button and power menu. |
| 5 | Taskbar | `preferences-desktop-plasma` | Windows 12 layout and islands (including monitors), taskbar colour, texture and buttons, auto-hide, the taskbar search box, the Plasma panel. |
| 6 | Windows | `preferences-system-windows` | Title bars and frames: opacity, blur, corners, height, picture; the XP Firefox title bar. |
| 7 | Appearance | `preferences-desktop-color` | Colours (mode, accent, Windows 12 shade), transparency and corners, texture, fonts, effects. |
| 8 | Search | `search` | How menu search behaves and which KRunner sources it uses. |
| 9 | Advanced | `configure` | System icon theme and wallpaper (apply at once), backup, restore, reset. |

Nine pages, as today, but each is one topic. The three Start pages sit together (2-4); the user's suggested
flow put "shortcuts" after colours, but the rail, power menu and button are menu content, so they stay with the
menu. The old Layout page is split in two (Start menu / Apps list) because it held 32 rows in three unrelated
blocks; Look and Colours merge into Appearance because they edited the same two keys.

Sizes after the move (interactive rows, TaskbarLayoutBox counted as one row): Style 11, Start menu 23,
Apps list 20, Start button & shortcuts 18, Taskbar 23 + 2 new, Windows 9, Appearance 30, Search 9, Advanced 7.
That is the inventory's 153 rows minus the 4 merged duplicates, plus the 2 new island settings (the inventory
counts the Start Button page as 6 where this list counts its 7 rows, so totals differ by one; the mapping below
is row by row).

## 3. Pages, groups and options

Format: `New label <- Old page / Old label (key)`. "(renamed)" marks a label change; "(moved)" is implied
whenever the old page differs. "MERGED" marks a duplicate resolved. Info rows (hints) are listed where they
survive; dropped hints are listed at the end of each page. Visibility/enabled rules use the properties defined
in section 4.

### 3.1 Style

Heading text: "Style" + one line "A style sets the Start menu, the taskbar and the window title bars together.
Mix them below." (replaces the question "Which style would you prefer?").

Group (no title): style cards
- Style cards (9 cards) <- Style / card picker (`menuStyle`)

Group "Mix and match"
- Start menu: <- Style / Start menu: (`menuLook`)
- Taskbar: <- Style / Taskbar: (`taskbarLook`, `taskbarFollowsStyle`, `xpTaskbarTheme`)
- Window title bars: (renamed) <- Style / Windows: (`windowLook`, `windowTheme`) -- fixes the two "Windows:" labels
- hint: "KDE Plasma (default): your own Plasma Style and title bars come back." (kept)

Group "Windows 7" -- visible: `menuKind` in {windows7, modern}
- Menu look: Classic / Modern (renamed) <- Style / Look: (writes `menuLook`, or `menuStyle` when `menuLook` is empty)
- Windows 7 list: (renamed) <- Style / Substyle: (Windows 7 list) (`win7Substyle`) -- visible `menuKind === "windows7"`
- Modern list: (renamed) <- Style / Substyle: (Modern list) (`modernSubstyle`) -- visible `menuKind === "modern"`

Group "Windows XP" -- visible: `anyXp`
- XP theme: (renamed) <- Style / Substyle: (XP) (`xpSubstyle`)
- Colour scheme: <- Style / Colour scheme: (`xpColor` + `xpCustomColor` colour button when Custom)
- Use an XP icon theme while XP is active (renamed) <- Icons / Use XP icons with the XP style (`xpIcons`)
- XP icon theme: <- Icons / XP icon theme: (`xpIconTheme`) -- enabled when the check above is on
- hint (reworded): "No XP icon theme is installed. Get one on the Advanced page (Download icon themes..., search
  for "XP")." <- Icons / "No XP icon theme is installed..." -- shown under the same condition as today

Leaves Style: Taskbar layout box (-> Taskbar, MERGED), Start button colour (-> Start button & shortcuts), the
seven size/position rows (-> Start menu), Reset/Backup/Restore and their hint (-> Advanced).

### 3.2 Start menu

Group "Size and position"
- Size: (renamed) <- Style / What size should it be? (`menuSize`)
- Custom height: <- Style / Custom height: (`menuHeight`)
- Position: (renamed) <- Style / Menu position: (`menuPosition`)
- Also align the Start button and taskbar icons with the menu (renamed; keeps today's Right / vertical-panel
  text variants) <- Style / Also centre the Start button and the taskbar icons (`centerTaskbar`) -- visible
  `cfg_menuPosition !== 0`
- Full screen menu (renamed) <- Style / Use full screen menu (`fullScreen`)
- Keep the panel visible <- Style / Keep the panel visible (`keepPanelVisible`) -- enabled when full screen is on
- Menu scale: (renamed) <- Style / Adjust global menu scaling: (`uiScale`)

Group "Tiles"
- Edit menu layout... (applies at once) (renamed: the "Section dividers:" label is dropped) <- Layout / Section
  dividers: [Edit menu layout...] (`designRequest`)
- Tile columns: (renamed) <- Layout / Show icons as tiles split into: (`tileColumns`)
- Space between tiles: <- Layout / Space between tiles: (`tileGap`)
- Tile icon size: <- Layout / Tile icon size: (`tileIconScale`)
- Hide tile captions (show them as tooltips) (renamed) <- Layout / Hide caption on tiles (show it as a tooltip) (`hideTileText`)
- Tile colour: <- Look / Tile colour: (`tileColorMode` + `tileColor` when Custom)
- Tile background: <- Look / Tile background: (`tileOpacity`)
- Tile image: <- Look / Tile image: (`tileImage`)
- Tile image opacity: <- Look / Tile image opacity: (`tileImageOpacity`) -- enabled when an image is set
- Fetch favicons for pinned web links (renamed) <- Layout / Fetch the favicon for pinned web links (`webFavicons`)
  (pinned web links are pins, not list rows; this also removes it from the "Hide the apps list" dependency it never had)
- Lock tile and pin positions (renamed) <- Layout / Lock menu shortcut positions (`tilesLocked`)
- Reset tile and pin positions... (renamed) <- Layout / [Reset icon positions...] (`resetRequest` "positions")

Group "Windows 12" -- visible: `menuKind === "windows12"` (today: `cfg_menuStyle === "windows12"`, which ignores `menuLook`)
- Apps list side: <- Layout / Apps list side: (`win12ListSide`)
- Greeting with picture: <- Layout / Greeting with picture: (`win12Greeting`)
- Show Documents, Pictures and Music above the cards <- Layout (`win12Chips`)
- Show the clock and date above the cards <- Layout (`win12Clock`)

### 3.3 Apps list

Group "Apps list"
- Hide the apps list (tiles only) <- Layout (`hideAppList`) -- when on, every other row of this group and of
  "All Programs" is disabled (today 8 rows; `recentHideFolders` now included, inventory problem 12)
- Icon size: (renamed) <- Layout / Shortcut icons should be sized: (`listIconSize`)
- Row height: <- Layout / Row height: (`listRowHeight`)
- Show recently added apps <- Layout (`showRecentlyAdded`)
- Show my most used applications <- Layout (`showMostUsed`)
- Highlight new applications <- Layout (`highlightNew`)
- Show app categories as folders <- Layout (`categoryFolders`)
- Place folders at the bottom of the list <- Layout (`foldersAtBottom`) -- enabled when folders are on
- Hide folders in the recent list (renamed: "(Windows 11 style)" dropped) <- Layout (`recentHideFolders`) --
  visible `menuKind` in {windows11} -- VERIFY: `common/Recommended.qml` reads it; confirm which styles show
  the Recommended list (12 may too) and widen the set accordingly before hiding

Group "All Programs (Windows 7 and XP)" -- visible: `menuKind` in {windows7, modern, xp}
  (VERIFY: `win10/AppList.qml` gates all three behind `treeExtras`; the set must equal the styles that set
  `treeExtras: true`)
- Auto expand folders on hover (renamed: tag dropped) <- Layout / Auto expand folders on hover (Windows 7, XP) (`autoExpand`)
- Show the Desktop folder in All Programs (renamed: tag dropped) <- Layout (`desktopInTree`)
- Show categories as a cascading menu (renamed: tag dropped) <- Layout (`allAppsAsMenu`) -- enabled when folders are on

Group "Hidden apps"
- hint: "None. Right-click an app in the list and choose "Hide from apps"." (kept, when empty)
- one row per hidden app + [Show] <- Layout (`hiddenApps`)
- Clear hidden list <- Layout (`hiddenApps`)

Group "Filtered folders"
- one row per filter + [Remove] <- Layout (`filteredFolders`)
- Name: <- Layout / Name: (`filteredFolders`, new entry)
- Folder: <- Layout / Folder:
- Include (regex): <- Layout / Include (regex):
- Exclude (regex): <- Layout / Exclude (regex):
- Add filtered folder <- Layout / Add filtered folder -- enabled when Name and Folder are filled

### 3.4 Start button & shortcuts

Group "Start button"
- Start button: Icon / Picture / Four squares (renamed: the question "What should the Start button look like?"
  and the hard-coded "Start11 button image" go) <- Start Button (`buttonMode`)
- Icon: (default text "System default" instead of the hard-coded "Arch Linux (default)") <- Start Button / Icon: (`buttonIcon`) -- visible `buttonMode === 2`
- Icon size: <- Start Button / Icon size: (`buttonIconScale`) -- visible `buttonMode === 2`
- Picture: (renamed) <- Start Button / Image: grid (`buttonImage`) -- visible `buttonMode === 0`
- Use a custom picture... (renamed) <- Start Button / Use a custom start button image... (`buttonImage`) -- visible `buttonMode === 0`
- hint (reworded): "A custom picture holds 3 images stacked top to bottom: normal, mouse over, pressed." -- visible `buttonMode === 0`
- First square: / Other squares: (renamed: two labelled colour buttons instead of one "Colours:" with two unlabelled
  buttons) <- Start Button / Colours: (`squaresAccent` = first square, `squaresColor` = the other three; `StartButton.qml:191`) -- visible `buttonMode === 1`
- XP Start button colour: (renamed) <- Style / Start button colour: (`xpStartGreen`, `xpStartCustom`, `xpStartColor`) -- visible `menuKind === "xp"`

Group "Right-click menu"
- Show in the right-click menu: 9 checks (renamed from the section title "Right click menu") <- Start Button (`buttonMenuItems`)

Group "Menu shortcuts" (the left-hand rail)
- Shortcut labels: (renamed) <- Shortcuts / Decide how shortcut icons look: (`railMode`)
- Icon size: <- Shortcuts / Icon size: (`railIconSize`)
- Align the icons to the bottom (renamed: tag dropped) <- Shortcuts / ... (Windows 10 style) (`railAlignBottom`) -- visible `menuKind === "windows10"` (`win10/LeftRail.qml`)
- Small place rows (renamed: tag dropped) <- Shortcuts / Small place rows (Windows 7, XP, Pro styles) (`smallPlaces`) --
  visible `menuKind` in {windows7, modern, xp, pro} (read by `styles/windows7.qml`, `xp.qml`, `pro.qml`; VERIFY modern uses windows7.qml)
- Top to bottom: list (icon, name, up, down, remove) <- Shortcuts (`railItems`)
- add combo + [Add] <- Shortcuts (`railItems`)
- Add custom location... <- Shortcuts (`railItems`)

Group "Power"
- Clicking the power button: (renamed, "should" dropped) <- Shortcuts / Clicking the power button should: (`powerButtonAction`)
- Show in the power menu: 7 checks <- Shortcuts (`powerItems`)
- Show an icon on the Shut down button (renamed: tag dropped) <- Shortcuts / ... (Windows 7 style) (`powerButtonIcon`) --
  visible `menuKind` in {windows7, modern} (read by `styles/windows7.qml`; VERIFY modern)

### 3.5 Taskbar

- hint (reworded, now conditional): "Your own Plasma Style is in use. Pick a Windows taskbar on the Style page to
  change its layout and colours." <- Taskbar / "The taskbar and window looks are picked on the Style page." --
  visible `taskbarKind === ""`

Group "Windows 12 taskbar" -- visible: `taskbarKind === "windows12"` (today `(taskbarLook || menuStyle) === "windows12"`,
which shows the group for a taskbar set to KDE Plasma)
- Layout: (TaskbarLayoutBox: combo "One floating bar / Split: widgets left, apps centre, tray right / Split: Start and
  apps left, tray right / 3 Islands: pinned apps, open apps, tray", its hint, "Magnetic islands" + hint, the red
  patched-KDE warning) <- Style / Taskbar layout: AND Taskbar / Taskbar layout: (`taskbarLayout`, `taskbarMagnetic`)
  -- MERGED: one instance, one pacman check, one warning
- Show islands on: -- NEW (spec section 10): one entry per connected monitor (output name) + "All monitors";
  default the main monitor -- enabled when Layout is not "One floating bar" (same rule as Magnetic islands)
- Open apps on other monitors: -- NEW (spec section 10): "Only apps on that monitor" (default) / "All apps on every
  monitor" -- enabled when Show islands on = All monitors
  (key names for both come from the islands implementation, not this document)
- Floating bar <- Taskbar / Floating bar (`taskbarFloating`)
- Bottom spacing: <- Taskbar / Bottom spacing: (`taskbarGap`) -- enabled when Floating bar is on
- Maximised windows: Fill the screen / Keep a gap above the taskbar (renamed) <- Taskbar / Windows: (`taskbarWindows`)
  + its "Fill the screen: ..." hint -- enabled when Floating bar is on
- Menu gap: <- Taskbar / Menu gap: (`menuGap`)
- Pop-up animation: (renamed) <- Taskbar / Taskbar pop-up animation: (`taskbarPopAnim`) -- enabled when Floating bar
  is on; hidden in gap mode (as today)
- Corner roundness: Square ... Round (renamed: gets a field label) <- Taskbar / unlabelled slider (`taskbarRound`)

Group "Taskbar colour and texture" -- visible: `taskbarGenerated` (= `cfg_taskbarFollowsStyle || cfg_taskbarTheme === "custom"`)
or the Override row below is visible
- Use a custom taskbar colour instead of your Plasma Style (renamed) <- Taskbar / Override the taskbar colour
  (`taskbarTheme`) -- visible `!cfg_taskbarFollowsStyle` (today: enabled-only, with no visible reason)
- Colour: <- Taskbar / Custom taskbar: (a) AND Taskbar / Bar colour: colour (`taskbarColor`) -- MERGED
- Opacity: <- Taskbar / Custom taskbar: (b) AND Taskbar / Bar colour: slider (`taskbarAlpha`) -- MERGED; the
  "Colour and opacity follow "Override the taskbar colour" below." hint is dropped
- Corner radius: (renamed: gets a label) <- Taskbar / Custom taskbar: (c) unlabelled spin (`taskbarRadius`)
- Light line on the taskbar edge <- Taskbar / Custom taskbar: (d) (`taskbarLine`)
- Task buttons: <- Taskbar / Task buttons: (`taskbarButtons`)
  (Colour .. Task buttons enabled when Override is on; when disabled one hint line: "Available with a custom
  taskbar colour." -- inventory problem 11)
- Gradient: (combo + strength) <- Taskbar / Gradient: (`taskbarGradient`, `taskbarGradientStrength`)
- Texture: <- Taskbar / Texture: (a) (`taskbarImage`)
- Placement: (renamed: gets a label) <- Taskbar / Texture: (b) mode combo (`taskbarImageMode`) -- enabled when an image is set
- Texture opacity: (renamed) <- Taskbar / Texture: (c) (`taskbarImageAlpha`) -- enabled when an image is set
- Tint with the accent colour <- Taskbar / Texture: (d) (`taskbarImageTint`) -- enabled when an image is set
- Paint the active button less transparent <- Taskbar (`taskbarFocusedSolid`)

Group "Behaviour"
- Auto-hide: + its explanation hint <- Taskbar / Auto-hide: (`taskbarHiding`) -- visible `taskbarKind` not in
  {"", "plasma"} and not gap mode (today tests `(taskbarLook || menuStyle) === "plasma"`)
- Show a search box on the taskbar (renamed) <- Taskbar / Search: Show search on taskbar (`taskbarSearch`)
- Search box width: <- Taskbar / Search box width: (`taskbarSearchWidth`) -- enabled when the box is on

Group "Plasma panel"
- hint (reworded, drops "floating, auto-hide" which contradicted the rows above -- problem 10): "Panel height,
  position and task grouping are set by Plasma itself." <- Taskbar closing label
- Edit panel... (applies at once) <- Taskbar / Plasma settings: [Edit panel...]

Leaves Taskbar: Window frames (a)-(h) and the XP Firefox row (-> Windows); How light / How dark / Transparency /
Blur strength / Grain and the "Taskbar, Start menu and title bars; ..." hint (-> Appearance); Bar colour row
(MERGED above).

### 3.6 Windows

Group "Title bars and frames" -- enabled when `cfg_windowTheme`; when off one hint line: "Pick a Windows title bar
on the Style page to change these."
- Opacity: (renamed: gets a label) <- Taskbar / Window frames: (a) (`windowOpacity`)
- Blur behind translucent frames <- Taskbar / Window frames: (b) (`windowBlur`) -- enabled when opacity < 100
- Corner radius: <- Taskbar / Window frames: (c) (`windowRadius`, check + spin)
- Title bar height: <- Taskbar / Window frames: (d) (`titleHeight`, check + spin)
- Title bar picture: <- Taskbar / Window frames: (e) (`windowImage`)
- Placement: (renamed: gets a label) <- Taskbar / Window frames: (f) (`windowImageMode`) -- enabled when a picture is set
- Picture opacity: (renamed: gets a label) <- Taskbar / Window frames: (g) (`windowImageAlpha`) -- enabled when a picture is set
- Tint with the accent colour <- Taskbar / Window frames: (h) (`windowImageTint`) -- enabled when a picture is set

Group "Windows XP" -- visible: `windowKind === "xp"` (today `cfg_menuStyle === "xp"`)
- Use the system title bar in Firefox (renamed: "with XP themes" dropped, the group says it) <- Taskbar (`xpFirefoxTitlebar`)

### 3.7 Appearance (Look + Colours merged)

Group "Colours" -- visible: `menuKind !== "xp"`; when XP, one line instead: "Windows XP uses its own colour scheme
(Style page)." <- Colours / "The Windows XP style uses its own colour scheme (Start Menu page)..." (reworded; the
page it names no longer exists)
- Mode: Light / Dark / Automatic (Plasma colours) / Custom colours (renamed) <- Look / Start menu colours: AND Colours /
  Choose your mode: (`colorMode`) -- MERGED into one combo with all four values (2, 1, 0, 3); the Colours page's
  "Custom (Look page)" placeholder goes
- Menu colour: <- Look / Menu colour: (`menuColor`) -- visible `colorMode === 3`
- Text colour: <- Look / Text colour: (`textColor`) -- visible `colorMode === 3`
- Accent colour: Automatic (Plasma accent colour) / Custom <- Look / Selected item accent colour: AND Colours / Accent
  colour: (`accentMode`) -- MERGED; the Look page's separate colour button goes (View colours below does the same)
- Recent colours: <- Colours / Recent colours: (`recentAccents`) -- visible when not empty
- Windows colours: 48 swatches <- Colours / Windows colours: (`accentColor`; clicking still sets `accentMode` = 1)
- Custom colours: [View colours] <- Colours / Custom colours: (`accentColor`)
- Show accent colour on Start and taskbar <- Colours (`win10AccentStart`)
- Show accent colour on title bars and window borders <- Colours (`win10AccentTitles`)
- hint: "The taskbar and title bars follow these when their look is not Plasma's own (Style page)." (kept)

Group "Windows 12 shade" -- visible: `anyKind("windows12")`
- Light mode tint: Soft tint ... Near white (renamed) <- Taskbar / How light: (`win12Light`)
- Dark mode shade: Soft dark ... Near black (renamed) <- Taskbar / How dark: (`win12Dark`)

Group "Transparency and corners"
- Transparency effects <- Colours / Transparency effects (`win10Transparency`) -- visible `menuKind !== "xp"` (same as Colours)
- Solid Start menu (renamed) <- Look / Make the start menu solid (`solidMenu`)
- Start menu background: N% solid (renamed) <- Look / Menu background transparency level: (`menuOpacity`) -- enabled when not solid
- Windows 12 transparency: Solid ... Clear (renamed) <- Taskbar / Transparency: (`win12Transparency`) -- visible `anyKind("windows12")`
- Windows 12 blur: Soft ... Strong (renamed) <- Taskbar / Blur strength: (`win12Blur`) -- visible `anyKind("windows12")`
- Windows 12 grain: Smooth ... Grainy (renamed) <- Taskbar / Grain: (`win12Noise`) -- visible `anyKind("windows12")`
- hint: "Taskbar, Start menu and title bars; transparency changes only the background ..." <- Taskbar 12 group hint
  (moved with the three sliders; visible with them)
- Menu corner radius: (renamed) <- Look / Round the corners of the menu: (`cornerRadius`)

Group "Texture"
- Texture: (renamed) <- Look / Apply an image to the background: (`texture` + [Pick custom texture...])
- Placement: (renamed) <- Look / Texture alignment: (`textureMode`) -- enabled when a texture is set
- Fade texture: <- Look / Fade texture: (`textureOpacity`) -- enabled when a texture is set
- Darken texture <- Look (`darkenTexture`) -- enabled when a texture is set
- Recolour the texture with the menu colour <- Look (`colorizeTexture`) -- enabled when a texture is set
- texture preview (info) <- Look

Group "Fonts"
- Menu font: <- Look (`fontFamily`)
- Font size: <- Look (`fontSize`)
- Header font: <- Look (`headerFontFamily`)
- Header font size: <- Look (`headerFontSize`)
- Item font size: <- Look (`itemFontSize`)

Group "Effects"
- Fade the menu in when it opens (renamed: "Animation:" prefix dropped) <- Look (`animateMenu`)
- Show app icons in grey (renamed: "Icons:" prefix dropped) <- Look (`greyIcons`)

Leaves Look: the four Tile rows (-> Start menu).

### 3.8 Search (same controls, grouped)

Group "Results"
- Group results by type (Apps, Files, Settings ...) <- Search (`groupResults`)
- Classic search: dense one-line results, no Best match card <- Search (`classicSearch`)
- Show filter icons above the results (Apps, Documents, Web ...) <- Search (`searchFilterIcons`)
- Show the "Search the internet" button (renamed) <- Search / Show search the internet button (`showWebSearch`)
- Search the internet with: + "%s is replaced by what you typed." <- Search (`webSearchUrl`) -- enabled when the button is on

Group "History"
- Remember recent searches <- Search (`searchHistoryEnabled`)
- Clear search history <- Search (`searchHistory`) -- enabled when history is not empty

Group "What to search"
- Use everything KRunner can search <- Search (`searchRunners`) + new hint "Off: starts with Apps, Settings and
  Files; tick more below." (problem 11, the silent pre-selection)
- 14 source checks <- Search (`searchRunners`) -- enabled when "Use everything" is off

### 3.9 Advanced

Group "Icons and wallpaper (applied at once, not on Apply)"
- Icon theme: <- Icons / Icon theme: (system, runs plasma-changeicons)
- Download icon themes... <- Icons / Get more: [Download icon themes...] (KNewStuff)
- Download wallpapers... <- Icons / Wallpapers: [Download wallpapers...] (KNewStuff)
- Wallpaper: [Choose image...] [Set as wallpaper] + chosen path (renamed from "Your own wallpaper:") <- Icons

Group "Backup and reset" (act after OK/Apply, as today)
- Backup settings... <- Style (`resetRequest` "backup")
- Restore settings... <- Style (`resetRequest` "restore")
- Reset all Win12 settings... <- Style (`resetRequest` "all")
- hint: "Settings, tiles and pinned icons go into one file. After picking the file, press OK/Apply ..." <- Style (kept)

### 3.10 Duplicates and removals, summarised

| Inventory duplicate | Resolution |
|---|---|
| TaskbarLayoutBox on Style and Taskbar | Taskbar > Windows 12 taskbar only; one pacman check, one warning |
| `colorMode` on Look (4 choices) and Colours (3 + placeholder) | Appearance > Colours > Mode:, one combo with the 4 values |
| `accentMode` + `accentColor` on Look and Colours | Appearance > Colours > Accent colour: + swatches/recent/View colours; Look's colour button dropped |
| `taskbarColor` + `taskbarAlpha` as "Bar colour:" (12 group) and "Custom taskbar:" | Taskbar > Taskbar colour and texture > Colour:, Opacity:; "Bar colour:" row and its hint dropped |

Dropped text (not controls): the question headings, "Section dividers:", "Colour and opacity follow ... below.",
the two duplicated TaskbarLayoutBox hints and warning, and "floating, auto-hide" in the Plasma hint. Nothing
else is removed; no key is removed.

## 4. Visibility rules (one rule set for every page)

Today the Style page uses `menuKind` / `taskbarKind` / `windowKind` / `anyXp` while the other pages test
`cfg_menuStyle` (and `cfg_taskbarLook`) directly and ignore `menuLook` / `taskbarFollowsStyle`. The proposal: one
helper (for example `config/StyleKinds.qml`, an Item each page instantiates with the six cfg values) defines the
properties below; every page uses only these, never `cfg_menuStyle` directly for style gating.

```
menuKind    = cfg_menuLook    || cfg_menuStyle            // "modern" stays "modern"; the 7 family is {windows7, modern}
taskbarKind = cfg_taskbarFollowsStyle ? (cfg_taskbarLook || cfg_menuStyle) : ""   // "" = the user's own Plasma Style
windowKind  = cfg_windowTheme ? (cfg_windowLook || cfg_menuStyle) : ""           // "" = the user's own title bars
anyXp       = menuKind === "xp" || taskbarKind === "xp" || windowKind === "xp"
anyKind(k)  = menuKind === k || taskbarKind === k || windowKind === k
taskbarGenerated = cfg_taskbarFollowsStyle || cfg_taskbarTheme === "custom"
```

Every page that gates by style declares the six keys read-only (`cfg_menuStyle`, `cfg_menuLook`, `cfg_taskbarLook`,
`cfg_taskbarFollowsStyle`, `cfg_windowLook`, `cfg_windowTheme`); Taskbar also `cfg_taskbarTheme`, Start menu also
`cfg_menuPosition`, Windows also `cfg_windowTheme`.

Which kind an option tests, by what it changes:

| Option affects | Test | Examples |
|---|---|---|
| the menu | `menuKind` | Windows 7 / Modern lists, Windows 12 menu extras, All Programs options, rail alignment, small places, power button icon, XP Start button colour, Colours group (`!== "xp"`) |
| the taskbar | `taskbarKind` | Windows 12 taskbar group, Auto-hide (`not in {"", "plasma"}`), the Plasma hint (`=== ""`) |
| the window decoration | `windowKind` / `cfg_windowTheme` | XP Firefox title bar, Title bars and frames group |
| menu, taskbar and title bars together | `anyKind` / `anyXp` | XP theme, colour scheme and icons; Windows 12 shade; Windows 12 transparency, blur, grain |

Conventions:
- Style gating uses `visible` (the row collapses; Kirigami.FormLayout hides an empty section title with it).
  Dependencies on another control of the same page use `enabled`, and whenever a whole block is disabled one hint
  line says why ("Available with a custom taskbar colour.", "Pick a Windows title bar on the Style page ...").
- Label text never carries a style tag again ("(Windows 7, XP)" etc.); the group title or the visibility does it.
- Three sets are marked VERIFY in section 3 (`recentHideFolders`, `smallPlaces`/`powerButtonIcon` for modern,
  the `treeExtras` styles). Grep the style files for each key before hiding; until verified, show the row for the
  wider set rather than hide it for a style that reads it.

## 5. Spanish (and the other po files)

`po/es.po` (568 msgids today; also ar, de, fr, hi, it, ja, pt_BR, ru, zh_CN) needs an entry for every renamed or
new string: the 9 page names (`Start menu`, `Apps list`, `Start button & shortcuts`, `Windows`, `Appearance`,
`Advanced` are new as page names), every group title in section 3, every label marked "(renamed)", the reworded
hints, the two new island settings and their choices ("Show islands on", "All monitors", "Open apps on other
monitors", "Only apps on that monitor", "All apps on every monitor"), the "3 Islands: pinned apps / open apps /
tray" layout choice, and "(applies at once)". Labels that only move keep their existing msgid and translation.
Old msgids for dropped text become obsolete on the next `template.pot` refresh. Translation itself is not part of
this document.

## 6. Risks and rules for the implementation

- Keys do not change. `main.xml` is untouched except for the two new island keys the islands work adds. User
  configs keep working because only `config.qml` (page list, names, icons, sources) and the page QML move.
- Every page must declare `cfg_<key>` for each key it edits; moving a control means moving its `property alias`
  too, or the control silently stops saving. The merged `colorMode` combo must keep the 0..3 mapping; the swatch
  grid must keep writing `accentMode = 1` and `recentAccents`.
- The 12-only sliders (`win12Light` ... `win12Noise`) move off the Taskbar page: anything that binds them to the
  Taskbar page's other rows (gap mode checks) must be re-read; they are independent keys, so this should be a move only.
- `taskbarTheme` "custom" stays a third taskbar mode reachable only by the Override check. Follow-up (not here):
  add "Custom colour" to the Style page's Taskbar combo so the mode is picked where the others are.
- Hiding a style-tagged row hides a setting the user may have turned on under another style; the value stays in
  the config and comes back with the style. Acceptable, but the VERIFY sets in section 3 must be checked first so
  no style loses a row it reads.
- The four corner controls and the three transparency controls keep their units and ranges (a unit change would be
  a behaviour change, out of scope); the harmonised labels make the differences visible instead.
- `ConfigMenu.qml` is split into two files and `ConfigLook.qml` / `ConfigColours.qml` merge: any test or script that
  names those files (`tests/`, `kde/`) must be updated with the rename. Other agents are editing `package/contents/ui`
  and `kde/` now; this reorganisation should land after the islands work so the two new settings have their keys.

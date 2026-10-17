# Win12 settings inventory (current state)

Source: `package/contents/config/config.qml` (9 pages, in this order) and `package/contents/ui/config/*.qml`.
Types and defaults from `package/contents/config/main.xml`. Read-only inventory, no code was changed.

Conventions:
- Rows are in on-screen order. "info" in the type column means a non-interactive label or message (not counted as a control).
- "ALSO ON: X" marks a key or widget that appears on more than one page.
- Style tags: XP / 7 / 10 / 11 / 12 / App / Pro / Launcher / Plasma = menu style ids `windows7`, `windows10`, `windows11`, `windows12`, `app`, `pro`, `launcher`, `xp`, `plasma`. "all" = no style condition in the page code (a style may still ignore it inside `main.qml`; that was not checked).
- `menuKind` (Style page) = `menuLook || menuStyle`; `taskbarKind` = taskbarFollowsStyle ? (`taskbarLook || menuStyle`) : "" ; `windowKind` = windowTheme ? (`windowLook || menuStyle`) : "". `anyXp` = any of the three is `xp`.
- Pages other than Style do not know `menuLook`; they test `cfg_menuStyle` (and on Taskbar `cfg_taskbarLook`) directly.

Page order: 1 Style, 2 Layout, 3 Shortcuts, 4 Look, 5 Colours, 6 Search, 7 Taskbar, 8 Icons, 9 Start Button.

---

## 1. Style (`ConfigStyle.qml`, icon preferences-desktop-theme-global)

| Label (as shown) | Config key | Type / choices / range (default) | Shown only when | Affects |
|---|---|---|---|---|
| Which style would you prefer? (heading) + "A style sets the Start menu, the taskbar and the window title bars together. Mix them below." | none | info | always | explains the card picker |
| Style cards: Windows 7 style, Windows 10 style, Windows 11 style, Windows 12 style, Windows App style, Windows Pro style, Windows Launcher style, Windows XP style, KDE Plasma (each with a drawn preview) | `menuStyle` | card picker, 9 cards: windows7, windows10, windows11, windows12, app, pro, launcher, xp, plasma (def windows10). The Windows 7 card is also lit when value is `modern`. Cards not in the `built` list show "Coming soon" and are disabled (all 9 are currently built) | always | the whole look: menu, and the taskbar and window title bars unless overridden below |
| Start menu: | `menuLook` | combo: Same as the style (""), Windows 7, Modern, Windows 10, Windows 11, Windows 12, App, Pro, Launcher, Windows XP, KDE Plasma (default) (def "") | always | menu style only, overrides the card |
| Taskbar: | `taskbarLook` + `taskbarFollowsStyle` + `xpTaskbarTheme` (all three written by one combo) | combo: Same as the style, KDE Plasma (default) ("none" = both bools off), Windows XP, 7, 10, 11, 12 (def Same) | always | which generated Plasma Style the taskbar uses; "KDE Plasma" restores the user's own style |
| Windows: | `windowLook` + `windowTheme` (one combo) | combo: Same as the style, KDE Plasma (default) ("none" = windowTheme off), Windows XP, 7, 10, 11 (no 12) (def Same) | always | window title bar decoration |
| "KDE Plasma (default): your own Plasma Style and title bars come back." | none | info | always | hint for the two combos above |
| Taskbar layout: (TaskbarLayoutBox) ALSO ON: Taskbar | `taskbarLayout`, `taskbarMagnetic` (see TaskbarLayoutBox rows at the end) | widget group | `taskbarKind === "windows12"` | 12 taskbar shape (single bar / split / islands) |
| Look: | `menuLook` if set, else `menuStyle` (writes "windows7" or "modern") | combo: Classic, Modern | `menuKind` is windows7 or modern | 7 only: classic vs modern menu |
| Substyle: (Windows 7 list) | `win7Substyle` | combo of 6: Windows 7 style, ... with icons, Compact, Grid, Grid with icons, Compact Grid (def 0) | `menuKind === "windows7"` | 7 only: list layout variant |
| Substyle: (Modern list) | `modernSubstyle` | combo of 6: Modern style, ... with icons, Compact, Grid, Grid with icons, Compact Grid (def 1) | `menuKind === "modern"` | 7 Modern only: layout variant |
| Substyle: (XP) | `xpSubstyle` | combo: Classic XP (luna), Modern classic (classic), Modern (modern) (def classic) | `anyXp` | XP only: menu, taskbar and window flavour |
| Colour scheme: | `xpColor` (+ `xpCustomColor` colour button when Custom) | combo: Blue, Olive Green, Silver, Custom (def blue); colour button (def #3a6ea5) shown when xpColor === custom | `anyXp` | XP only: Luna colour scheme for menu, taskbar, windows |
| Start button colour: | `xpStartGreen`, `xpStartCustom` (one combo) + `xpStartColor` colour button | combo: XP green, Colour scheme, Custom (def XP green); colour button (def #3c9a3c) shown when xpStartCustom | `menuKind === "xp"` | XP only: colour of the Start button |
| What size should it be? | `menuSize` | combo: Small, Tall, Wide (two tile columns), Large (two tile columns) (def 1) | always | menu size preset |
| Menu position: | `menuPosition` | combo: Left/Center/Right (Top/Middle/Bottom on a vertical panel) (def 0) | always | where the menu opens relative to the screen |
| Also centre the Start button and the taskbar icons (text changes: "Also mirror the taskbar (Start button on the right, clock on the left)" when Right; vertical-panel variants) | `centerTaskbar` | check (def false) | `menuPosition !== 0` | taskbar icon alignment follows menu position |
| Custom height: | `menuHeight` | spin 0..1600 step 20, 0 shows "From size" (def 0) | always | fixed menu height |
| Use full screen menu | `fullScreen` | check (def false) | always | full screen menu mode |
| Keep the panel visible | `keepPanelVisible` | check (def false) | enabled only when "Use full screen menu" is on | panel stays over the full screen menu |
| Adjust global menu scaling: | `uiScale` | slider 50..200 step 5 percent, label "N% of original size" (def 1.0 = 100) | always | scale of the whole menu |
| Reset all Win12 settings… | `resetRequest` ("all/time") | button + confirm dialog | always | resets every setting, tiles and pins; restores previous Plasma Style if XP taskbar was active |
| Backup settings… | `resetRequest` ("backup/time/file") | button + save-file dialog | always | writes settings, tiles and pins to one json file |
| Restore settings… | `resetRequest` ("restore/time/file") | button + open-file dialog | always | reads that json back |
| "Settings, tiles and pinned icons go into one file. After picking the file, press OK/Apply..." | none | info | always | hint |

Hidden keys the page declares but only reads: `cfg_taskbarFollowsStyle`, `cfg_xpTaskbarTheme`, `cfg_windowTheme`, `cfg_taskbarLayout`, `cfg_taskbarMagnetic`.

### TaskbarLayoutBox (`TaskbarLayoutBox.qml`), used on Style and Taskbar

| Label (as shown) | Config key | Type / choices / range | Shown only when | Affects |
|---|---|---|---|---|
| "The taskbar layouts and the Start position need Win12's patched KDE packages. Run kde/build.sh ..." | none (pacman -T check of libplasma-win12, plasma-workspace-win12, plasma-desktop-win12) | info, red | the pacman check fails | warns the layouts need the patched KDE |
| (no label) layout combo: One floating bar, Split: widgets left / apps centre / tray right, Split: Start and apps left / tray right, 3 Islands: pinned apps / open apps / tray | `taskbarLayout` | combo of 4: single, split-left, split-right, islands (def single) | parent decides (Style: taskbarKind windows12; Taskbar: inside the Windows 12 group) | 12 taskbar layout |
| "Split layouts put the tray, clock and weather into separate floating panels (islands). ..." | none | info | always | hint |
| Magnetic islands | `taskbarMagnetic` | check (def true) | enabled when layout is not single | islands close together in the middle vs pushed to edges |
| "On: the islands sit close together in the middle of the screen. Off: pushed to the screen edges." | none | info | always | hint |

---

## 2. Layout (`ConfigMenu.qml`, icon view-grid)

Contents: tiles, apps list, Windows 12 extras, hidden apps, filtered folders. The Start11 name of the page is "Configure menu".

| Label (as shown) | Config key | Type / choices / range (default) | Shown only when | Affects |
|---|---|---|---|---|
| Section dividers: [Edit menu layout…] | none (writes `designRequest` straight to the widget config) | button, applies at once, no Apply | always | opens the menu in design mode |
| (section) Tiles | | | | |
| Show icons as tiles split into: | `tileColumns` | spin 3..8, "N columns" (def 6) | always (no style test) | tile grid width |
| Space between tiles: | `tileGap` | spin 0..16 px (def 4) | always | tile spacing |
| Tile icon size: | `tileIconScale` | slider 25..90 percent (def 56) | always | icon size inside a tile |
| Hide caption on tiles (show it as a tooltip) | `hideTileText` | check (def true) | always | tile captions |
| Lock menu shortcut positions | `tilesLocked` | check (def false) | always | stops drag moving of tiles and pins |
| (section) Apps list | | | | |
| Hide the apps list (tiles only) | `hideAppList` | check (def false) | always | hides the apps list; disables 8 controls below |
| Shortcut icons should be sized: | `listIconSize` | spin 12..64 px (def 22) | enabled when apps list not hidden | apps list icon size |
| Row height: | `listRowHeight` | spin 20..80 px (def 36) | enabled when apps list not hidden | apps list row height |
| Show recently added apps | `showRecentlyAdded` | check (def false) | enabled when list not hidden | recent apps group |
| Hide folders in the recent list (Windows 11 style) | `recentHideFolders` | check (def false) | always enabled; text says 11 | recent list content (11) |
| Fetch the favicon for pinned web links | `webFavicons` | check (def false) | always | pinned web link icons |
| Show my most used applications | `showMostUsed` | check (def false) | enabled when list not hidden | most used group |
| Highlight new applications | `highlightNew` | check (def true) | enabled when list not hidden | new app highlight |
| Show app categories as folders | `categoryFolders` | check (def false) | enabled when list not hidden | category folders in the list |
| Place folders at the bottom of the list | `foldersAtBottom` | check (def false) | enabled when list not hidden and folders on | folder order |
| Auto expand folders on hover (Windows 7, XP) | `autoExpand` | check (def false) | enabled when list not hidden; text says 7, XP | 7 / XP folder expand |
| Show the Desktop folder in All Programs (Windows 7, XP) | `desktopInTree` | check (def false) | enabled when list not hidden; text says 7, XP | 7 / XP All Programs tree |
| Show categories as a cascading menu (Windows 7, XP) | `allAppsAsMenu` | check (def false) | enabled when list not hidden and folders on; text says 7, XP | 7 / XP cascading All Programs |
| (section) Windows 12 style | | | `cfg_menuStyle === "windows12"` (ignores `menuLook`) | |
| Apps list side: | `win12ListSide` | combo: Automatic (menu on the left: left, else right), Left, Right (def 0) | same as section | 12 only |
| Greeting with picture: | `win12Greeting` | combo: Automatic (when the list is on the left), On, Off (def 0) | same | 12 only |
| Show Documents, Pictures and Music above the cards | `win12Chips` | check (def true) | same | 12 only |
| Show the clock and date above the cards | `win12Clock` | check (def true) | same | 12 only |
| (section) Hidden apps | | | | |
| "None. Right-click an app in the list and choose "Hide from apps"." | none | info | `hiddenApps` is empty | hint |
| (one row per hidden app: name + [Show]) | `hiddenApps` | string list; button per row | one row per entry | un-hides one app |
| Clear hidden list | `hiddenApps` | button | `hiddenApps` not empty | un-hides all |
| (section) Filtered folders | | | | |
| (one row per filter: "name (path, +include, -exclude)" + [Remove]) | `filteredFolders` | JSON string list; button per row (def []) | one row per entry | App-style groups / 7 All Programs folders |
| Name: | `filteredFolders` (new entry) | text, placeholder "PDFs" | always | new filtered folder |
| Folder: | same | text, placeholder "~/Downloads" | always | new filtered folder |
| Include (regex): | same | text, placeholder "pdf$" | always | new filtered folder |
| Exclude (regex): | same | text | always | new filtered folder |
| Add filtered folder | `filteredFolders` | button | enabled when Name and Folder filled | adds the entry |
| [Reset icon positions…] (under section "Reset") | `resetRequest` ("positions/time") | button + confirm dialog | always | tiles, pins, pin names back to fresh install |

Also declared (not shown): `cfg_menuStyle`, `cfg_filteredFolders`.

---

## 3. Shortcuts (`ConfigRail.qml`, icon bookmarks)

Contents: the left-hand rail (shortcuts), place rows, the power button and power menu. Start11 sections "Left hand side shortcuts" and "Power menu options".

| Label (as shown) | Config key | Type / choices / range (default) | Shown only when | Affects |
|---|---|---|---|---|
| Decide how shortcut icons look: | `railMode` | combo: Only show icons, Show names when the mouse is over it, Always show icons and names (def 0) | always | rail label mode |
| Icon size: | `railIconSize` | spin 12..48 px (def 18) | always | rail icon size |
| Align the icons to the bottom (Windows 10 style) | `railAlignBottom` | check (def true) | always; text says 10 | 10 rail alignment |
| Small place rows (Windows 7, XP, Pro styles) | `smallPlaces` | check (def false) | always; text says 7, XP, Pro | place row height |
| Show an icon on the Shut down button (Windows 7 style) | `powerButtonIcon` | check (def false) | always; text says 7 | 7 shut down button icon |
| (section) Left hand side shortcuts | | | | |
| Top to bottom: (list; each row has icon, name, up, down, remove) | `railItems` | string list (def user, documents, downloads, music, pictures, personalization, settings, files, power); 24 built-in ids plus `custom/name/icon/target` entries | always | rail items and order |
| (add combo) + [Add] | `railItems` | combo of built-ins not yet used (Separator can repeat) + button; new items go above "Power" | Add enabled when combo not empty | adds a rail item |
| Add custom location… | `railItems` | button -> dialog: Name (text), Opens (Folder or web address / Command), Location or Command (text), Icon (text + icon picker) | always | adds a custom rail item |
| (section) Power menu options | | | | |
| Clicking the power button should: | `powerButtonAction` | combo: Show the power menu, Lock, Sign out, Switch user, Sleep, Hibernate, Restart, Shut down (def menu) | always | action of the power button |
| Show in the power menu: | `powerItems` | 7 checks: Lock, Sign out, Switch user, Sleep, Hibernate, Restart, Shut down (def lock-screen, suspend, reboot, shutdown) | always | entries of the power menu |

---

## 4. Look (`ConfigLook.qml`, icon preferences-desktop-color)

Contents: menu colour, transparency, tile look, texture, fonts. Start11 sections "Adjust colour, transparency..." and "Menu texture and appearance settings". No style conditions anywhere on the page.

| Label (as shown) | Config key | Type / choices / range (default) | Shown only when | Affects |
|---|---|---|---|---|
| (section) Colour | | | | |
| Start menu colours: ALSO ON: Colours | `colorMode` | combo: Automatic (Plasma colours), Force dark mode menu, Force light mode menu, Custom colour (def 1 = dark) | always | menu light/dark mode |
| Menu colour: | `menuColor` | colour (def #1c1b22) | `colorMode === 3` | custom menu colour |
| Text colour: | `textColor` | colour (def #ffffff) | `colorMode === 3` | custom text colour |
| Selected item accent colour: ALSO ON: Colours | `accentMode` + `accentColor` | combo: Plasma accent colour, Custom (def 0); colour (def #0078d4) shown when Custom | colour only when `accentMode === 1` | accent of selected items |
| (section) Transparency | | | | |
| Make the start menu solid | `solidMenu` | check (def false) | always | disables menu transparency |
| Menu background transparency level: | `menuOpacity` | slider 0..100 "N% solid" (def 88) | enabled when not solid | menu background opacity |
| Round the corners of the menu: | `cornerRadius` | spin 0..30 px, 0 = Square (def 8) | always | menu corner radius |
| (section) Tiles | | | | |
| Tile colour: | `tileColorMode` + `tileColor` | combo: Soft (follows text colour), Accent colour, Custom (def 0); colour (def #3a3a44) shown when Custom | colour only when `tileColorMode === 2` | tile fill colour |
| Tile background: | `tileOpacity` | slider 0..100 "N% solid" (def 14) | always | tile fill opacity |
| Tile image: | `tileImage` | file picker (text name + [Pick image…] + clear button) | clear button only when set | picture on tiles |
| Tile image opacity: | `tileImageOpacity` | slider 0..100 (def 60) | enabled when a tile image is set | tile picture opacity |
| Icons: Show app icons in grey | `greyIcons` | check (def false) | always | grey app icons |
| Animation: Fade the menu in when it opens | `animateMenu` | check (def true) | always | open animation |
| (section) Texture | | | | |
| Apply an image to the background: | `texture` | combo (None, bundled textures from images/MenuTextures, the custom file name) + [Pick custom texture…] | always | menu background image |
| Texture alignment: | `textureMode` | combo: Tile, Stretch to show entire texture, Fit, Fill (def 0) | enabled when a texture is set | texture placement |
| Fade texture: | `textureOpacity` | slider 5..100 percent (def 100) | enabled when a texture is set | texture opacity |
| Darken texture | `darkenTexture` | check (def true) | enabled when a texture is set | texture darkening |
| Recolour the texture with the menu colour | `colorizeTexture` | check (def false) | enabled when a texture is set | texture tint |
| (texture preview image) | none | info | texture set | preview |
| (section) Fonts | | | | |
| Menu font: | `fontFamily` | combo: ** Default font **, all Qt font families (def "") | always | menu font |
| Font size: | `fontSize` | spin 7..24 px (def 11) | always | menu font size |
| Header font: | `headerFontFamily` | combo: ** Menu font **, all families (def "") | always | header font |
| Header font size: | `headerFontSize` | spin 0..32 px, 0 = "Menu size" (def 0) | always | header font size |
| Item font size: | `itemFontSize` | spin 0..24 px, 0 = "Menu size" (def 0) | always | item font size |

---

## 5. Colours (`ConfigColours.qml`, icon color-management)

Modelled on Windows 11 Settings > Personalisation > Colours. Header comment: "Every menu style but XP".

| Label (as shown) | Config key | Type / choices / range (default) | Shown only when | Affects |
|---|---|---|---|---|
| "The Windows XP style uses its own colour scheme (Start Menu page). These settings are for the other styles." | none | info message | `cfg_menuStyle === "xp"` (ignores `menuLook`) | explains XP is excluded ("Start Menu page" no longer exists; it is now Style) |
| Choose your mode: ALSO ON: Look | `colorMode` | combo: Light, Dark, Automatic (Plasma colours) (maps to 2, 1, 0); shows "Custom (Look page)" when value is 3 | always | menu light/dark mode |
| Accent colour: ALSO ON: Look | `accentMode` | combo: Automatic (Plasma accent colour), Manual (def 0) | always | accent source |
| Recent colours: | `recentAccents` | up to 5 swatches (string list) | list not empty | quick re-pick of accents |
| Windows colours: | `accentColor` (+ sets `accentMode`=1 and `recentAccents`) | 48 swatches, 8 per row | always | picks the accent colour |
| Custom colours: [View colours] | `accentColor` (same) | button -> colour dialog | always | picks the accent colour |
| Show accent colour on Start and taskbar | `win10AccentStart` | switch (def false) | always | accent on menu and taskbar |
| Show accent colour on title bars and window borders | `win10AccentTitles` | switch (def false) | always | accent on title bars |
| Transparency effects | `win10Transparency` | switch (def true) | always | transparency of menu/taskbar/title bars (Windows 10/11 style looks) |
| "The taskbar and title bars follow these when their look is not "Plasma's own" (Style page)." | none | info | always | hint |

---

## 6. Search (`ConfigSearch.qml`, icon search)

Start11 "How should search work?". No style conditions.

| Label (as shown) | Config key | Type / choices / range (default) | Shown only when | Affects |
|---|---|---|---|---|
| Group results by type (Apps, Files, Settings ...) | `groupResults` | check (def true) | always | result grouping |
| Show search the internet button | `showWebSearch` | check (def true) | always | web search button |
| Show filter icons above the results (Apps, Documents, Web ...) | `searchFilterIcons` | check (def true) | always | filter icon row |
| Classic search: dense one-line results, no Best match card | `classicSearch` | check (def false) | always | result layout |
| Remember recent searches | `searchHistoryEnabled` | check (def true) | always | search history |
| Clear search history | `searchHistory` | button | enabled when history not empty | wipes history |
| Search the internet with: | `webSearchUrl` | text, placeholder https://duckduckgo.com/?q=%s (def same) | enabled when web button on | web search URL |
| "%s is replaced by what you typed." | none | info | always | hint |
| (section) What to search | | | | |
| Use everything KRunner can search | `searchRunners` | check; empty list = all (def empty) | always | all runners vs selection (unchecking starts with Apps, Settings, Files) |
| 14 runner checks: Apps, Settings, Files (file search index), Recent documents, Places, Calculator, Unit converter, Commands, Web shortcuts (gg:, wp:, ...), Browser bookmarks, Open windows, Sessions and power, Special characters, Dictionary | `searchRunners` | 14 checks (string list of runner ids) | enabled when "Use everything" is off | which KRunner plugins are queried |

---

## 7. Taskbar (`ConfigTaskbar.qml`, icon preferences-desktop-icons)

Contents: window frame options (all styles), Windows 12 taskbar group, custom taskbar colour, gradient, texture, task buttons, auto-hide, taskbar search box, link to Plasma panel editing. Declares read-only: `cfg_menuStyle`, `cfg_taskbarLook`, `cfg_windowTheme`, `cfg_taskbarFollowsStyle`, `cfg_taskbarLayout`, `cfg_taskbarMagnetic`.

| Label (as shown) | Config key | Type / choices / range (default) | Shown only when | Affects |
|---|---|---|---|---|
| "The taskbar and window looks are picked on the Style page." | none | info | always | pointer to Style |
| Use the system title bar in Firefox with XP themes | `xpFirefoxTitlebar` | check (def true), tooltip | `cfg_menuStyle === "xp"` (ignores `menuLook`/`taskbarLook`/`windowLook`) | XP only: Firefox title bar |
| Window frames: (a) opacity slider "N% opaque" | `windowOpacity` | slider 30..100 step 5 (def 100) | whole group enabled when `cfg_windowTheme` | all styles: title bar/frame opacity |
| Window frames: (b) Blur behind translucent frames | `windowBlur` | check (def true) | enabled when opacity below 100 | frame blur |
| Window frames: (c) Corner radius: (check + spin) | `windowRadius` | check on = own radius; spin 0..12 px, 0 = Square; -1 = style default (def -1) | spin enabled when check on | frame corner radius |
| Window frames: (d) Title bar height: (check + spin) | `titleHeight` | check on = own height; spin 20..48 px; 0 = style default (def 0) | spin enabled when check on | title bar height |
| Window frames: (e) Title bar picture: | `windowImage` | file picker (name + [Pick image…] + clear) | clear only when set | picture over the title bar |
| Window frames: (f) image mode combo | `windowImageMode` | combo: Stretch, Tile (def stretch) | enabled when a picture is set | picture placement |
| Window frames: (g) image opacity slider "N% opaque" | `windowImageAlpha` | slider 0..100 (def 100) | enabled when a picture is set | picture opacity |
| Window frames: (h) Tint with the accent colour | `windowImageTint` | check (def false) | enabled when a picture is set | picture tint |
| (group) Windows 12 taskbar: Floating bar | `taskbarFloating` | check (def true) | group visible when `(cfg_taskbarLook or cfg_menuStyle) === "windows12"` (ignores `taskbarFollowsStyle` and `menuLook`) | 12: floating bar |
| Taskbar pop-up animation: | `taskbarPopAnim` | combo: Slide (Plasma), Fade, Zoom, Rise (def 2 = Zoom) | 12 group; enabled when Floating on; hidden in "gap mode" (12 + floating + Keep a gap + single layout) | 12: KWin pop-up effect |
| Square ... Round (no field label) | `taskbarRound` | slider 0..100 (def 30) | 12 group | 12: bar corner roundness |
| Bar colour: (colour + opacity slider "N% opaque") | `taskbarColor`, `taskbarAlpha` (SAME keys as "Custom taskbar:" below) | colour (def #101010) + slider 0..100 (def 80) | 12 group; enabled only when "Override the taskbar colour" is on and `taskbarFollowsStyle` is off | 12: bar colour/opacity |
| "Colour and opacity follow "Override the taskbar colour" below." | none | info | 12 group | hint |
| How light: Soft tint ... Near white | `win12Light` | slider 0..100; -1 = default look, shown at 80 (def -1) | 12 group | 12: light-mode tint strength |
| How dark: Soft dark ... Near black | `win12Dark` | slider 0..100; -1 default, shown at 50 (def -1) | 12 group | 12: dark-mode shade |
| Transparency: Solid ... Clear | `win12Transparency` | slider 0..90; -1 default, shown at 70 (def -1) | 12 group | 12: taskbar, Start menu and title bar background transparency |
| Blur strength: Soft ... Strong | `win12Blur` | slider 1..15; <=0 default, shown at 3 (def -1) | 12 group | 12: KWin blur for all windows |
| Grain: Smooth ... Grainy | `win12Noise` | slider 0..14 (def 0) | 12 group | 12: noise texture |
| Bottom spacing: None ... N px | `taskbarGap` | slider 0..24 px (def 8) | 12 group; enabled when Floating on | 12: gap bar/islands to screen edge |
| Windows: Fill the screen / Keep a gap above the taskbar | `taskbarWindows` | combo of 2 (def 0) | 12 group; enabled when Floating on | 12: maximized windows fill screen or stop above the bar (single layout only) |
| "Fill the screen: maximized windows use the whole screen ..." | none | info | 12 group | hint |
| Menu gap: None ... N px | `menuGap` | slider 0..24 px (def 8) | 12 group | 12: Start menu to taskbar distance |
| "Taskbar, Start menu and title bars; transparency changes only the background ..." | none | info | 12 group | hint |
| Taskbar layout: (TaskbarLayoutBox) ALSO ON: Style | `taskbarLayout`, `taskbarMagnetic` | see TaskbarLayoutBox | 12 group | 12: taskbar layout |
| Override the taskbar colour | `taskbarTheme` ("custom"/"off") | check (def off) | enabled only when `taskbarFollowsStyle` is off | custom generated taskbar colour |
| Custom taskbar: (a) colour button | `taskbarColor` | colour (def #101010) | group enabled when Override on and `taskbarFollowsStyle` off | custom bar colour |
| Custom taskbar: (b) opacity slider "N% opaque" | `taskbarAlpha` | slider 0..100 (def 80) | same | custom bar opacity |
| Custom taskbar: (c) corner spin (no label) | `taskbarRadius` | spin 2..24 px (xml allows 2..40), <=2 shows Square (def 2) | same | custom bar corner radius |
| Custom taskbar: (d) Light line on the taskbar edge | `taskbarLine` | check (def false) | same | highlight line |
| Gradient: (combo + strength slider + label "Gradient strength: N%") | `taskbarGradient` (none, left-right, right-left, top-bottom, bottom-top; def none) + `taskbarGradientStrength` (slider 0..100 step 5, def 30) | combo + slider | enabled when `taskbarFollowsStyle` or Override on; slider enabled when gradient not none | gradient inside the generated panel background |
| Texture: (a) file picker | `taskbarImage` | name + [Pick image…] + clear | enabled when `taskbarFollowsStyle` or Override on | taskbar texture |
| Texture: (b) mode combo | `taskbarImageMode` | combo: Tile, Stretch, Align top, Align middle, Align bottom (def tile) | enabled when an image is set | texture placement |
| Texture: (c) opacity slider "N% opaque" | `taskbarImageAlpha` | slider 0..100 (def 100) | enabled when an image is set | texture opacity |
| Texture: (d) Tint with the accent colour | `taskbarImageTint` | check (def false) | enabled when an image is set | texture tint |
| Task buttons: | `taskbarButtons` | combo: Plasma Style, Windows 10 (bar under active), Windows 11 (pill under active), Windows 7 (glass) (def win10) | enabled when Override on and `taskbarFollowsStyle` off | button look in generated tasks.svg |
| Paint the active button less transparent | `taskbarFocusedSolid` | check (def false) | enabled when Override on or `taskbarFollowsStyle` | active button opacity |
| Auto-hide: | `taskbarHiding` | combo: Off, Hide when a window covers it, Always auto-hide (def 0); Off reads "Off (the floating bar still auto-hides)" for 12 + floating | hidden when look is Plasma (`(taskbarLook or menuStyle) === "plasma"`) or gap mode | native panel visibility |
| (auto-hide explanation label; changes text for Plasma look / 12 floating) | none | info | hidden in gap mode | hint |
| Search: Show search on taskbar | `taskbarSearch` | check (def false) | always | search box on the taskbar |
| Search box width: | `taskbarSearchWidth` | spin 100..600 step 10 px (def 220) | enabled when Show search on taskbar is on | taskbar search box width |
| "Panel height, position, floating, auto-hide and task grouping are handled by Plasma itself." | none | info | always | pointer to Plasma panel settings |
| Plasma settings: [Edit panel…] | none (D-Bus editMode true) | button, applies at once | always | opens Plasma edit mode |

---

## 8. Icons (`ConfigIcons.qml`, icon preferences-desktop-icons)

Mostly KDE system actions that run immediately (not saved by Apply), plus the XP icon pair.

| Label (as shown) | Config key | Type / choices / range (default) | Shown only when | Affects |
|---|---|---|---|---|
| Icon theme: | none (runs `/usr/lib/plasma-changeicons <theme>` on pick; reads kdeglobals Icons/Theme) | combo of installed icon themes | always | system-wide icon theme, immediately |
| Use XP icons with the XP style | `xpIcons` | check (def true) | always | XP only: switch icon theme while XP is active |
| XP icon theme: | `xpIconTheme` | combo of installed themes (def "" = first theme with "xp" in its name) | enabled when XP icons on | XP only: which theme |
| "No XP icon theme is installed. Get one with the button below (search for "XP")." | none | info | `xpIconTheme` empty and no theme with "xp" found | hint |
| Get more: [Download icon themes…] | none (KNewStuff icons.knsrc) | button | always | installs icon themes |
| Wallpapers: [Download wallpapers…] | none (KNewStuff wallpaper.knsrc) | button | always | installs wallpapers |
| Your own wallpaper: [Choose image…] [Set as wallpaper] | none (runs `plasma-apply-wallpaperimage`) | 2 buttons; Set enabled when a file is chosen | always | system wallpaper, immediately |
| (chosen wallpaper path) | none | info | a file is chosen | shows path |

---

## 9. Start Button (`ConfigButton.qml`, icon start-here-kde)

| Label (as shown) | Config key | Type / choices / range (default) | Shown only when | Affects |
|---|---|---|---|---|
| What should the Start button look like? | `buttonMode` | 3 radios: Icon (2), Start11 button image (0), Four squares (1) (def 2) | always | button kind |
| Icon: (icon picker button, [Pick custom image…], [Reset icon]) | `buttonIcon` | icon-dialog button (default text "Arch Linux (default)", icon start-here-kde) + file picker + reset | `buttonMode === 2`; Reset only when set | button icon |
| Icon size: | `buttonIconScale` | slider 30..100 percent (def 76) | `buttonMode === 2` | button icon size |
| Image: (grid of bundled 3-frame images) | `buttonImage` | single choice from images/StartButtons (def Default.png) | `buttonMode === 0` | button picture |
| Use a custom start button image… (shows file name when custom) | `buttonImage` | button + file dialog | `buttonMode === 0` | custom button picture |
| "Custom images hold 3 pictures stacked top to bottom: normal, mouse over, pressed." | none | info | `buttonMode === 0` | hint |
| Colours: (two unlabelled colour buttons) | `squaresAccent` (def #e81123), `squaresColor` (def #ffffff) | 2 colour buttons | `buttonMode === 1` | four squares colours |
| (section) Right click menu: 9 checks: Settings, File Explorer, System Monitor, Terminal, Run..., Show desktop, Edit panel, Auto-hide panel, Lock / sign out / sleep / restart / shut down | `buttonMenuItems` | 9 checks (string list; def settings, files, monitor, terminal, run, desktop, editpanel, power) | always | entries of the Start button right-click menu |

---

## Control counts (interactive controls = table rows not typed "info" and not section headings; a list or a group of checks counts as one row)

Style 21 (20 own rows + the TaskbarLayoutBox group, which has 2 more), Layout 32, Shortcuts 10, Look 23, Colours 8, Search 9, Taskbar 38 (includes the TaskbarLayoutBox group), Icons 6, Start Button 6. Total 153 rows.
With repeated items expanded: Style has 9 style cards; Shortcuts has a 24-id add list, 7 power-menu checks; Search has 14 runner checks; Start Button has 9 right-click checks; Layout has one row per hidden app and per filtered folder.

---

## Controls that appear (or are keyed) on more than one page

| Control / key | Where |
|---|---|
| `taskbarLayout` + `taskbarMagnetic` (TaskbarLayoutBox) | Style (taskbarKind 12) and Taskbar (12 group): same widget twice, two pacman checks, two copies of the red warning |
| `colorMode` | Look "Start menu colours:" (4 choices) and Colours "Choose your mode:" (3 choices, different order, shows "Custom (Look page)") |
| `accentMode` + `accentColor` | Look "Selected item accent colour:" and Colours "Accent colour:" + swatches |
| `taskbarColor` + `taskbarAlpha` | Taskbar "Bar colour:" (12 group) and Taskbar "Custom taskbar:" |
| `taskbarFollowsStyle`, `xpTaskbarTheme`, `windowTheme`, `taskbarLook`, `windowLook` | set on Style; read on Taskbar, whose controls are enabled or disabled by them |
| XP colour scheme / substyle | only on Style (`anyXp`), but XP icons (`xpIcons`) are on Icons and the XP Firefox title bar is on Taskbar |

---

## Problems noticed (facts only)

1. Duplicates: TaskbarLayoutBox is on Style and Taskbar; colorMode and accentMode/accentColor are edited on Look and Colours; taskbarColor/taskbarAlpha are edited twice on Taskbar ("Bar colour:" and "Custom taskbar:").
2. Two different controls labelled "Windows:" with different meanings: Style "Windows:" (window decoration look, `windowLook`) and Taskbar "Windows:" (`taskbarWindows`: Fill the screen / Keep a gap). Also "Taskbar layout:" appears as a label on Style and Taskbar; "Substyle:" appears three times on Style (7, Modern, XP) with different keys; "Search" means menu search (Search page) and the taskbar search box (Taskbar).
3. Visibility rules are inconsistent across pages: Style uses `menuKind`/`taskbarKind`/`anyXp`; Layout (12 section), Colours (XP message) and Taskbar (XP Firefox) test `cfg_menuStyle === ...` only and ignore `menuLook`; Taskbar's 12 group and Auto-hide test `(taskbarLook || menuStyle)` and ignore `taskbarFollowsStyle`, so they can show for a taskbar set to "KDE Plasma (default)".
4. Long pages: Taskbar (38 rows, 12 group alone about 17 rows, plus window frame group, gradient, texture) and Layout (32 rows, 3 unrelated blocks: tiles/list, hidden apps, filtered folders form) are the longest; Style has the card picker plus about 20 form rows and the reset/backup block.
5. Odd placement: Style holds menu size, position, height, scaling and "centre taskbar icons" (layout/menu-geometry items) while "Layout" holds tiles and the apps list; Window frame options (opacity, blur, radius, title bar height, picture) are on Taskbar though windows are picked on Style; the XP Firefox title bar option is on Taskbar; Rail page holds the power button and power menu under the name "Shortcuts"; Icons page mixes the system icon theme, XP icons, and wallpapers (wallpaper is not an icon); Start Button "Right click menu" lives apart from the Shortcuts page items; Tile settings are split between Layout (columns, gap, icon size, caption, lock) and Look (colour, background, image).
6. Immediate (no Apply) controls mixed with saved ones, with no marking: Layout "Edit menu layout…", Icons icon theme combo and "Set as wallpaper", Taskbar "Edit panel…"; Style Reset/Backup/Restore only act after pressing OK/Apply.
7. Unclear or inconsistent names: "Section dividers:" labels the "Edit menu layout…" button; "Look" (menu colours/fonts/texture) vs "Colours" (mode/accent) vs Style's "Look:" (Classic/Modern); page "Layout" vs "Taskbar layout"; "Start11 button image" and "Arch Linux (default)" are hard-coded names; Start Button "Colours:" has two unlabelled colour buttons; Taskbar corner spin and the Square-Round slider have no field label; Colours switches are keyed `win10*` but shown on all non-XP styles; Colours message refers to a "Start Menu page" that no longer exists (now Style); label styles mix questions ("What size should it be?", "Decide how shortcut icons look:") and nouns ("Row height:").
8. Four different "corner" controls with different units: `cornerRadius` (menu, px spin), `windowRadius` (frames, px spin with opt-in check), `taskbarRadius` (custom taskbar, px spin 2..24 though xml allows 2..40), `taskbarRound` (12 bar, 0..100 slider). Three "transparency" controls: Look (`menuOpacity`/`solidMenu`), Colours (`win10Transparency`), Taskbar 12 (`win12Transparency`).
9. Style-specific options are mostly marked only in label text, not hidden: Layout "(Windows 7, XP)" x3, "(Windows 11 style)"; Shortcuts "(Windows 10 style)", "(Windows 7, XP, Pro styles)", "(Windows 7 style)"; all stay visible and enabled for every style. Only the XP substyle/colour, 7/Modern look and substyle, XP start button colour, 12 sections, and Firefox XP option are actually hidden by style.
10. Contradictory text on Taskbar: the closing label says "Panel height, position, floating, auto-hide and task grouping are handled by Plasma itself" while the page has "Floating bar" and "Auto-hide:" controls. Taskbar page header comment mentions "T29-T35" keys but several tuning groups are 12-only (win12Light/Dark/Transparency/Blur/Noise) with no equivalent for other looks.
11. Dependent controls with no visible reason: Taskbar "Task buttons:", "Override the taskbar colour" and "Custom taskbar:" are disabled whenever `taskbarFollowsStyle` is on (the default), while "Gradient:", "Texture:" and "Paint the active button less transparent" are enabled in that case; the 12 "Bar colour:" row is therefore normally greyed out. `searchRunners` "Use everything KRunner can search" unchecking silently pre-selects 3 runners.
12. `Layout` page: "Fetch the favicon for pinned web links" and "Hide folders in the recent list (Windows 11 style)" are in the Apps list block but are not disabled by "Hide the apps list", unlike the neighbouring checks.

# Win12 Explorer: window top, right-click menu and tag fixes (design)

Date: 2026-10-24. Builds on `2026-10-23-win12-explorer-design.md` (phases 1-4 merged) and the
`small-items` branch (Dolphin ▸ Win12 menu, follow-up fixes). Mockup:
`.superpowers/brainstorm/874519-1792856575/content/explorer-layout-v3.html` (git-ignored scratch;
the description below is the authority).

## Intent

The user tried Win12 Explorer and wants it tighter and closer to Windows Explorer: less vertical
chrome above the files, a right-click menu that has the useful Dolphin entries without doubles,
and tagging from the right-click menu without a separate window.

## Goals

1. **Ribbon tabs in the title bar.** Win12 Explorer draws its own title bar (client-side
   decoration). Left to right: the app icon, the ribbon tabs `File`, `Home`, `View`, `Advanced`,
   an empty drag area, then minimise / maximise / close. The window title text
   ("Downloads – Win12 Explorer") is not drawn. The bar has the KWin title-bar colour (red with
   the user's current Win12 setup). Unselected tabs are that colour; the selected tab has the
   ribbon body's colour (black now) and is shorter than the bar, so a strip of title-bar colour
   stays above it. `File` keeps its own look (it opens the backstage, as today). The ribbon body
   sits directly under this bar; the old ribbon tab row is gone.
2. **No folder tabs.** The tab strip is removed (split view covers two folders). Under the
   ribbon there is one line: back, forward, recent (˅), up, the address bar, refresh, search.
   The file view starts right under it.
   - Middle-click on a folder opens it in a new window (was: new tab).
   - Ctrl+T and every "new tab" action go away; "Open in New Tab" is not offered anywhere.
   - At start the window restores its last folder and the split view state (was: all tabs).
3. **Share folds into Advanced.** The Share ribbon tab is removed. Its buttons (Share, Email,
   Zip, Print, Open terminal, Advanced security) become the last group of the Advanced tab, at
   the far right. Ribbon tabs are now File, Home, View, Advanced.
4. **Right-click menu.**
   - Add from Dolphin's menu exactly these: Open in New Window, Open in Split View, Create New ›,
     Copy Location (Ctrl+Alt+C), Open Terminal Here (Alt+Shift+F4), Compress ›, and
     **Extract ›** directly under Compress (shown for archive files).
   - Each function appears once: an entry Win12 Explorer already has (Open, Open With, Cut, Copy,
     Paste, Rename, Move to Trash, Properties, View/Sort/Group, Refresh, Tags) is not added again.
   - Not shown: Dolphin's Assign Tags, the folder-colour row, Activities, Open in New Tab,
     Duplicate Here, Add to Places.
   - The **Win12 ›** submenu is removed from Win12 Explorer's right-click menu (Select by rule,
     Rename many and My rules stay on the Advanced tab). Dolphin's own right-click ▸ Win12 menu
     (small-items branch) stays.
   - Other apps' service menus that are not listed above keep showing, except Win12's own
     service menus (which would double the removed Win12 submenu) — builder: filter them out.
5. **Tags as a slide-out.** Right-click ▸ Tags opens a submenu panel, not a dialog:
   - "Your tags": the existing Win12 tags as checkable rows (colour swatch + word); ticking adds,
     unticking removes, with the 3-tag limit greying as today.
   - "New tag": an optional name field and the 48-colour grid (same order and colours as the
     current New Tag dialog). Clicking a colour adds the tag (colour + typed word, or colour only)
     to the selected files and closes the menu.
   - The ribbon's Tags button may keep using the same panel (builder's call), but no separate
     New Tag window opens from the right-click path.
6. **No "No tag" header.** When the view is grouped by tag name or tag colour, untagged files show
   at the end with no group header (no "No tag" label, no line).

## Non-goals

- No change to Dolphin itself (spec non-goal stands); only our own service-menu files.
- No new tab-like feature in another form (no tab overview, no "recent windows" list).
- No change to the ribbon's Home and View contents beyond what is listed.

## Anti-goals (a failure even if it works)

- The window can no longer be moved, resized, maximised by double-click, snapped, or closed the
  usual ways once it draws its own title bar.
- The title bar colour does not follow the user's Win12/KWin colour (e.g. stays red after the
  user picks another colour).
- A right-click entry appears twice, or Assign Tags / Activities / folder colours sneak back in.
- Tagging from the right-click menu opens a separate window.

## Constraints

- Same stack and rules as the main spec: Qt 6 / KF6, our own code in `explorer/`, tests in
  `explorer/autotests`, `install.sh` builds the package, translations through `i18n()`.
- Wayland and X11: window move/resize must use `QWindow::startSystemMove()` /
  `startSystemResize()` (works on both).
- The spec's earlier non-goal "no quick-access toolbar in the title bar (KWin draws the title
  bar)" is superseded for the ribbon tabs only: the user asked for tabs in the title bar; still no
  quick-access toolbar.

## Decided how

- Client-side title bar inside Win12 Explorer's window (frameless window + our own bar widget);
  the colours come from KWin's colour source (`kdeglobals` `[WM] activeBackground`,
  `activeForeground`, `inactiveBackground`, `inactiveForeground`), re-read when KDE announces a
  colour change.
- Folder tabs removed, not hidden: the code and tests for tabs go away.
- Dolphin entries come from KIO (`KFileItemActions` services/plugins, `KNewFileMenu` for
  Create New) filtered by the lists above, not re-implemented.

## Left to the builder

- Title bar height (match the KWin title bar the user has today, or a fixed Windows-like 32 px),
  how much title-bar colour shows above the selected tab, button glyphs and hover colours.
- Resize borders/corners for the frameless window and the double-click-to-maximise behaviour.
- How Extract is sourced (Ark's service menu / KFileItemActions plugin) and how the filter
  matches entries (desktop file names are steadier than translated texts).
- Whether the ribbon Tags button reuses the slide-out panel.
- Migration of saved settings that mention tabs (old session entries are ignored, not an error).

## Calls made (designer's calls, after the builder check)

- **Windows and session:** Win12 Explorer stays one process with several `ExplorerWindow`s
  (KDBusService Unique, as today). "Open in New Window" and middle-click make another window in
  that process. At start, the saved session is the last-closed window's folder + split state
  (today's "last-closed window wins" rule, minus the tab list).
- **Extract:** comes from Ark's service menu / KIO plugin. Without Ark it is not shown (no own
  extractor); the package lists `ark` as an optional dependency.
- **Win12's own service menus** = the fixed list of files this repo installs or writes:
  `win12-servicemenu.desktop` and `win12-my-rules.desktop`, matched by desktop file name.
- **Tags panel:** first choice is a real submenu (`QWidgetAction`). If typing in its name field is
  unreliable on Wayland, a popup panel anchored to the Tags entry is acceptable — it must open
  beside the menu like a submenu and close with it, never as a separate window with a title bar.
- **Colours:** `kdeglobals [WM]` keys first; when missing, the colour scheme's Header colours
  (`KColorScheme(…, KColorScheme::Header)`). An inactive window uses the inactive colours, as
  KWin does.
- **User decision (2026-10-24):** the frameless window is accepted. It loses KWin's shadow, rounded
  corners and the title-bar window menu; move, resize, maximise (incl. double-click), snap and
  close must keep working. Win12 Explorer draws a simple shadow of its own (builder's call how).

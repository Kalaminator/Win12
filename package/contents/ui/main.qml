pragma ComponentBehavior: Bound

import QtQuick
import QtCore
import QtQuick.Layouts
import QtQuick.Dialogs as Dialogs
import QtQuick.Controls as QQC2
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.private.kicker as Kicker
import org.kde.plasma.plasma5support as P5Support
import org.kde.kirigami as Kirigami
import org.kde.kitemmodels as KItemModels
import org.kde.iconthemes as KIconThemes
import "code/tiles.js" as Tiles

// Root is a ContainmentItem (metadata CustomEmbedded) so real KDE widgets can live in tiles (applet|<id>).
ContainmentItem {
    id: root

    readonly property var cfg: Plasmoid.configuration

    // Styles that have a QML file. The others fall back to Windows 10 for now.
    readonly property var builtStyles: ["windows7", "modern", "windows10", "windows11", "windows12", "app", "pro", "launcher", "xp", "plasma"]
    // "plasma" = the KDE Plasma card: a Kickoff-like menu, Plasma's own taskbar and title bars.
    // Modern style = the Windows 7 file with a flat look.
    // T43 mix and match: menuStyle = the Style card; menuLook/taskbarLook/windowLook ("" = same as the style)
    // pick another look for the menu, the taskbar preset and the window decoration.
    readonly property string menuKind: cfg.menuLook || cfg.menuStyle
    readonly property string taskbarKind: cfg.taskbarLook || cfg.menuStyle
    readonly property string styleFile: menuKind === "modern" ? "windows7"
                                      : builtStyles.includes(menuKind) ? menuKind : "windows10"

    property string searchText: ""

    // Design mode (T40, not saved): dividers between menu sections can be dragged (common/SplitHandle).
    // The menu stays open on outside clicks; closing it ends design mode.
    property bool designMode: false
    // Divider widths: `name` = "<style>" or "<style>/<part>"; stored unscaled, returned scaled.
    function splitFor(name, def) {
        const w = Tiles.splitGet(cfg.splits, name);
        return w === null ? def : Math.round(w * cfg.uiScale);
    }
    function setSplit(name, w) {
        cfg.splits = Tiles.splitSet(cfg.splits, name, w / cfg.uiScale);
    }
    function editLayout() {
        designMode = true;
        menuOpen = true;
    }
    Connections {
        target: root.cfg
        function onDesignRequestChanged() { if (root.cfg.designRequest) root.editLayout(); }
    }
    readonly property bool searching: searchText.length > 0
    property Item kbCurrent: null   // keyboard focus item (common/KeyNav), drawn by common/KeyFrame

    // ---- Pinned apps ----
    // Windows 10 style pins to tile groups; the grid styles (Windows 11, App, Pro) to a flat list.
    readonly property bool usesTiles: styleFile === "windows10"
    // Tile pages: `groups` is the shown page (cfg.tilePage); saveGroups writes that page back.
    readonly property var pages: Tiles.parsePages(cfg.tileGroups)
    readonly property int tilePage: Math.max(0, Math.min(cfg.tilePage, pages.length - 1))
    readonly property var groups: pages[tilePage].groups
    function savePages(p) {
        cfg.tileGroups = JSON.stringify({ pages: p });
    }
    function saveGroups(g) {
        savePages(Tiles.setPage(pages, tilePage, g));
    }
    function isPinned(id) {
        return usesTiles ? pages.some(p => Tiles.findTile(p.groups, id) !== null) : Tiles.allPins(cfg.pinnedGrid).includes(id);
    }
    // Hidden apps (key hiddenApps, desktop ids): left out of every app list and search.
    // ponytail: Kicker.RootModel has no hiddenEntries property in 6.7, so the lists filter.
    function isHidden(id) {
        return !!id && cfg.hiddenApps.includes(String(id).replace(/^applications:/, ""));
    }
    function hideApp(id) {
        if (id && !isHidden(id)) cfg.hiddenApps = cfg.hiddenApps.concat([String(id).replace(/^applications:/, "")]);
    }
    // Renamed pins (key pinnedCaptions, JSON id -> name); capOf returns "" when not renamed.
    function capOf(id) {
        try { return JSON.parse(cfg.pinnedCaptions)[id] || ""; } catch (e) { return ""; }
    }
    function setCap(id, name) {
        let m = {};
        try { m = JSON.parse(cfg.pinnedCaptions); } catch (e) {}
        if (name) m[id] = name; else delete m[id];
        cfg.pinnedCaptions = JSON.stringify(m);
    }
    function renamePin(id, current) {
        close();
        renameDialog.apply = null;
        renameDialog.pinId = id;
        renameField.text = capOf(id) || current || "";
        renameDialog.show();
    }
    // Same dialog for any name (canvas tabs/groups): fn(newName) runs on OK.
    function askName(current, fn) {
        close();
        renameDialog.apply = fn;
        renameField.text = current || "";
        renameDialog.show();
    }
    function pin(id) {
        if (!id || isPinned(id)) return;
        if (!usesTiles) {
            cfg.pinnedGrid = cfg.pinnedGrid.concat([id]);
            return;
        }
        // New pins go to the end of the last group, like Windows.
        const g = Math.max(0, groups.length - 1);
        saveGroups(Tiles.place(groups, id, g, groups.length ? groups[g].tiles.length : 0));
    }
    function unpin(id) {
        if (Tiles.isApplet(id)) {
            const a = appletFor(id);
            removedApplets.push(Number(String(id).slice(7)));   // "remove" only marks it transient (undo); a twin may stay
            if (a) a.internalAction("remove").trigger();
        }
        if (usesTiles) {
            // Folder ids repeat across pages: a folder only goes on the shown page.
            savePages(Tiles.isFolder(id) ? Tiles.setPage(pages, tilePage, Tiles.remove(groups, id))
                                         : pages.map(p => ({ groups: Tiles.remove(p.groups, id) })));
        } else {
            cfg.pinnedGrid = Tiles.removePin(cfg.pinnedGrid, id);
        }
    }
    // Pins whose app is not installed (e.g. the default Konsole pin) are hidden, not removed.
    // ponytail: one model over all pins (as windows7.qml); if a pin id is also rewritten
    // (preferred://browser) while another app is missing, the rewritten one may hide too.
    Kicker.SimpleFavoritesModel {
        id: pinCheck
        favorites: Tiles.allPins(cfg.pinnedGrid).filter(id => !Tiles.custom(id))
    }
    readonly property var missingPins: {
        const apps = Tiles.allPins(cfg.pinnedGrid).filter(id => !Tiles.custom(id)); // not pinCheck.favorites: the model drops missing ids there
        if (pinCheck.count >= apps.length) return [];
        const role = pinCheck.KItemModels.KRoleNames.role("favoriteId");
        const have = [];
        for (let r = 0; r < pinCheck.count; r++)
            have.push((pinCheck.data(pinCheck.index(r, 0), role) || "").replace(/^applications:/, ""));
        return apps.filter(id => !have.includes(id));
    }
    readonly property var shownPins: cfg.pinnedGrid.filter(id => !missingPins.includes(id))
    function movePinned(id, index) {
        // Out of a folder: unpin it there first (a dissolving folder keeps its place).
        const list = cfg.pinnedGrid.includes(id) ? cfg.pinnedGrid : Tiles.removePin(cfg.pinnedGrid, id);
        cfg.pinnedGrid = Tiles.placeInList(list, id, index);
    }
    // Pinned grid folders (Windows 11): drop id on entry target; rename; unpin = its apps back in place.
    function folderPins(id, target) {
        cfg.pinnedGrid = Tiles.placeInFolder(cfg.pinnedGrid, id, target, i18n("Folder"));
    }
    function renameFolder(entry, name) {
        const f = Tiles.folderOf(entry);
        if (f) cfg.pinnedGrid = cfg.pinnedGrid.map(e => e === entry ? Tiles.folderEntry(name, f.ids) : e);
    }
    function unfoldFolder(entry) {
        const f = Tiles.folderOf(entry), i = cfg.pinnedGrid.indexOf(entry);
        if (f && i >= 0) cfg.pinnedGrid = cfg.pinnedGrid.slice(0, i).concat(f.ids, cfg.pinnedGrid.slice(i + 1));
    }

    // ---- Models from KDE's own launcher library ----
    readonly property Kicker.RootModel rootModel: Kicker.RootModel {
        autoPopulate: false
        appletInterface: root
        flat: true
        sorted: true
        showSeparators: false
        showTopLevelItems: true
        showAllApps: true
        showAllAppsCategorized: false
        showRecentApps: false
        showRecentDocs: false
        showPowerSession: false
        showFavoritesPlaceholder: true
        highlightNewlyInstalledApps: true
        Component.onCompleted: {
            favoritesModel.initForClient("org.start12.menu.favorites.instance-" /* ponytail: KAStats client id, kept so stored favourites survive the rename */ + Plasmoid.id);
            root.cfg.favoritesPortedToKAstats = true;
        }
    }

    readonly property Kicker.RunnerModel runnerModel: Kicker.RunnerModel {
        appletInterface: root
        query: root.searchText
        mergeResults: !root.cfg.groupResults
        // Plasma 6.7: an empty list means no runners at all, so "empty = default" is spelled out here.
        runners: root.cfg.searchRunners.length ? root.cfg.searchRunners
            : ["krunner_services", "krunner_systemsettings", "baloosearch", "krunner_recentdocuments",
               "krunner_placesrunner", "locations", "calculator", "unitconverter", "krunner_sessions",
               "krunner_powerdevil", "krunner_bookmarksrunner"]
        favoritesModel: root.rootModel.favoritesModel
    }

    // Created only after rootModel.refresh(): SystemModel crashes on a null favoritesModel.
    property Kicker.SystemModel systemModel: null
    Component {
        id: systemModelComponent
        Kicker.SystemModel {}
    }

    readonly property Kicker.RecentUsageModel frequentAppsModel: Kicker.RecentUsageModel {
        favoritesModel: root.rootModel.favoritesModel
        shownItems: Kicker.RecentUsageModel.OnlyApps
        ordering: 1 // most used
    }

    Kicker.ProcessRunner {
        id: processRunner
    }

    // Run a shell command (systemsettings, dolphin, ...).
    P5Support.DataSource {
        id: executable
        engine: "executable"
        connectedSources: []
        onNewData: sourceName => disconnectSource(sourceName)
    }
    function run(cmd) {
        executable.connectSource(cmd);
    }
    // XP taskbar: with the XP style, the Plasma Style follows substyle + colour scheme (made by
    // code/xptheme.py: install.sh writes blue/olive/silver; a custom colour is generated here under a
    // new name per colour, so Plasma's svg cache never shows a stale one). Leaving XP restores the theme
    // saved in savedDesktopTheme. Reads the current theme first so nothing is applied when already right.
    // ponytail: two Win12 widgets with different styles would fight over the theme; one menu is the norm.
    readonly property string xpCustomHex: cfg.xpCustomColor.toString().slice(-6)
    readonly property string xpScheme: "win12-xp-" + cfg.xpSubstyle + (cfg.xpColor === "olive" || cfg.xpColor === "silver" ? "-" + cfg.xpColor
                                       : cfg.xpColor === "custom" ? "-custom-" + xpCustomHex : "")
    readonly property string xpTheme: taskbarKind !== "xp" || !cfg.xpTaskbarTheme ? "" : xpScheme
    onXpThemeChanged: syncDesktopTheme()
    // Window decorations (T26a, key windowTheme): Aurorae theme per menu style, and (T26b) the colour scheme of the
    // same name. From xptheme.py --deco / --scheme (install.sh; XP custom colour made here). T37: windows10/11
    // light or dark like the user's own scheme (userDark), windows7 light, modern/app/pro/launcher = Win11 look.
    function windowDecoration() {
        const s = cfg.windowLook || cfg.menuStyle;
        // T39 Colours page: every non-XP style follows light/dark (winDark) and "accent on title bars" (-accent-hex).
        const acc = cfg.win10AccentTitles ? "-accent-" + accentHex : "";
        return s === "xp" ? xpScheme : s === "windows7" ? "win12-win7" + acc
            : "win12-" + (s === "windows10" ? "win10-" : "win11-") + (winDark ? "dark" : "light") + acc;
    }
    // T38/T39 Colours page: light/dark from colorMode (0 Plasma = userDark, 1 dark, 2 light, 3 custom menu colour),
    // accent = the menu's accent (Plasma or custom).
    property var userAccent: ""   // read with userDark (readBrightness)
    readonly property string accentHex: (cfg.accentMode === 1 ? cfg.accentColor
                                         : userAccent !== "" ? userAccent : Kirigami.Theme.highlightColor).toString().slice(-6)
    readonly property bool winDark: cfg.colorMode === 1 || (cfg.colorMode === 0 && userDark)
        || (cfg.colorMode === 3 && Kirigami.ColorUtils.brightnessForColor(cfg.menuColor) === Kirigami.ColorUtils.Dark)
    // xptheme.py args for a deco/scheme made at apply time (XP custom colour, Win10 accent title bar); "" = shipped one.
    function customArgs(name) {
        return name.includes("-custom-") ? "--custom " + xpCustomHex + " --sub " + cfg.xpSubstyle
            : name.includes("-accent-") ? "--custom " + accentHex + " --win10 " + (winDark ? "dark" : "light") + " --kind " + name.split("-")[1] : "";
    }
    readonly property string customNames: "\\( -name 'win12-xp-*-custom-*' -o -name 'win12-win*-accent-*' -o -name 'win12-*-g[0-9]*b[01]r*t[0-9]*' \\)"
    readonly property string windowDeco: cfg.windowTheme && (cfg.windowLook || cfg.menuStyle) !== "plasma" ? windowDecoration() : ""
    // T41 window glass (Taskbar page): opacity/blur/radius/title height = a deco made at apply time,
    // <windowDeco>-g<opacity>b<blur>r<radius|d>t<title>; the colour scheme keeps the windowDeco name.
    // W8: Windows 12 windows default to a glass title bar (35 %, blurred; 65 % still read as solid accent behind the blur) while "Transparency effects" is on.
    readonly property int windowOpacity: cfg.windowOpacity === 100 && (cfg.windowLook || cfg.menuStyle) === "windows12"
        && cfg.win10Transparency ? (cfg.win12Transparency < 0 ? 35 : 100 - cfg.win12Transparency) : cfg.windowOpacity
    // W14 title bar picture: xptheme.py --image..., the md5 makes a new deco per change
    readonly property string windowImageArgs: cfg.windowImage === "" ? "" : " --image " + Tiles.shQuote(cfg.windowImage)
        + " --imagemode " + (cfg.windowImageMode === "tile" ? "tile" : "stretch") + " --imagealpha " + cfg.windowImageAlpha
        + (cfg.windowImageTint ? " --tint " + cfg.accentColor.toString().slice(-6) : "")
    readonly property string glassSuffix: windowOpacity < 100 || cfg.windowRadius >= 0 || cfg.titleHeight > 0 || windowImageArgs !== ""
        ? "-g" + windowOpacity + "b" + (cfg.windowBlur ? 1 : 0) + "r" + (cfg.windowRadius < 0 ? "d" : cfg.windowRadius) + "t" + cfg.titleHeight
          + (windowImageArgs === "" ? "" : "i" + Qt.md5(windowImageArgs).slice(0, 8)) : ""
    readonly property string decoName: windowDeco === "" ? "" : windowDeco + glassSuffix
    readonly property bool wantBlur: windowDeco !== "" && windowOpacity < 100 && cfg.windowBlur
        || win12Taskbar && cfg.win10Transparency
    onGlassSuffixChanged: syncWindowDeco()
    onWantBlurChanged: syncWindowDeco()
    onWindowDecoChanged: readBrightness()
    // Brightness of the user's own colour scheme (the saved one while ours is applied), then both syncs.
    property bool userDark: false
    function readBrightness() {
        if (!configReady) return;   // never act on default settings before migrateConfig is done
        brightReader.connectSource("S=" + Tiles.shQuote(cfg.savedColorScheme) + "; f=kdeglobals; "
            + "case \"$(kreadconfig6 --file kdeglobals --group General --key ColorScheme)\" in win12-*|start12-*) "
            + "f=$(ls \"$HOME/.local/share/color-schemes/$S.colors\" \"/usr/share/color-schemes/$S.colors\" 2>/dev/null | head -1);; esac; "
            + "kreadconfig6 --file \"${f:-kdeglobals}\" --group Colors:Window --key BackgroundNormal; "
            + "kreadconfig6 --file kdeglobals --group General --key AccentColor; "
            + "kreadconfig6 --file \"${f:-kdeglobals}\" --group Colors:Selection --key BackgroundNormal");
    }
    P5Support.DataSource {
        id: brightReader
        engine: "executable"
        connectedSources: []
        onNewData: (sourceName, data) => {
            disconnectSource(sourceName);
            const lines = (data.stdout || "").split("\n");
            const v = (lines[0] || "").trim().split(",").map(Number);
            root.userDark = v.length === 3 && 0.299 * v[0] + 0.587 * v[1] + 0.114 * v[2] < 128;
            // The user's own accent (Plasma accent colour, else their scheme's selection): Kirigami's highlight
            // follows whatever scheme/style Win12 applied, so it can't be used for "Automatic".
            const a = ((lines[1] || "").trim() || (lines[2] || "").trim()).split(",").map(Number);
            root.userAccent = a.length >= 3 && a.every(n => n >= 0 && n <= 255) ? Qt.rgba(a[0] / 255, a[1] / 255, a[2] / 255, 1) : "";
            root.syncWindowDeco();
            root.syncWindowColors();
        }
    }
    // Same save/restore idea as syncDesktopTheme: kwinrc [org.kde.kdecoration2] library + theme are saved in
    // savedDecoLibrary/savedDecoTheme when ours goes on and written back when it goes off.
    function syncWindowDeco() {
        if (!configReady) return;   // never act on default settings before migrateConfig is done
        const rd = key => "\"$(kreadconfig6 --file kwinrc --group org.kde.kdecoration2 --key " + key + ")\"";
        decoReader.connectSource("printf '%s|%s|%s' " + rd("library") + " " + rd("theme")
            + " \"$(kreadconfig6 --file kwinrc --group Plugins --key blurEnabled)\"");
    }
    P5Support.DataSource {
        id: decoReader
        engine: "executable"
        connectedSources: []
        onNewData: (sourceName, data) => {
            disconnectSource(sourceName);
            const parts = (data.stdout || "").split("|");
            const lib = parts[0].trim(), cur = (parts[1] || "").trim(), blur = (parts[2] || "").trim();
            // T41: KWin's blur effect on for the glass frame; the user's blurEnabled is saved in savedBlur ("unset" =
            // no key) and written back when our blur goes off.
            const plug = "kwriteconfig6 --file kwinrc --group Plugins --key blurEnabled ";
            const fx = f => " && " + Tiles.QDBUS + " org.kde.KWin /Effects org.kde.kwin.Effects." + f + " blur";
            if (root.wantBlur && root.cfg.savedBlur === "") {
                root.cfg.savedBlur = blur || "unset";
                root.run("sh -c " + Tiles.shQuote(plug + "true" + fx("loadEffect")));
            } else if (!root.wantBlur && root.cfg.savedBlur !== "") {
                const old = root.cfg.savedBlur;
                root.cfg.savedBlur = "";
                root.run("sh -c " + Tiles.shQuote(old === "unset" ? plug + "--delete" : plug + Tiles.shQuote(old)
                         + (old === "false" ? fx("unloadEffect") : "")));
            }
            const ours = /^__aurorae__svg__(win|start)12-/.test(cur);
            const kw = (key, v) => v === "" ? "kwriteconfig6 --file kwinrc --group org.kde.kdecoration2 --key " + key + " --delete"
                : "kwriteconfig6 --file kwinrc --group org.kde.kdecoration2 --key " + key + " " + Tiles.shQuote(v);
            const reconf = " && " + Tiles.QDBUS + " org.kde.KWin /KWin reconfigure";
            const dir = "$HOME/.local/share/aurorae/themes/";
            if (root.windowDeco !== "") {
                const want = "__aurorae__svg__" + root.decoName;
                if (cur === want && lib === "org.kde.kwin.aurorae")
                    return;
                if (!ours) {
                    root.cfg.savedDecoLibrary = lib;
                    root.cfg.savedDecoTheme = cur;
                }
                const gen = decodeURIComponent(Qt.resolvedUrl("code/xptheme.py").toString().replace("file://", ""));
                const make = root.customArgs(root.windowDeco) === "" && root.glassSuffix === "" ? "true"
                    : "python3 " + Tiles.shQuote(gen) + " --deco --name " + root.windowDeco + " --opacity " + root.windowOpacity
                      + " --blur " + (root.cfg.windowBlur ? 1 : 0) + " --radius " + root.cfg.windowRadius + " --title " + root.cfg.titleHeight
                      + root.windowImageArgs + " --out " + dir + root.decoName;
                root.run("sh -c " + Tiles.shQuote(make + " && " + kw("library", "org.kde.kwin.aurorae") + " && " + kw("theme", want) + reconf
                         + " && find " + dir + " -maxdepth 1 " + root.customNames + " ! -name " + root.decoName + " -exec rm -rf {} +"));
            } else if (ours) {
                root.run("sh -c " + Tiles.shQuote(kw("library", root.cfg.savedDecoLibrary) + " && " + kw("theme", root.cfg.savedDecoTheme) + reconf));
            }
        }
    }
    // Window colours (T26b, same windowTheme key and name as windowDeco). Same save/restore idea: kdeglobals [General] ColorScheme is saved in savedColorScheme when ours goes on.
    function syncWindowColors() {
        if (!configReady) return;   // never act on default settings before migrateConfig is done
        colorReader.connectSource("kreadconfig6 --file kdeglobals --group General --key ColorScheme");
    }
    P5Support.DataSource {
        id: colorReader
        engine: "executable"
        connectedSources: []
        onNewData: (sourceName, data) => {
            disconnectSource(sourceName);
            const cur = (data.stdout || "").trim();
            const ours = /^(win|start)12-/.test(cur);
            const want = root.windowDeco;
            if (want !== "") {
                if (cur === want)
                    return;
                if (!ours)
                    root.cfg.savedColorScheme = cur;
                const dir = "$HOME/.local/share/color-schemes/";
                const gen = decodeURIComponent(Qt.resolvedUrl("code/xptheme.py").toString().replace("file://", ""));
                const make = root.customArgs(want) === "" ? "true"
                    : "python3 " + Tiles.shQuote(gen) + " --scheme " + root.customArgs(want) + " --out " + dir + want + ".colors";
                root.run("sh -c " + Tiles.shQuote(make + " && plasma-apply-colorscheme " + want
                         + " && find " + dir + " -maxdepth 1 " + root.customNames + " ! -name " + want + ".colors -delete"));
            } else if (ours) {
                root.run("plasma-apply-colorscheme " + Tiles.shQuote(root.cfg.savedColorScheme || "BreezeLight"));
            }
        }
    }
    // Generated taskbar for any style (T31, xptheme.py --taskbar): win12-taskbar-<hash of args + fallback>,
    // FallbackTheme = the user's saved style, so only panel/tasks/colors change. XP taskbar wins.
    // T35: taskbarFollowsStyle picks a preset per menu style instead of the custom keys (T37: others = windows11, xp = none).
    // T34: button style (buttons=plasma|win10|win11|win7), accent for the Win10/11 active bar, solid = focused
    // item less transparent; added to presets and custom alike.
    // T38/T39 the presets follow the Colours page (light/dark, accent taskbar, transparency); windows7 = accent-tinted glass.
    readonly property var taskbarPresets: ({
        "windows10": "colour=" + (cfg.win10AccentStart ? accentHex : winDark ? "101010" : "eeeeee")
                     + " alpha=" + (cfg.win10Transparency ? 0.85 : 1) + " radius=2 line=0 buttons=win10",
        "windows11": "colour=" + (cfg.win10AccentStart ? accentHex : winDark ? "202020" : "f3f3f3")
                     + " alpha=" + (cfg.win10Transparency ? 0.8 : 1) + " radius=8 line=0 buttons=win11",
        "windows7": "colour=" + (cfg.win10AccentStart ? accentHex : "2a5a96")
                    + " alpha=" + (cfg.win10Transparency ? 0.6 : 1) + " radius=6 line=1 buttons=win7",
        "windows12": "colour=" + win12Hex
                     + " alpha=" + (!cfg.win10Transparency ? 1 : cfg.win12Transparency >= 0 ? 1 - cfg.win12Transparency / 100
                                    : cfg.win10AccentStart ? 0.15 : winDark ? 0.3 : 0.25) + " radius=" + win12Radius + " line=0 buttons=win11 gap=" + cfg.taskbarGap
                     + (win12GapMode && cfg.taskbarGap > 0 ? " ring=" + cfg.taskbarGap + " size=" + panelWidth + "x" + panelHeight : "") })
    // W11 Windows 12 colour sliders (-1 = the W8 look): light 0 soft tint .. 100 near white (80 = f7f7fb),
    // dark 0 soft dark .. 100 near black (~50 = 202028); taskbar and menu (Metrics.bg) background.
    // With the accent colour on, the sliders mix the accent towards white (light) or black (dark) instead.
    readonly property string win12Hex: {
        const v = winDark ? cfg.win12Dark : cfg.win12Light;
        if (cfg.win10AccentStart)
            return v < 0 ? accentHex : Kirigami.ColorUtils.linearInterpolation("#" + accentHex,
                winDark ? "#050507" : "#fdfdff", v / 100 * 0.9).toString().slice(-6);
        return v < 0 ? (winDark ? "202028" : "f7f7fb") : Kirigami.ColorUtils.linearInterpolation(winDark ? "#3a3a46" : "#dde1ec",
            winDark ? "#050507" : "#fdfdff", v / 100).toString().slice(-6);
    }
    // W11 blur strength + grain: kwinrc [Effect-blur] BlurStrength / NoiseStrength while any Windows 12 part is on
    // (KWin's blur is global; its default noise makes the glass look grainy). The user's values are kept next to
    // them in win12Saved / win12SavedNoise ("unset" = no key) and put back when Windows 12 goes off.
    readonly property bool win12Glass: cfg.win10Transparency && (menuKind === "windows12" || win12Taskbar
        || (cfg.windowLook || cfg.menuStyle) === "windows12")
    // Default 3: KWin's own default smears the background so much that the glass looks opaque.
    readonly property int win12Blur: win12Glass ? (cfg.win12Blur > 0 ? cfg.win12Blur : 3) : -1
    readonly property int win12Noise: win12Glass ? cfg.win12Noise : -1
    // A style switch flips these through several values at once: wait for the final state, then one run under
    // the panel lock, so an older "restore" run cannot reload KWin after the newer one.
    onWin12BlurChanged: blurTimer.restart()
    onWin12NoiseChanged: blurTimer.restart()
    Timer { id: blurTimer; interval: 600; onTriggered: root.syncBlurStrength() }
    function syncBlurStrength() {
        if (!configReady) return;
        const g = "--file kwinrc --group Effect-blur --key ";
        const one = (key, saved, v) => v >= 0
            ? "[ -n \"$(kreadconfig6 " + g + saved + ")\" ] || kwriteconfig6 " + g + saved + " \"$(kreadconfig6 " + g
              + key + " --default unset)\"; kwriteconfig6 " + g + key + " " + v + "; "
            : "s=$(kreadconfig6 " + g + saved + "); if [ -n \"$s\" ]; then if [ \"$s\" = unset ]; then kwriteconfig6 " + g
              + key + " --delete; else kwriteconfig6 " + g + key + " \"$s\"; fi; kwriteconfig6 " + g + saved + " --delete; fi; ";
        run("flock \"${XDG_RUNTIME_DIR:-/tmp}/win12-blur.lock\" sh -c " + Tiles.shQuote(one("BlurStrength", "win12Saved", win12Blur > 0 ? win12Blur : -1)
            + one("NoiseStrength", "win12SavedNoise", win12Noise) + Tiles.QDBUS + " org.kde.KWin /KWin reconfigure; "
            // /KWin reconfigure does not reload a running effect's settings: reload the blur itself too.
            + Tiles.QDBUS + " org.kde.KWin /Effects org.kde.kwin.Effects.reconfigureEffect blur"));
    }
    // W3: corners in % of half the panel thickness (applet size + our theme's 2+2 margins; 46 until laid out).
    readonly property int barThickness: (Plasmoid.formFactor === PlasmaCore.Types.Vertical ? width : height) + 4
    readonly property int win12Radius: Math.max(2, Math.round((barThickness < 20 ? 46 : barThickness) / 2 * cfg.taskbarRound / 100))
    readonly property bool win12Taskbar: cfg.taskbarFollowsStyle && taskbarKind === "windows12"
    readonly property string taskbarBase: cfg.taskbarFollowsStyle ? (taskbarPresets[taskbarKind] || (taskbarKind === "xp" || taskbarKind === "plasma" ? "" : taskbarPresets.windows11))
        : cfg.taskbarTheme !== "custom" ? ""
        : "colour=" + cfg.taskbarColor.toString().slice(-6) + " alpha=" + (cfg.taskbarAlpha / 100)
          + " radius=" + cfg.taskbarRadius + " line=" + (cfg.taskbarLine ? 1 : 0) + " buttons=" + Tiles.shQuote(cfg.taskbarButtons)
    readonly property string taskbarArgs: taskbarBase === "" ? "" : taskbarBase
        // T33 texture, W14 any generated look (path shell-quoted: it ends up in sh -c; the hash makes a new theme per change)
        + (cfg.taskbarImage === "" ? "" : " image=" + Tiles.shQuote(cfg.taskbarImage) + " mode=" + cfg.taskbarImageMode
           + " imagealpha=" + (cfg.taskbarImageAlpha / 100)
           + (cfg.taskbarImageTint ? " tint=" + cfg.accentColor.toString().slice(-6) : ""))
        + (cfg.taskbarGradient === "none" ? "" : " gradient=" + cfg.taskbarGradient + " strength=" + cfg.taskbarGradientStrength)
        + " accent=" + (taskbarKind !== "xp" && cfg.win10AccentStart && cfg.taskbarFollowsStyle
                        ? Qt.lighter("#" + accentHex, 1.6).toString().slice(-6) : cfg.accentColor.toString().slice(-6)) + " solid=" + (cfg.taskbarFocusedSolid ? 1 : 0)
    onTaskbarArgsChanged: syncDesktopTheme()
    readonly property bool firefoxSystemTitle: xpTheme !== "" && cfg.xpFirefoxTitlebar
    onFirefoxSystemTitleChanged: syncDesktopTheme()
    function syncDesktopTheme() {
        if (!configReady) return;   // never act on default settings before migrateConfig is done
        // Firefox draws its own title bar: with an XP theme it gets the system one (user.js, takes effect on its next start).
        root.run("sh " + Tiles.shQuote(decodeURIComponent(Qt.resolvedUrl("code/firefox-titlebar.sh").toString().replace("file://", "")))
                 + (firefoxSystemTitle ? " on" : " off"));
        themeReader.connectSource("kreadconfig6 --file plasmarc --group Theme --key name");
        syncIcons();
    }
    // XP icons (Icons page): with the XP style the icon theme becomes xpIconTheme (empty = the first installed
    // theme with "xp" in its folder or Name), the user's one is saved in savedIconTheme and put back after.
    readonly property bool xpIcons: cfg.menuStyle === "xp" && cfg.xpIcons
    onXpIconsChanged: syncIcons()
    readonly property string xpIconTheme: cfg.xpIconTheme
    onXpIconThemeChanged: syncIcons()
    function syncIcons() {
        if (!configReady) return;
        iconReader.connectSource("sh -c " + Tiles.shQuote("printf '%s|' \"$(kreadconfig6 --file kdeglobals --group Icons --key Theme)\"; "
            + "for d in \"$HOME\"/.local/share/icons/* /usr/share/icons/*; do grep -qs ^Directories= \"$d/index.theme\" || continue; "
            + "case \"$(basename \"$d\") $(grep -m1 ^Name= \"$d/index.theme\")\" in *[xX][pP]*) basename \"$d\"; break;; esac; done"));
    }
    P5Support.DataSource {
        id: iconReader
        engine: "executable"
        connectedSources: []
        onNewData: (sourceName, data) => {
            disconnectSource(sourceName);
            const v = (data.stdout || "").trim().split("|");
            const cur = v[0] || "breeze";
            const want = root.xpIcons ? (root.cfg.xpIconTheme || v[1] || "") : "";
            if (want !== "") {
                if (cur === want) return;
                if (root.cfg.savedIconTheme === "") root.cfg.savedIconTheme = cur;
                root.run("/usr/lib/plasma-changeicons " + Tiles.shQuote(want));
            } else if (root.cfg.savedIconTheme !== "") {
                if (cur !== root.cfg.savedIconTheme) root.run("/usr/lib/plasma-changeicons " + Tiles.shQuote(root.cfg.savedIconTheme));
                root.cfg.savedIconTheme = "";
            }
        }
    }
    P5Support.DataSource {
        id: themeReader
        engine: "executable"
        connectedSources: []
        onNewData: (sourceName, data) => {
            disconnectSource(sourceName);
            const cur = (data.stdout || "").trim() || "default";
            const ours = /^(win|start)12-(xp|taskbar)-/.test(cur);
            const fallback = ours ? (root.cfg.savedDesktopTheme || "default") : cur;
            const want = root.xpTheme !== "" ? root.xpTheme : root.taskbarArgs === "" ? ""
                : "win12-taskbar-" + Qt.md5(root.taskbarArgs + " " + fallback).slice(0, 10);
            if (want !== "") {
                if (cur === want)
                    return;
                if (!ours)
                    root.cfg.savedDesktopTheme = cur;
                if (want !== root.xpTheme) {
                    const tdir = "$HOME/.local/share/plasma/desktoptheme/";
                    const tgen = decodeURIComponent(Qt.resolvedUrl("code/xptheme.py").toString().replace("file://", ""));
                    // Old generated themes go only after the new one is applied (a failed generate keeps the current one).
                    root.run("sh -c " + Tiles.shQuote("python3 " + Tiles.shQuote(tgen)
                             + " --taskbar " + root.taskbarArgs + " fallback=" + Tiles.shQuote(fallback) + " --out " + tdir + want
                             + " && plasma-apply-desktoptheme " + want
                             // W12: a floating panel re-reads the theme's floating margins (bottom spacing) only on a floating change, once the theme has loaded
                             + " && sleep 2 && " + Tiles.QDBUS + " org.kde.plasmashell /PlasmaShell evaluateScript "
                             + Tiles.shQuote("for (const p of panels()) if (p.floating) { p.floating = false; p.floating = true; }")
                             // W15: never the applied one either (two quick runs, e.g. a panel resize, deleted each other's theme)
                             + " && find " + tdir + " -maxdepth 1 -name 'win12-taskbar-*' ! -name " + want
                             + " ! -name \"$(kreadconfig6 --file plasmarc --group Theme --key name)\" -exec rm -rf {} +"));
                } else if (root.xpTheme.includes("-custom-")) {
                    const dir = "$HOME/.local/share/plasma/desktoptheme/";
                    const gen = decodeURIComponent(Qt.resolvedUrl("code/xptheme.py").toString().replace("file://", ""));
                    root.run("sh -c " + Tiles.shQuote("python3 " + Tiles.shQuote(gen) + " --custom "
                             + root.xpCustomHex + " --sub " + root.cfg.xpSubstyle + " --out " + dir + root.xpTheme
                             + " && plasma-apply-desktoptheme " + root.xpTheme
                             + " && find " + dir + " -maxdepth 1 -name 'win12-xp-*-custom-*' ! -name " + root.xpTheme + " -exec rm -rf {} +"));
                } else {
                    root.run("plasma-apply-desktoptheme " + root.xpTheme);
                }
            } else if (ours) {
                root.run("plasma-apply-desktoptheme " + Tiles.shQuote(root.cfg.savedDesktopTheme || "default"));
            }
        }
    }
    function openUrl(url) {
        Qt.openUrlExternally(url);
        root.menuOpen = false;
    }
    // Pinned website/file/folder (Tiles.custom): open its target, or run "cmd:..." targets.
    function openCustom(c) {
        if (c.target.startsWith("cmd:")) {
            run(c.target.slice(4));
            close();
        } else {
            openUrl(c.target);
        }
    }
    // T32: custom pins on the taskbar. Writes win12-pin-<md5>.desktop (Tiles.pinDesktop) and adds/removes its
    // url in the launchers of the Task Manager(s) in this widget's panel (evaluateScript, like "autohide").
    // ponytail: pinned state is our own list (cfg.taskbarPins); a launcher removed by hand on the panel or a
    // renamed pin (new id) leaves a stale entry, harmless. Read the launchers back if that ever matters.
    function taskbarItem(id) {
        if (!Tiles.custom(id)) return [];
        const on = cfg.taskbarPins.indexOf(id) >= 0;
        return [{ text: on ? i18n("Unpin from taskbar") : i18n("Pin to taskbar"), icon: on ? "window-unpin" : "window-pin",
                  run: () => setTaskbarPin(id, !on) }];
    }
    function setTaskbarPin(id, on) {
        const dir = decodeURIComponent(StandardPaths.writableLocation(StandardPaths.GenericDataLocation).toString().replace(/^file:\/\//, "")) + "/applications";
        const name = "win12-pin-" + Qt.md5(id).slice(0, 8) + ".desktop";
        const path = dir + "/" + name;
        const script = "for (const p of panels()) { if (p.widgetIds.indexOf(" + Plasmoid.id + ") < 0) continue; "
            + "for (const w of p.widgets()) { if (!/icontasks|taskmanager/.test(w.type)) continue; w.currentConfigGroup = ['General']; "
            // ponytail: launchers still at the task manager's built-in default read as '' here; writing would
            // drop those defaults, so such a task manager is skipped (pinning works once it has its own list).
            + "const raw = String(w.readConfig('launchers', '')); if (!raw) continue; "
            + "const l = raw.split(',').filter(s => s && s.indexOf(" + JSON.stringify(name) + ") < 0); "
            + (on ? "l.push(" + JSON.stringify("file://" + path) + "); " : "")
            + "w.writeConfig('launchers', l); w.reloadConfig(); } }";
        const dbus = "" + Tiles.QDBUS + " org.kde.plasmashell /PlasmaShell evaluateScript " + Tiles.shQuote(script);
        run("sh -c " + Tiles.shQuote(on
            ? "mkdir -p " + Tiles.shQuote(dir) + " && printf %s " + Tiles.shQuote(Tiles.pinDesktop(Tiles.custom(id))) + " > " + Tiles.shQuote(path) + " && " + dbus
            : dbus + "; rm -f " + Tiles.shQuote(path)));
        cfg.taskbarPins = on ? cfg.taskbarPins.concat([id]) : cfg.taskbarPins.filter(p => p !== id);
    }

    // T42: Menu position + key centerTaskbar = the panel follows the menu (any panel edge, along the panel):
    // Center: two expanding panel spacers marked win12Centre, one right before the Start button and one after
    // the last Task Manager (tasks stop filling the panel: key "fill", old value kept in win12Fill), so Start
    // and the tasks sit centred; the rest stays at the far end.
    // Right: the whole widget order reversed (show desktop, clock, tray first, Start at the far end) with one
    // spacer before the Task Manager so the tasks sit next to Start. The user's order is kept in the panel's
    // win12PanelOrder key and put back when it ends. Off: spacers removed, fill and order put back.
    // One script does it all and is idempotent: it reads the mode from this widget's config, compares the
    // wanted order with the panel's and only touches what differs, printing "done" when nothing was left to do.
    // The shell loop runs it again until "done" (checks the result of the pass before); flock keeps two syncs
    // from interleaving. So the panelMode change at every plasmashell start (config load) is a no-op.
    // Widget.index set = live reorder (AppletOrder writes alone do not). On Win12's patched KDE (kde/) all index
    // sets land in one pass (immediate panel save, Widget.index fixes); the next pass only verifies. A removed widget stays in p.widgets()
    // for undo (its config already reads empty), so removed ids are listed in the panel's win12Gone key
    // and ignored (ids are never reused; the list drops ids no longer in the panel).
    readonly property int panelMode: !cfg.centerTaskbar ? 0 : cfg.menuPosition === 1 ? 1 : cfg.menuPosition === 2 ? 2 : 0
    // Menu contents mirrored: Right position on horizontal panels; on vertical panels the Start column
    // sits next to the panel, so mirrored on the right screen edge only, whatever menuPosition is.
    readonly property bool mirrorMenu: Plasmoid.formFactor === PlasmaCore.Types.Vertical
        ? Plasmoid.location === PlasmaCore.Types.RightEdge : cfg.menuPosition === 2
    readonly property bool centerPanel: panelMode !== 0
    onPanelModeChanged: syncCenterPanel()
    // W3: native panel settings while our taskbar look is on: floating only for Windows 12 (the other styles
    // never float: Plasma de-floats/re-floats with maximized windows and Show Desktop), auto-hide from
    // taskbarHiding. null = leave it. The user's value is saved once in the panel's win12Saved_<prop> key
    // (plasmashellrc is written lazily, so it cannot be the restore source) and put back when null again.
    // W12: every style but KDE Plasma ("plasma" = the user's own panel, never touched): docked unless Windows 12
    // floats; auto-hide Off = "none" (windows maximize up to the docked bar).
    readonly property bool kdeTaskbar: taskbarKind === "plasma"
    // W15 "Windows: keep a gap": the floating single Windows 12 bar becomes a docked, always-visible panel that is
    // 2 x taskbarGap thicker; the ring theme (xptheme ring=/size=, the exact panel size) draws the island inside a
    // transparent ring, so maximized windows stop gap px above it. height {ring} = the saved thickness + ring.
    // ponytail: single bar only; islands keep the fill mode (their magnetic offsets assume the 8 px floating padding).
    readonly property bool win12GapMode: win12Taskbar && cfg.taskbarFloating && cfg.taskbarWindows === 1 && islandWant === "single"
    readonly property int panelHeight: Window.window ? Window.window.height : 0
    readonly property string panelWant: JSON.stringify({
        floating: kdeTaskbar ? null : win12Taskbar && cfg.taskbarFloating && !win12GapMode,
        // W7: the floating Windows 12 bar auto-hides unless the user picks a mode (Plasma keeps a non-normal
        // panel floating; in normal visibility it docks over maximized windows).
        hiding: kdeTaskbar ? null : win12GapMode ? "none" : [win12Taskbar && cfg.taskbarFloating ? "autohide" : "none", "dodgewindows", "autohide"][cfg.taskbarHiding],
        height: win12GapMode ? { ring: 2 * cfg.taskbarGap } : null })
    onPanelWantChanged: { syncPanelLook(); syncIslands(); }
    // W7: KWin effect extras/win12-panelpop (installed by setup.sh) animates the auto-hidden bar: 0 off (Plasma's slide).
    readonly property int panelPop: win12Taskbar && !win12GapMode ? cfg.taskbarPopAnim : 0
    onPanelPopChanged: syncPanelPop()
    function syncPanelPop() {
        if (!configReady) return;
        const fx = f => Tiles.QDBUS + " org.kde.KWin /Effects org.kde.kwin.Effects." + f + " win12panelpop";
        const plug = "kwriteconfig6 --file kwinrc --group Plugins --key win12panelpopEnabled ";
        run("sh -c " + Tiles.shQuote(panelPop > 0
            ? "kwriteconfig6 --file kwinrc --group Effect-win12panelpop --key Animation " + panelPop + " && " + plug + "true && "
              + fx("unloadEffect") + "; " + fx("loadEffect")
            : plug + "--delete; " + fx("unloadEffect")));
    }
    // W5: split taskbar islands, Windows 12 style only (code/islands.js reads the style from this widget's config).
    readonly property string islandWant: win12Taskbar ? cfg.taskbarLayout : "single"
    // back to One bar: syncIslands puts the bar's own order back first, then runs the Start position sync (centerLoop)
    onIslandWantChanged: { if (islandWant !== "single") syncCenterPanel(); syncIslands(); }
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
    // W9: magnetic islands are placed from their widths: re-run when this panel's width changes (tasks opened or
    // closed) and every 30 s for the other islands (tray icons, weather), which this widget cannot watch.
    readonly property bool islandMagnetic: islandWant !== "single" && cfg.taskbarMagnetic
    onIslandMagneticChanged: { syncIslands(); armTrayWatch(); }
    // W16: the space between magnetic islands = the bottom gap (islands.js @GAP@).
    readonly property int islandGap: islandWant !== "single" ? cfg.taskbarGap : -1
    onIslandGapChanged: syncIslands()
    // W17: the 3 Islands layout follows the Start position (roles + launchers swap, panels re-placed).
    readonly property int islandPos: islandWant === "islands" ? cfg.menuPosition : -1
    onIslandPosChanged: syncIslands()
    // 10: islands on a chosen monitor (connector name, empty = main) or on every one; the connected names go to the
    // script (@SCREENS@, resolved there with screenForConnector). A monitor plugged or unplugged re-runs the sync.
    readonly property string islandScreen: islandWant !== "single" ? cfg.islandScreen : ""
    onIslandScreenChanged: syncIslands()
    readonly property bool islandAllApps: cfg.islandAllApps
    onIslandAllAppsChanged: if (islandScreen === "all") syncIslands()
    readonly property string screenNames: Array.prototype.map.call(Qt.application.screens, s => s.name).join(",").replace(/[^A-Za-z0-9,_-]/g, "")
    onScreenNamesChanged: if (islandWant !== "single") islandTimer.restart()
    // W16: auto-hidden split islands show and hide together: KWin script extras/win12-islandreveal (installed by
    // setup.sh) watches the pointer and sets the org.win12.reveal helpers islands.js puts in every split panel.
    readonly property bool islandReveal: islandWant !== "single" && JSON.parse(panelWant).hiding !== "none"
    onIslandRevealChanged: syncIslandReveal()
    function syncIslandReveal() {
        if (!configReady) return;
        const sc = f => Tiles.QDBUS + " org.kde.KWin /Scripting org.kde.kwin.Scripting." + f;
        const plug = "kwriteconfig6 --file kwinrc --group Plugins --key win12islandrevealEnabled ";
        run("sh -c " + Tiles.shQuote(islandReveal ? plug + "true && " + sc("start")
            : plug + "--delete; " + sc("unloadScript win12islandreveal")));
    }
    readonly property int panelWidth: Window.window ? Window.window.width : 0
    onPanelWidthChanged: if (islandMagnetic) islandTimer.restart()
    Timer { id: islandTimer; interval: 1000; onTriggered: syncIslands() }
    Timer { interval: 30000; repeat: true; running: root.islandMagnetic; onTriggered: syncIslands() }
    // Tray icons come and go through the StatusNotifierWatcher: wait for its (Un)Registered signal and re-place
    // the islands 1 s later (the tray island has resized by then). Re-armed after each signal or 10 min timeout.
    // ponytail: Plasma's own tray items (volume, network...) do not signal; the 30 s timer still covers them.
    readonly property string trayWait: "W='busctl --user --timeout=600 wait /StatusNotifierWatcher org.kde.StatusNotifierWatcher StatusNotifierItem';"
        + " $W'Registered' >/dev/null 2>&1 & a=$!; $W'Unregistered' >/dev/null 2>&1 & b=$!; wait -n; kill $a $b 2>/dev/null; echo"
    // A fresh source name each time: the executable engine does not rerun a source just disconnected.
    property int trayArm: 0
    function armTrayWatch() { if (islandMagnetic) trayWatch.connectSource("bash -c " + Tiles.shQuote(trayWait) + " # " + (++trayArm)); }
    P5Support.DataSource {
        id: trayWatch
        engine: "executable"
        connectedSources: []
        onNewData: (sourceName, data) => {
            disconnectSource(sourceName);
            if (!root.islandMagnetic) return;
            islandTimer.restart();
            root.armTrayWatch();
        }
    }
    function syncIslands() {
        if (!configReady || !kdePatched) return;
        const file = decodeURIComponent(Qt.resolvedUrl("code/islands.js").toString().replace("file://", ""));
        const logic = decodeURIComponent(Qt.resolvedUrl("code/islandlogic.js").toString().replace("file://", ""));
        const dbus = Tiles.QDBUS + " org.kde.plasmashell /PlasmaShell evaluateScript \"$(sed -e s/@ID@/" + Plasmoid.id + "/ -e s/@WANT@/" + islandWant
            + "/ -e s/@POS@/" + cfg.menuPosition + "/ -e s/@MAG@/" + islandMagnetic + "/ -e s/@GAP@/" + cfg.taskbarGap + "/ "
            + "-e s/@SCR@/" + islandScreen.replace(/[^A-Za-z0-9_-]/g, "") + "/ -e s/@ALL@/" + islandAllApps + "/ -e s/@SCREENS@/" + screenNames + "/ "
            + Tiles.shQuote(logic) + " " + Tiles.shQuote(file) + ")\"";
        const loop = "for i in $(seq 130); do [ \"$(" + dbus + ")\" = done ] && break; sleep 0.3; done"
            // a restore applies the unmirrored order without the centre spacers: the Start position goes on again after it
            + (islandWant === "single" ? "; " + centerLoop() : "");
        run("flock \"${XDG_RUNTIME_DIR:-/tmp}/win12-panel.lock\" sh -c " + Tiles.shQuote(loop));
    }
    function syncPanelLook() {
        if (!configReady) return;
        const js = "const ID = " + Plasmoid.id + ", W = " + panelWant + ";"
            + "for (const p of panels()) { if (!p.widgets().some(w => w.id === ID)) continue; const h = p.hiding;"
            + " for (const k in W) { p.currentConfigGroup = ['General']; const key = 'win12Saved_' + k, s = String(p.readConfig(key, ''));"
            + "  if (W[k] !== null) { if (!s) p.writeConfig(key, JSON.stringify(p[k]));"
            // W15: {ring: n} = the user's saved value (or the current one, about to be saved) + n
            + "   const v = W[k] && typeof W[k] === 'object' ? (s ? JSON.parse(s) : p[k]) + W[k].ring : W[k]; if (p[k] !== v) p[k] = v; }"
            + "  else if (s) { p[k] = JSON.parse(s); p.currentConfigGroup = ['General']; p.writeConfig(key, ''); } }"
            // W7: Plasma re-checks "a non-normal panel never de-floats" only when floating changes: nudge it.
            + " if (p.floating && p.hiding !== h) { p.floating = false; p.floating = true; } }"
            + "print('done');";
        run("flock \"${XDG_RUNTIME_DIR:-/tmp}/win12-panel.lock\" " + Tiles.QDBUS + " org.kde.plasmashell /PlasmaShell evaluateScript " + Tiles.shQuote(js));
    }
    function syncCenterPanel() {
        if (!configReady || !kdePatched) return;   // never act on default settings before migrateConfig is done
        run("flock \"${XDG_RUNTIME_DIR:-/tmp}/win12-panel.lock\" sh -c " + Tiles.shQuote(centerLoop()));
    }
    function centerLoop() {
        const js = "const ID = " + Plasmoid.id + ", M = 'win12Centre'; let out = 'done';"
            + "const get = (w, g, k, d) => { w.currentConfigGroup = [g]; return String(w.readConfig(k, d)); };"
            + "for (const p of panels()) { const me = p.widgets().find(w => w.id === ID); if (!me) continue;"
            + " const pos = Number(get(me, 'Style', 'menuPosition', 0)), on = get(me, 'Style', 'centerTaskbar', false) === 'true';"
            // a split host is an island that fits its content: an expanding spacer would make it screen-wide
            + " const mode = !on ? 0 : pos === 1 ? 1 : pos === 2 ? 2 : 0, n = " + (islandWant !== "single" ? "0" : "[0, 2, 1][mode]") + ";"
            + " p.currentConfigGroup = ['General']; const goneWas = String(p.readConfig('win12Gone', ''));"
            + " const gone = goneWas.split(';').map(Number).filter(i => p.widgetIds.indexOf(i) >= 0), live = () => p.widgets().filter(w => gone.indexOf(w.id) < 0);"
            + " const mark = w => w.type === 'org.kde.plasma.panelspacer' ? get(w, 'General', M, '') : '';"
            + " const mine = live().filter(w => mark(w) !== '');"
            + " while (mine.length > n) { const s = mine.pop(); gone.push(s.id); s.remove(); out = 'again'; }"
            + " p.currentConfigGroup = ['General']; if (gone.join(';') !== goneWas) p.writeConfig('win12Gone', gone.join(';'));"
            + " while (mine.length < n) { const s = p.addWidget('org.kde.plasma.panelspacer'); s.currentConfigGroup = ['General']; s.writeConfig(M, '1'); s.writeConfig('expanding', true); s.reloadConfig(); mine.push(s); }"
            + " const others = live().filter(w => mark(w) === '').sort((a, b) => a.index - b.index), ids = others.map(w => w.id);"
            + " const tasks = others.filter(w => /icontasks|taskmanager/.test(w.type)).map(w => w.id), isTask = id => tasks.indexOf(id) >= 0;"
            + " p.currentConfigGroup = ['General']; const saved = String(p.readConfig('win12PanelOrder', '')).split(';').map(Number).filter(i => ids.indexOf(i) >= 0);"
            + " const base = saved.length ? saved.concat(ids.filter(i => saved.indexOf(i) < 0)) : ids;"
            + " if (mode === 2 && !saved.length) p.writeConfig('win12PanelOrder', base.join(';'));"
            + " const want = mode === 2 ? base.slice().reverse() : base.slice();"
            + " if (mode === 1 && n) { want.splice(want.indexOf(ID), 0, mine[0].id); let e = want.indexOf(ID); want.forEach((id, i) => { if (isTask(id)) e = Math.max(e, i); }); want.splice(e + 1, 0, mine[1].id); }"
            + " if (mode === 2 && n) { const t = want.findIndex(isTask); want.splice(t < 0 ? want.indexOf(ID) : t, 0, mine[0].id); }"
            + " const all = others.concat(mine), byId = id => all.find(w => w.id === id);"
            + " const cur = all.slice().sort((a, b) => a.index - b.index).map(w => w.id);"
            // all index sets in one pass (patched KDE: each set is saved at once), the next pass checks them.
            + " const bad = want.some((id, k) => byId(id).index < 0 || cur[k] !== id);"
            // the saved order goes only once it is back (each pass rebuilds the wanted order from it).
            + " if (!bad && mode !== 2 && saved.length) { p.currentConfigGroup = ['General']; p.writeConfig('win12PanelOrder', ''); }"
            + " if (bad) { want.forEach((id, k) => { if (byId(id).index !== k) byId(id).index = k; });"
            // the spacers' optimalSize binding misses the move: flip "expanding" so it recomputes.
            + "  for (const s of mine) { s.currentConfigGroup = ['General']; s.writeConfig('expanding', false); s.reloadConfig(); s.writeConfig('expanding', true); s.reloadConfig(); } out = 'again'; }"
            + " for (const id of tasks) { const w = byId(id), f = get(w, 'General', 'win12Fill', '');"
            + "  if (mode && !f) { w.writeConfig('win12Fill', get(w, 'General', 'fill', true)); w.writeConfig('fill', false); w.reloadConfig(); out = 'again'; }"
            + "  if (!mode && f) { w.writeConfig('fill', f !== 'false'); w.writeConfig('win12Fill', ''); w.reloadConfig(); out = 'again'; } } }"
            + "print(out);";
        const dbus = Tiles.QDBUS + " org.kde.plasmashell /PlasmaShell evaluateScript " + Tiles.shQuote(js);
        return "for i in $(seq 25); do [ \"$(" + dbus + ")\" = done ] && break; sleep 0.3; done";
    }

    // Find a power/session action by its id ("shutdown", "reboot", ...).
    // Returns the row, or -1 when this session can't do it (e.g. no hibernate).
    function systemAction(id) {
        if (!systemModel) return -1;
        const role = systemModel.KItemModels.KRoleNames.role("favoriteId");
        for (let i = 0; i < systemModel.count; i++) {
            if (systemModel.data(systemModel.index(i, 0), role) === id) {
                return i;
            }
        }
        return -1;
    }
    function runSystemAction(id) {
        const row = systemAction(id);
        if (row >= 0) {
            systemModel.trigger(row, "", null);
        }
        close();
    }

    function close() {
        root.menuOpen = false;
    }

    // The Win12 settings window (Plasma's configure dialog with our pages).
    function openSettings() {
        Plasmoid.internalAction("configure").trigger();
    }

    // The "Win12 Settings" app asks for the window through this config key.
    Connections {
        target: root.cfg
        function onOpenSettingsRequestChanged() {
            root.openSettings();
        }
    }

    // Reset from the settings pages (they set cfg.resetRequest = "positions|<time>" or "all|<time>").
    // Backup/restore (T13) use the same key: "backup|<time>|<file>" / "restore|<time>|<file>" (JSON file).
    // Delayed so the dialog's own Apply finishes writing first. "all" puts every key back to its main.xml
    // default; savedDesktopTheme is kept so leaving XP (menuStyle default) restores the user's Plasma Style.
    Timer {
        id: resetTimer
        interval: 700
        property string what: ""
        onTriggered: {
            const parts = root.cfg.resetRequest.split("|");
            const file = parts.slice(2).join("|");
            if (what === "backup") {
                const data = {};
                for (const k of root.userKeys()) {
                    const v = root.cfg[k];
                    // Colours become "#rrggbb"; string lists become plain arrays.
                    data[k] = v !== null && typeof v === "object" ? (v.length !== undefined ? Array.from(v) : v.toString()) : v;
                }
                root.run(Tiles.writeFileCmd(JSON.stringify({ win12Backup: 1, settings: data }), file));
                root.cfg.resetRequest = "";
                return;
            }
            if (what === "restore") {
                backupReader.connectSource("cat " + Tiles.shQuote(file));
                root.cfg.resetRequest = "";
                return;
            }
            const keys = what === "all" ? root.userKeys() : ["tileGroups", "pinnedGrid", "pinnedCaptions", "tilePage"];
            for (const k of keys) {
                const d = root.cfg[k + "Default"];
                if (d !== undefined)
                    root.cfg[k] = d;
            }
            root.cfg.resetRequest = "";
        }
    }
    // Keys that reset/backup/restore touch: everything in main.xml but bookkeeping and kicker's favorites.
    function userKeys() {
        const keep = ["savedDesktopTheme", "savedDecoLibrary", "savedDecoTheme", "savedColorScheme", "savedBlur", "savedIconTheme", "resetRequest", "openSettingsRequest", "designRequest", "favorites", "favoritesPortedToKAstats", "configMigrated"];
        return root.cfg.keys().filter(k => !k.endsWith("Default") && !keep.includes(k));
    }
    // Restore: read the backup JSON, write back every known key, then put the Plasma Style in line
    // (xpTheme changes call syncDesktopTheme too; the explicit call covers an unchanged xpTheme).
    P5Support.DataSource {
        id: backupReader
        engine: "executable"
        connectedSources: []
        onNewData: (sourceName, data) => {
            disconnectSource(sourceName);
            let s = null;
            try { s = JSON.parse(data.stdout).settings; } catch (e) {}
            if (!s) {
                console.warn("win12: not a Win12 backup:", sourceName, data.stderr);
                return;
            }
            for (const k of root.userKeys()) {
                if (s[k] !== undefined)
                    root.cfg[k] = s[k];
            }
            root.syncDesktopTheme();
            root.syncPanelPop();
            root.syncIslandReveal();
            root.syncPanelLook();   // W12: the onReady run can die with a reloading applet (panel left as the last style)
            root.syncBlurStrength();
            root.readBrightness();
        }
    }
    property string doneReset: ""
    Connections {
        target: root.cfg
        // The settings window re-writes its old resetRequest on every later Apply: run each request once.
        function onResetRequestChanged() {
            const v = root.cfg.resetRequest;
            if (!v || v === root.doneReset) return;
            root.doneReset = v;
            const w = v.split("|")[0];
            if (["all", "positions", "backup", "restore"].includes(w)) {
                resetTimer.what = w;
                resetTimer.restart();
            }
        }
    }

    // Per-tile icon and colour pickers, shared by all tiles.
    property string pickingTile: ""
    KIconThemes.IconDialog {
        id: iconDialog
        onIconNameChanged: name => {
            if (root.pickingTile && name) {
                root.saveGroups(Tiles.setProp(root.groups, root.pickingTile, "icon", name));
            }
        }
    }
    Dialogs.ColorDialog {
        id: colorDialog
        property var apply: null   // set by askColour; null = tile colour
        onAccepted: apply ? apply(selectedColor.toString()) : root.saveGroups(Tiles.setProp(root.groups, root.pickingTile, "color", selectedColor.toString()))
    }
    function pickTileIcon(id) {
        pickingTile = id;
        close();
        iconDialog.open();
    }
    Dialogs.FileDialog {
        id: tileImageDialog
        title: i18n("Pick tile image…")
        nameFilters: [i18n("Images (*.png *.jpg *.jpeg *.webp *.svg)")]
        onAccepted: root.saveGroups(Tiles.setProp(root.groups, root.pickingTile, "image",
            decodeURIComponent(selectedFile.toString().replace("file://", ""))))
    }
    Dialogs.FileDialog {
        id: photoFileDialog
        title: i18n("Choose picture…")
        nameFilters: [i18n("Images (*.png *.jpg *.jpeg *.webp *.svg)")]
        onAccepted: root.saveGroups(Tiles.setProp(root.groups, root.pickingTile, "photo",
            decodeURIComponent(selectedFile.toString().replace("file://", ""))))
    }
    Dialogs.FolderDialog {
        id: photoFolderDialog
        title: i18n("Choose folder…")
        onAccepted: root.saveGroups(Tiles.setProp(root.groups, root.pickingTile, "photo",
            "dir:" + decodeURIComponent(selectedFolder.toString().replace("file://", ""))))
    }
    function pickPhoto(id, folder) {
        pickingTile = id;
        close();
        (folder ? photoFolderDialog : photoFileDialog).open();
    }
    function pickTileImage(id) {
        pickingTile = id;
        close();
        tileImageDialog.open();
    }
    function pickTileColor(id, current) {
        pickingTile = id;
        colorDialog.apply = null;
        colorDialog.selectedColor = current || "#3a3a44";
        close();
        colorDialog.open();
    }
    // Same dialogs for canvas groups: fn(colour) / fn(folderUrl) runs on OK.
    function askColour(current, fn) {
        colorDialog.apply = fn;
        colorDialog.selectedColor = current || "#3a3a44";
        close();
        colorDialog.open();
    }
    function askFolder(fn) {
        close();
        pinFolderDialog.apply = fn;
        pinFolderDialog.open();
    }
    // Desktop ids of installed Steam games (Steam writes <Game>.desktop with steam://rungameid), for fn(ids).
    property var steamDone: null
    // Filtered folders (T17): [{ name, path, include, exclude }]; folderUrl(f) = its file url.
    readonly property var filteredFolders: Tiles.parseFiltered(cfg.filteredFolders)
    function folderUrl(f) { return Tiles.dirUrl(f.path, StandardPaths.writableLocation(StandardPaths.HomeLocation)); }
    function listSteamGames(fn) {
        steamDone = fn;
        steamLister.connectSource("grep -l steam://rungameid ~/.local/share/applications/*.desktop; true");
    }
    P5Support.DataSource {
        id: steamLister
        engine: "executable"
        connectedSources: []
        onNewData: (sourceName, data) => {
            disconnectSource(sourceName);
            const ids = (data.stdout || "").split("\n").filter(l => l).map(l => l.split("/").pop());
            if (root.steamDone) root.steamDone(ids);
            root.steamDone = null;
        }
    }

    // "Pin website / file / folder" from the menu-surface right-click menus.
    Dialogs.FileDialog {
        id: pinFileDialog
        title: i18n("Pin file to Start")
        onAccepted: root.pin(Tiles.customFor(selectedFile.toString(), "file"))
    }
    Dialogs.FolderDialog {
        id: pinFolderDialog
        property var apply: null   // set by askFolder; null = pin the folder
        title: apply ? i18n("Add a folder as group") : i18n("Pin folder to Start")
        onAccepted: apply ? apply(selectedFolder.toString()) : root.pin(Tiles.customFor(selectedFolder.toString(), "folder"))
    }
    // A plain window: the popup is closed, so an in-popup Kirigami dialog can't show.
    Window {
        id: webDialog
        title: i18n("Pin website to Start")
        width: Kirigami.Units.gridUnit * 22
        height: webLayout.implicitHeight + Kirigami.Units.largeSpacing * 2
        color: Kirigami.Theme.backgroundColor
        onVisibleChanged: if (visible) { requestActivate(); webField.forceActiveFocus(); }
        function accept() {
            const u = webField.text.trim();
            if (u) root.pin(Tiles.customFor(/^\w+:\/\//.test(u) ? u : "https://" + u, "web", cfg.webFavicons));
            webField.text = "";
            close();
        }
        ColumnLayout {
            id: webLayout
            anchors.fill: parent
            anchors.margins: Kirigami.Units.largeSpacing
            QQC2.Label { text: i18n("Web address:") }
            QQC2.TextField {
                id: webField
                Layout.fillWidth: true
                placeholderText: "https://…"
                onAccepted: webDialog.accept()
                Keys.onEscapePressed: webDialog.close()
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                QQC2.Button { text: i18n("Pin"); icon.name: "window-pin"; onClicked: webDialog.accept() }
                QQC2.Button { text: i18n("Cancel"); icon.name: "dialog-cancel"; onClicked: webDialog.close() }
            }
        }
    }
    Window {
        id: renameDialog
        property string pinId: ""
        property var apply: null
        title: i18n("Rename")
        width: Kirigami.Units.gridUnit * 18
        height: renameLayout.implicitHeight + Kirigami.Units.largeSpacing * 2
        color: Kirigami.Theme.backgroundColor
        onVisibleChanged: if (visible) { requestActivate(); renameField.forceActiveFocus(); renameField.selectAll(); }
        function accept() {
            if (apply) apply(renameField.text.trim());
            else root.setCap(pinId, renameField.text.trim());
            close();
        }
        ColumnLayout {
            id: renameLayout
            anchors.fill: parent
            anchors.margins: Kirigami.Units.largeSpacing
            QQC2.Label { text: renameDialog.apply ? i18n("Name:") : i18n("Name (empty = original):") }
            QQC2.TextField {
                id: renameField
                Layout.fillWidth: true
                onAccepted: renameDialog.accept()
                Keys.onEscapePressed: renameDialog.close()
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                QQC2.Button { text: i18n("OK"); icon.name: "dialog-ok"; onClicked: renameDialog.accept() }
                QQC2.Button { text: i18n("Cancel"); icon.name: "dialog-cancel"; onClicked: renameDialog.close() }
            }
        }
    }
    function pinNew(kind) {
        close();
        if (kind === "web") webDialog.show();
        else if (kind === "file") pinFileDialog.open();
        else {
            pinFolderDialog.apply = null;
            pinFolderDialog.open();
        }
    }
    // Menu-surface entries for TileArea and PinnedGrid.
    readonly property var pinNewItems: [
        { text: i18n("Pin website…"), icon: "internet-web-browser", run: () => root.pinNew("web") },
        { text: i18n("Pin file…"), icon: "document-new", run: () => root.pinNew("file") },
        { text: i18n("Pin folder…"), icon: "folder-new", run: () => root.pinNew("folder") }
    ]

    // "Add widget" submenu (TileArea, TileGroup): a new kind = Tiles.WIDGETS + here + Tile.widgetFile + widgets/<Kind>.qml.
    readonly property var widgetItems: [["clock", i18n("Clock")], ["calendar", i18n("Calendar")], ["sysmon", i18n("System monitor")],
        ["drives", i18n("Drives")], ["weather", i18n("Weather")], ["notes", i18n("Notes")], ["photo", i18n("Photo")]]
        .map(k => ({ kind: k[0], text: k[1], run: () => savePages(Tiles.addWidget(pages, tilePage, k[0])) }))
        .concat([{ kind: "", text: i18n("KDE widgets…"), icon: "plasmagik", run: () => appletPicker.openFor(0) }])   // kind "" = real widget

    // Real KDE widgets: child applets of this containment, one tile "applet|<applet.id>" each.
    property var appletItems: ({})          // applet.id -> its PlasmoidItem (reparented into the tile)
    property int pendingAppletGroup: -1     // group for the applet being created by the picker
    property var removedApplets: []         // ids removed this session (still listed until Plasma deletes them)
    function appletFor(tileId) {
        const aid = Number(String(tileId).slice(7));
        const list = Containment.applets;
        for (let i = 0; i < list.length; i++) if (list[i].id === aid) return list[i];
        return null;
    }
    function hostApplet(applet) {
        if (appletItems[applet.id]) return;   // ponytail: plasmawindowed restores children twice, first one wins
        const it = root.itemFor(applet);
        if (!it) return;
        // Like on the desktop: a widget that picks its own look (analog clock = face, its full rep is a calendar)
        // or switches by size keeps that; one with no preference shows its full representation.
        if (!it.preferredRepresentation) it.preferredRepresentation = it.fullRepresentation;
        it.visible = false;
        const m = Object.assign({}, appletItems);
        m[applet.id] = it;
        appletItems = m;
    }
    function addApplet(pluginName, gi) {
        pendingAppletGroup = gi;
        appletPicker.explorer.addApplet(pluginName);   // Containment.onAppletAdded places the tile
    }
    Containment.onAppletAdded: (applet, geometry) => {
        hostApplet(applet);
        if (pendingAppletGroup >= 0) {
            savePages(Tiles.addApplet(pages, tilePage, applet.id, pendingAppletGroup));
            pendingAppletGroup = -1;
        }
        appletSync.restart();
    }
    Containment.onAppletRemoved: applet => {
        const m = Object.assign({}, appletItems);
        delete m[applet.id];
        appletItems = m;
        appletSync.restart();
    }
    // Restore check (also after adds/removes): each live applet has exactly one tile, tiles of gone applets go.
    Timer {
        id: appletSync
        interval: 1000
        onTriggered: {
            const ids = [];
            const list = root.Containment.applets;
            for (let i = 0; i < list.length; i++)
                if (ids.indexOf(list[i].id) < 0 && root.removedApplets.indexOf(list[i].id) < 0) ids.push(list[i].id);
            const next = Tiles.syncApplets(root.pages, ids);
            if (JSON.stringify(next) !== JSON.stringify(root.pages)) root.savePages(next);
        }
    }
    readonly property AppletPicker appletPicker: AppletPicker {}

    onMenuOpenChanged: {
        if (root.menuOpen) {
            rootModel.refresh();
        } else {
            searchText = "";
            designMode = false;
        }
    }

    // Full screen menu (UseFullScreenMenu): the popup takes the screen size, or the screen minus
    // the panels with SemiFull. Win10 tile groups then wrap into more columns (TileArea Flow).
    readonly property rect fullRect: {
        const c = Plasmoid.containment;
        const r = c ? (cfg.keepPanelVisible ? c.availableScreenRect : c.screenGeometry) : null;
        return r && r.width > 0 ? Qt.rect(0, 0, r.width, r.height) : Qt.rect(0, 0, 1280, 800);
    }
    // Win10/Win11 stretch over the full-screen popup; the narrow styles keep their size and sit
    // centred on a dim backdrop (Metrics.heightFor gives them their normal height).
    readonly property bool fillScreen: cfg.fullScreen && (styleFile === "windows10" || styleFile === "windows11")
    readonly property bool narrowFull: cfg.fullScreen && !fillScreen

    Plasmoid.icon: "start-here-kde"
    // A containment (needed to host real widgets) is always "expanded" in libplasma and never gets a
    // compact representation or popup, so Win12 does what the system tray does: the button is a child of
    // root and the menu lives in our own AppletPopup (docs/research/real-widgets-spike.md, Follow-up).
    // menuOpen replaces PlasmoidItem.expanded. On the desktop (or in a test window) the menu shows inline.
    property bool menuOpen: false
    readonly property bool planar: Plasmoid.formFactor === PlasmaCore.Types.Planar
    Plasmoid.onActivated: menuOpen = !menuOpen   // Meta key (ShellCorona::activateLauncherMenu)
    Layout.minimumWidth: planar ? full.Layout.minimumWidth : startButton.Layout.minimumWidth
    Layout.minimumHeight: planar ? full.Layout.minimumHeight : startButton.Layout.minimumHeight
    Layout.preferredWidth: planar ? full.Layout.preferredWidth : startButton.Layout.preferredWidth
    Layout.preferredHeight: planar ? full.Layout.preferredHeight : startButton.Layout.preferredHeight

    StartButton {
        id: startButton
        anchors.fill: parent
        visible: !root.planar
    }

    // Hosted widgets open their own popups (digital clock calendar); keep the menu open meanwhile.
    readonly property bool appletPopupOpen: {
        for (const k in appletItems) if (appletItems[k].expanded) return true;
        return false;
    }
    property int statusBeforeOpen: PlasmaCore.Types.ActiveStatus
    Timer {
        id: expandedSync   // as plasma-desktop CompactApplet: lets a click on the button after an outside close stay a close
        interval: 100
        onTriggered: root.menuOpen = popup.visible
    }
    PlasmaCore.AppletPopup {
        id: popup
        visualParent: startButton
        appletInterface: root
        mainItem: root.planar ? null : full
        visible: root.menuOpen && !root.planar
        hideOnWindowDeactivate: !root.designMode && !root.appletPopupOpen
        popupDirection: switch (Plasmoid.location) {
            case PlasmaCore.Types.TopEdge: return Qt.BottomEdge;
            case PlasmaCore.Types.LeftEdge: return Qt.RightEdge;
            case PlasmaCore.Types.RightEdge: return Qt.LeftEdge;
            default: return Qt.TopEdge;
        }
        margin: root.menuKind === "windows12" ? root.cfg.menuGap   // W12 menu gap
            : (Plasmoid.containmentDisplayHints & PlasmaCore.Types.ContainmentPrefersFloatingApplets) ? Kirigami.Units.largeSpacing : 0
        floating: Plasmoid.location === PlasmaCore.Types.Floating
        removeBorderStrategy: Plasmoid.location === PlasmaCore.Types.Floating
            ? PlasmaCore.AppletPopup.AtScreenEdges
            : PlasmaCore.AppletPopup.AtScreenEdges | PlasmaCore.AppletPopup.AtPanelEdges
        backgroundHints: (Plasmoid.containmentDisplayHints & PlasmaCore.Types.ContainmentPrefersOpaqueBackground)
            ? PlasmaCore.AppletPopup.SolidBackground : PlasmaCore.AppletPopup.StandardBackground
        onVisibleChanged: {
            if (!visible) {
                expandedSync.restart();
                Plasmoid.status = root.statusBeforeOpen;
            } else {
                root.statusBeforeOpen = Plasmoid.status;
                Plasmoid.status = PlasmaCore.Types.RequiresAttentionStatus;   // keeps an auto-hide panel up
                requestActivate();
                full.forceActiveFocus();
            }
        }
    }

    Item {
        id: full
        focus: true
        anchors.fill: root.planar ? root : undefined

        // The style gives the start size; the user can drag the popup edges to resize it.
        Layout.preferredWidth: root.cfg.fullScreen ? root.fullRect.width
                             : styleLoader.item ? styleLoader.item.implicitWidth : 600
        Layout.preferredHeight: root.cfg.fullScreen ? root.fullRect.height
                              : styleLoader.item ? styleLoader.item.implicitHeight : 600
        // Plasma keeps the user's dragged popup size and ignores preferred sizes, so full screen
        // forces the size through the minimum.
        Layout.minimumWidth: root.cfg.fullScreen ? root.fullRect.width
                           : designW > 0 ? designW
                           : shrink && styleLoader.item ? styleLoader.item.implicitWidth
                           : styleLoader.item && styleLoader.item.minimumWidth ? styleLoader.item.minimumWidth : 300
        // Design mode: Plasma keeps the dragged popup width, so pin it (min = max) at the opening width
        // plus whatever the style's implicitWidth gains from a divider (Win10 list, App canvas): the
        // section right of the divider keeps its size. Styles whose divider side is fixed on the right
        // keep implicitWidth, so the popup stays put and the left side gives way.
        property real designBase: 0
        readonly property real designW: root.designMode && !root.cfg.fullScreen && styleLoader.item
                                        ? designBase + styleLoader.item.implicitWidth : 0
        Connections {
            target: root
            function onDesignModeChanged() {
                if (root.designMode && styleLoader.item) full.designBase = full.width - styleLoader.item.implicitWidth;
            }
        }
        Layout.minimumHeight: root.cfg.fullScreen ? root.fullRect.height
                            : shrink && styleLoader.item ? styleLoader.item.implicitHeight : Math.round(320 * root.cfg.uiScale)
        // Plasma keeps one saved popup size for every style. After leaving full screen or switching style
        // (or menu size), pin the popup once (min = max) at the style's own size until it has opened and
        // stored that size; the user can drag it afterwards as before.
        property bool shrink: false
        onShrinkChanged: if (shrink && root.menuOpen) shrinkDone.restart()   // style switched while open
        Layout.maximumWidth: designW > 0 ? designW
                           : shrink && styleLoader.item ? styleLoader.item.implicitWidth : Number.POSITIVE_INFINITY
        Layout.maximumHeight: shrink && styleLoader.item ? styleLoader.item.implicitHeight : Number.POSITIVE_INFINITY
        Connections {
            target: root.cfg
            function onFullScreenChanged() {
                if (!root.cfg.fullScreen) full.shrink = true;
            }
            function onMenuSizeChanged() { full.shrink = true; }
        }
        // A new style starts from its own height (Win10: tall enough for the tiles), not the old custom one.
        property string lastStyle: ""
        Component.onCompleted: lastStyle = root.styleFile   // not a binding: it would update before the handler
        Connections {
            target: root
            function onStyleFileChanged() {
                if (!full.lastStyle || root.styleFile === full.lastStyle) return;
                full.lastStyle = root.styleFile;
                root.cfg.menuHeight = 0;
                full.shrink = true;
            }
        }
        Timer {
            id: shrinkDone
            interval: 1000
            onTriggered: full.shrink = false
        }

        // Remember a dragged height as the "Custom height" setting.
        Timer {
            id: saveHeight
            interval: 400
            onTriggered: {
                const h = Math.round(full.height / root.cfg.uiScale);
                if (root.menuOpen && !root.cfg.fullScreen && Math.abs(full.height - full.Layout.preferredHeight) > 2) {
                    root.cfg.menuHeight = h;
                }
            }
        }
        onHeightChanged: if (root.menuOpen) saveHeight.restart(); else place()

        // T42 Menu position (key menuPosition 0 left, 1 center, 2 right), horizontal panels only. The popup
        // (AppletPopup) lines its edge up with its visualParent, normally the Start button. For Center/Right it
        // gets StartButton.popupAnchor instead: an empty item in the panel, as wide as the popup, put under the
        // screen centre or at the right screen edge, so either alignment (LTR left edges, RTL right edges) lands
        // the popup there. Done while closed (and once when the popup window first appears) so it opens in place.
        readonly property var popupWindow: Window.window
        onPopupWindowChanged: place()
        onWidthChanged: if (!root.menuOpen) place()
        Connections {
            target: root
            function onMenuOpenChanged() { if (!root.menuOpen) full.place(); }
        }
        Connections {
            target: root.cfg
            function onMenuPositionChanged() { full.place(); }
        }
        function place() {
            const w = popupWindow, btn = startButton;
            if (!w || !btn || !btn.popupAnchor || w.visualParent === undefined) return;
            const g = Plasmoid.containment ? Plasmoid.containment.screenGeometry : null;
            const pos = !root.cfg.fullScreen && g && g.width > 0 ? root.cfg.menuPosition : 0;
            const vert = Plasmoid.formFactor === PlasmaCore.Types.Vertical;   // left/right panel: top / middle / bottom
            if (pos !== 1 && pos !== 2) {
                if (w.visualParent !== btn) w.visualParent = btn;
                return;
            }
            // The anchor lives in the panel window's root item, not in the button: the button moves when
            // centerTaskbar adds its spacers and a button-relative x went stale (popup opened off-centre).
            const a = btn.popupAnchor, o = btn.mapToGlobal(0, 0), sc = btn.mapToItem(null, 0, 0);
            if (btn.Window.contentItem && a.parent !== btn.Window.contentItem) a.parent = btn.Window.contentItem;
            if (vert) {
                a.width = btn.width;
                a.height = w.height;
                a.x = sc.x;
                a.y = (pos === 1 ? g.y + (g.height - w.height) / 2 : g.y + g.height - w.height) - (o.y - sc.y);
            } else {
                a.width = w.width;
                a.height = btn.height;
                a.y = sc.y;
                a.x = (pos === 1 ? g.x + (g.width - w.width) / 2 : g.x + g.width - w.width) - (o.x - sc.x);
            }
            w.visualParent = a;
        }

        // Open animation (Start11 "NoAnims" inverse): short fade + slide in.
        ParallelAnimation {
            id: openAnim
            NumberAnimation { target: full; property: "opacity"; from: 0; to: 1; duration: Kirigami.Units.shortDuration }
            NumberAnimation { target: styleLoader; property: "anchors.topMargin"; from: Kirigami.Units.gridUnit; to: 0; duration: Kirigami.Units.shortDuration; easing.type: Easing.OutCubic }
        }
        Connections {
            target: root
            function onMenuOpenChanged() {
                if (root.menuOpen && full.shrink) shrinkDone.restart();
                if (root.menuOpen && root.cfg.animateMenu) openAnim.restart();
                else { openAnim.stop(); full.opacity = 1; styleLoader.anchors.topMargin = 0; }
            }
        }

        // Narrow style in full screen: dim the rest of the screen, click it to close.
        Rectangle {
            anchors.fill: parent
            visible: root.narrowFull
            color: Qt.rgba(0, 0, 0, 0.5)
            MouseArea { anchors.fill: parent; onClicked: root.menuOpen = false }
        }

        Item {
            id: frame
            readonly property int gapX: root.narrowFull && styleLoader.item ? Math.max(0, (full.width - styleLoader.item.implicitWidth) / 2) : 0
            readonly property int gapY: root.narrowFull && styleLoader.item ? Math.max(0, full.height - styleLoader.item.implicitHeight) : 0
            readonly property bool atTop: Plasmoid.location === PlasmaCore.Types.TopEdge
            // T42 Menu position Right: the whole menu mirrored (anchors, rows, lists, split handles; styles flip
            // their explicit x with LayoutMirroring.enabled). Tile grids keep their own order inside.
            LayoutMirroring.enabled: root.mirrorMenu
            LayoutMirroring.childrenInherit: true
            anchors.fill: parent
            anchors.leftMargin: gapX
            anchors.rightMargin: gapX
            anchors.topMargin: (atTop ? 0 : gapY) + (root.designMode ? designBar.height : 0)   // sits on the panel side; design bar above
            anchors.bottomMargin: atTop ? gapY : 0

            // Right-click on empty menu space: the Start button's menu (Settings, Configure…). Items with
            // their own right-click menu grab the press first.
            TapHandler {
                acceptedButtons: Qt.RightButton
                onTapped: eventPoint => startButton.openMenu(styleLoader, eventPoint.position.x, eventPoint.position.y)
            }
            Loader {
                id: styleLoader
                anchors.fill: parent
                focus: true
                source: "styles/" + root.styleFile + ".qml"
            }
        }

        // Design mode bar above the menu (the menu moves down, nothing is covered).
        Rectangle {
            id: designBar
            visible: root.designMode
            anchors.left: frame.left
            anchors.right: frame.right
            anchors.bottom: frame.top
            height: designRow.implicitHeight + Kirigami.Units.smallSpacing * 2
            color: Kirigami.Theme.highlightColor
            RowLayout {
                id: designRow
                anchors.fill: parent
                anchors.margins: Kirigami.Units.smallSpacing
                QQC2.Label {
                    Layout.fillWidth: true
                    text: i18n("Design mode — drag the dividers")
                    color: Kirigami.Theme.highlightedTextColor
                    elide: Text.ElideRight
                }
                QQC2.Button {
                    text: i18n("Reset")
                    icon.name: "edit-reset"
                    onClicked: root.cfg.splits = Tiles.splitReset(root.cfg.splits, root.menuKind)
                }
                QQC2.Button {
                    text: i18n("Done")
                    icon.name: "dialog-ok"
                    onClicked: root.designMode = false
                }
            }
        }
    }

    // Start button right-click menu (Start11: run item, shutdown option, explorer link, ...).
    function hasMenuItem(id) {
        return cfg.buttonMenuItems.includes(id);
    }
    Plasmoid.contextualActions: [
        PlasmaCore.Action {
            text: i18n("Settings")
            icon.name: "systemsettings"
            visible: root.hasMenuItem("settings")
            onTriggered: root.run("systemsettings")
        },
        PlasmaCore.Action {
            text: i18n("File Explorer")
            icon.name: "system-file-manager"
            visible: root.hasMenuItem("files")
            onTriggered: root.run("dolphin")
        },
        PlasmaCore.Action {
            text: i18n("System Monitor")
            icon.name: "utilities-system-monitor"
            visible: root.hasMenuItem("monitor")
            onTriggered: root.run("plasma-systemmonitor")
        },
        PlasmaCore.Action {
            text: i18n("Terminal")
            icon.name: "utilities-terminal"
            visible: root.hasMenuItem("terminal")
            onTriggered: root.run("konsole")
        },
        PlasmaCore.Action {
            text: i18n("Run…")
            icon.name: "system-run"
            visible: root.hasMenuItem("run")
            onTriggered: root.run("krunner")
        },
        PlasmaCore.Action {
            text: i18n("Show desktop")
            icon.name: "user-desktop"
            visible: root.hasMenuItem("desktop")
            onTriggered: root.run("" + Tiles.QDBUS + " org.kde.kglobalaccel /component/kwin invokeShortcut \"Show Desktop\"")
        },
        PlasmaCore.Action {
            text: i18n("Edit panel")
            icon.name: "configure-toolbars"
            visible: root.hasMenuItem("editpanel")
            onTriggered: root.run("" + Tiles.QDBUS + " org.kde.plasmashell /PlasmaShell org.freedesktop.DBus.Properties.Set org.kde.PlasmaShell editMode true")
        },
        PlasmaCore.Action {
            text: i18n("Auto-hide panel")
            icon.name: "view-fullscreen"
            visible: root.hasMenuItem("autohide")
            // flips hiding of the panel holding this widget (found by widget id)
            onTriggered: root.run("" + Tiles.QDBUS + " org.kde.plasmashell /PlasmaShell evaluateScript 'for (const p of panels()) { if (p.widgetIds.indexOf(" + Plasmoid.id + ") >= 0) p.hiding = (p.hiding == \"none\" ? \"autohide\" : \"none\"); }'")
        },
        PlasmaCore.Action {
            isSeparator: true
            visible: root.hasMenuItem("power")
        },
        PlasmaCore.Action {
            text: i18n("Lock")
            icon.name: "system-lock-screen"
            visible: root.hasMenuItem("power")
            onTriggered: root.runSystemAction("lock-screen")
        },
        PlasmaCore.Action {
            text: i18n("Sign out")
            icon.name: "system-log-out"
            visible: root.hasMenuItem("power")
            onTriggered: root.runSystemAction("logout")
        },
        PlasmaCore.Action {
            text: i18n("Sleep")
            icon.name: "system-suspend"
            visible: root.hasMenuItem("power")
            onTriggered: root.runSystemAction("suspend")
        },
        PlasmaCore.Action {
            text: i18n("Restart")
            icon.name: "system-reboot"
            visible: root.hasMenuItem("power")
            onTriggered: root.runSystemAction("reboot")
        },
        PlasmaCore.Action {
            text: i18n("Shut down")
            icon.name: "system-shutdown"
            visible: root.hasMenuItem("power")
            onTriggered: root.runSystemAction("shutdown")
        },
        PlasmaCore.Action {
            isSeparator: true
        },
        PlasmaCore.Action {
            text: i18n("Edit menu layout")
            icon.name: "transform-move-horizontal"
            onTriggered: root.editLayout()
        },
        PlasmaCore.Action {
            text: i18nc("@action:inmenu", "Edit Applications…")
            icon.name: "kmenuedit"
            visible: Plasmoid.immutability !== PlasmaCore.Types.SystemImmutable
            onTriggered: processRunner.runMenuEditor()
        }
    ]

    Component.onCompleted: {
        Plasmoid.activationTogglesExpanded = true;
        armTrayWatch();
        for (let i = 0; i < Containment.applets.length; i++) hostApplet(Containment.applets[i]);
        rootModel.refresh();
        systemModel = systemModelComponent.createObject(root, { favoritesModel: rootModel.systemFavoritesModel });
        if (configReady)
            onReady();
        else
            migrateConfig();
    }
    // A Containment's config() is its own appletsrc group, a plain applet's is the "Configuration" subgroup
    // (libplasma Applet::config()), so after the PlasmoidItem -> ContainmentItem switch Plasmoid.configuration
    // read [Applets][N][General] etc. and saw defaults. Once per applet: copy every [Applets][N][Configuration][*]
    // group up one level through the shell's scripting API (raw strings, no type guessing), set configMigrated,
    // reloadConfig. The old groups stay (rollback to the PlasmoidItem build keeps working). A new widget has none.
    readonly property bool configReady: cfg.configMigrated
    onConfigReadyChanged: if (configReady) onReady()
    function migrateConfig() {
        const script = "const ID = " + Plasmoid.id + ";"
            + "for (const c of panels().concat(desktops())) for (const w of c.widgets()) { if (w.id != ID) continue;"
            + " c.currentConfigGroup = ['Applets', String(ID), 'Configuration'];"
            + " const old = {}; for (const g of c.configGroups.slice()) { c.currentConfigGroup = ['Applets', String(ID), 'Configuration', g]; old[g] = {};"
            // one-shot *Request keys (open settings, design mode, reset/backup) are not copied: changing them acts.
            + "  for (const k of c.configKeys.slice()) if (!/Request$/.test(k)) old[g][k] = c.readConfig(k, ''); }"
            + " for (const g in old) { w.currentConfigGroup = [g]; for (const k in old[g]) w.writeConfig(k, old[g][k]); }"
            + " w.currentConfigGroup = ['General']; w.writeConfig('configMigrated', 'true'); w.reloadConfig(); }";
        run(Tiles.QDBUS + " org.kde.plasmashell /PlasmaShell evaluateScript " + Tiles.shQuote(script));
    }
    property bool readyDone: false
    function onReady() {
        if (readyDone) return;   // configReady can flip after onCompleted already saw it true
        readyDone = true;
        appletSync.start();
        syncCenterPanel();
        syncPanelLook();
        syncIslands();
        // T44: code/setup.sh makes the generated themes first (KDE Store installs skip install.sh), then the syncs apply them.
        setupRunner.connectSource("sh " + Tiles.shQuote(decodeURIComponent(Qt.resolvedUrl("code/setup.sh").toString().replace("file://", ""))));
    }
    P5Support.DataSource {
        id: setupRunner
        engine: "executable"
        connectedSources: []
        onNewData: sourceName => {
            disconnectSource(sourceName);
            root.syncDesktopTheme();
            root.syncPanelPop();
            root.syncIslandReveal();
            root.syncPanelLook();   // W12: the onReady run can die with a reloading applet (panel left as the last style)
            root.syncBlurStrength();
            root.readBrightness();   // then syncWindowDeco + syncWindowColors
        }
    }
}

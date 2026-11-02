import QtQuick
import QtCore
import org.kde.plasma.plasmoid
import org.kde.coreaddons as KCoreAddons

// Shortcuts shared by every style: folders, settings, user menu and power menu.
// Built-in ids, or custom "custom|name|icon|target" (target "cmd:..." runs a command).
Item {
    id: places
    visible: false

    required property var contextMenu
    readonly property var cfg: Plasmoid.configuration

    readonly property alias user: kuser
    KCoreAddons.KUser {
        id: kuser
    }

    function folder(loc) {
        return StandardPaths.writableLocation(loc);
    }

    readonly property var catalog: ({
        "user":            { text: kuser.fullName || kuser.loginName, icon: "user-identity", menu: "user" },
        "documents":       { text: i18n("Documents"), icon: "folder-documents-symbolic", url: folder(StandardPaths.DocumentsLocation) },
        "downloads":       { text: i18n("Downloads"), icon: "folder-download-symbolic", url: folder(StandardPaths.DownloadLocation) },
        "music":           { text: i18n("Music"), icon: "folder-music-symbolic", url: folder(StandardPaths.MusicLocation) },
        "pictures":        { text: i18n("Pictures"), icon: "folder-pictures-symbolic", url: folder(StandardPaths.PicturesLocation) },
        "videos":          { text: i18n("Videos"), icon: "folder-videos-symbolic", url: folder(StandardPaths.MoviesLocation) },
        "home":            { text: i18n("Personal folder"), icon: "user-home-symbolic", url: folder(StandardPaths.HomeLocation) },
        "network":         { text: i18n("Network"), icon: "network-workgroup-symbolic", url: "remote:/" },
        "computer":        { text: i18n("Computer"), icon: "computer-symbolic", url: "file:///" },
        "trash":           { text: i18n("Trash"), icon: "user-trash-symbolic", url: "trash:/" },
        "recent":          { text: i18n("Recent files"), icon: "document-open-recent-symbolic", url: "recentlyused:/" },
        "games":           { text: i18n("Games"), icon: "applications-games-symbolic", url: "applications:/Games" },
        "printers":        { text: i18n("Printers"), icon: "printer-symbolic", cmd: "systemsettings kcm_printer_manager" },
        "users":           { text: i18n("Users"), icon: "system-users-symbolic", cmd: "systemsettings kcm_users" },
        "updates":         { text: i18n("Updates"), icon: "system-software-update-symbolic", cmd: "plasma-discover --mode update" },
        "separator":       { text: "", icon: "", separator: true },
        "files":           { text: i18n("File Explorer"), icon: "system-file-manager-symbolic", url: folder(StandardPaths.HomeLocation) },
        "personalization": { text: i18n("Personalization"), icon: "preferences-desktop-theme-global-symbolic", cmd: "systemsettings kcm_lookandfeel" },
        "settings":        { text: i18n("Settings"), icon: "configure-symbolic", cmd: "systemsettings" },
        "terminal":        { text: i18n("Terminal"), icon: "utilities-terminal-symbolic", cmd: "konsole" },
        "monitor":         { text: i18n("System Monitor"), icon: "utilities-system-monitor-symbolic", cmd: "plasma-systemmonitor" },
        "help":            { text: i18n("Help and Support"), icon: "help-browser", cmd: "khelpcenter" },
        "search":          { text: i18n("Search"), icon: "system-search", cmd: "krunner" },
        "run":             { text: i18n("Run…"), icon: "system-run-symbolic", cmd: "krunner" },
        "win12":         { text: i18n("Win12 Settings"), icon: "configure", configure: true },
        "power":           { text: i18n("Power"), icon: "system-shutdown-symbolic", menu: "power" }
    })

    function info(id) {
        if (id.startsWith("custom|")) {
            const p = id.split("|");
            const target = p.slice(3).join("|");
            return target.startsWith("cmd:")
                ? { text: p[1], icon: p[2] || "application-x-executable", cmd: target.slice(4) }
                : { text: p[1], icon: p[2] || "folder", url: target };
        }
        return catalog[id];
    }

    readonly property var powerCatalog: ({
        "lock-screen": { text: i18n("Lock"), icon: "system-lock-screen" },
        "logout":      { text: i18n("Sign out"), icon: "system-log-out" },
        "switch-user": { text: i18n("Switch user"), icon: "system-switch-user" },
        "suspend":     { text: i18n("Sleep"), icon: "system-suspend" },
        "hibernate":   { text: i18n("Hibernate"), icon: "system-suspend-hibernate" },
        "reboot":      { text: i18n("Restart"), icon: "system-reboot" },
        "shutdown":    { text: i18n("Shut down"), icon: "system-shutdown" }
    })

    function powerItems(ids) {
        return ids.filter(id => powerCatalog[id] && root.systemAction(id) >= 0).map(id => ({
            text: powerCatalog[id].text,
            icon: powerCatalog[id].icon,
            run: () => root.runSystemAction(id)
        }));
    }

    function showUserMenu(button) {
        contextMenu.show(button, null, -1, [
            { text: i18n("Change account settings"), icon: "system-users",
              run: () => { root.run("systemsettings kcm_users"); root.close(); } }
        ].concat(powerItems(["lock-screen", "logout", "switch-user"])));
    }

    function showPowerMenu(button) {
        if (cfg.powerButtonAction !== "menu" && root.systemAction(cfg.powerButtonAction) >= 0) {
            root.runSystemAction(cfg.powerButtonAction);
        } else {
            contextMenu.show(button, null, -1, powerItems(cfg.powerItems));
        }
    }

    function activate(id, button) {
        const item = info(id);
        if (!item) return;
        if (item.url) {
            root.openUrl(item.url);
        } else if (item.cmd) {
            root.run(item.cmd);
            root.close();
        } else if (item.configure) {
            root.close();
            root.openSettings();
        } else if (item.menu === "power") {
            showPowerMenu(button);
        } else if (item.menu === "user") {
            showUserMenu(button);
        }
    }
}

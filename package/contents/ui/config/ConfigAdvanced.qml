import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtQuick.Dialogs as Dialogs
import QtCore
import org.kde.kirigami as Kirigami
import org.kde.newstuff as NewStuff
import "../code/tiles.js" as Tiles
import org.kde.kcmutils as KCM
import org.kde.plasma.plasma5support as P5Support

// Advanced: system icon theme and wallpaper (apply at once), backup, restore, reset (proposal 3.9).
// No icons or wallpapers shipped (copyright).
KCM.SimpleKCM {
    id: page
    property string wallpaper: ""
    property string cfg_resetRequest

    // Themes with Directories= in index.theme (cursor-only themes have none).
    readonly property string themeList: "sh -c 'for d in /usr/share/icons/* \"$HOME\"/.local/share/icons/*; do grep -qs ^Directories= \"$d/index.theme\" && basename \"$d\"; done | sort -u'"
    readonly property string themeNow: "kreadconfig6 --file kdeglobals --group Icons --key Theme"

    P5Support.DataSource {
        id: executable
        engine: "executable"
        connectedSources: []
        onNewData: (sourceName, data) => {
            disconnectSource(sourceName);
            if (sourceName === page.themeList) {
                iconBox.model = (data.stdout || "").trim().split("\n").filter(n => n);
                executable.connectSource(page.themeNow);
            } else if (sourceName === page.themeNow) {
                iconBox.currentIndex = iconBox.model.indexOf((data.stdout || "").trim());
            }
        }
    }
    Component.onCompleted: executable.connectSource(themeList)
    function run(cmd) { executable.connectSource(cmd); }

    Dialogs.FileDialog {
        id: wallFile
        title: i18n("Pick a wallpaper…")
        nameFilters: [i18n("Images (*.png *.jpg *.jpeg *.webp)")]
        onAccepted: page.wallpaper = decodeURIComponent(selectedFile.toString().replace("file://", ""))
    }

    Kirigami.FormLayout {
        Kirigami.Separator { Kirigami.FormData.isSection: true; Kirigami.FormData.label: i18n("Icons and wallpaper (applied at once, not on Apply)") }
        QQC2.ComboBox {
            id: iconBox
            Kirigami.FormData.label: i18n("Icon theme:")
            onActivated: page.run("/usr/lib/plasma-changeicons " + Tiles.shQuote(currentText))
        }
        NewStuff.Button {
            configFile: "icons.knsrc"
            text: i18n("Download icon themes…")
            onEntryEvent: executable.connectSource(page.themeList)
        }

        NewStuff.Button {
            configFile: "wallpaper.knsrc"
            text: i18n("Download wallpapers…")
        }
        RowLayout {
            Kirigami.FormData.label: i18n("Wallpaper:")
            QQC2.Button {
                text: i18n("Choose image…")
                icon.name: "document-open"
                onClicked: wallFile.open()
            }
            QQC2.Button {
                text: i18n("Set as wallpaper")
                enabled: page.wallpaper !== ""
                onClicked: page.run("plasma-apply-wallpaperimage " + Tiles.shQuote(page.wallpaper))
            }
        }
        QQC2.Label {
            Layout.fillWidth: true
            visible: page.wallpaper !== ""
            elide: Text.ElideMiddle
            text: page.wallpaper
        }
        Kirigami.Separator { Kirigami.FormData.isSection: true; Kirigami.FormData.label: i18n("Backup and reset") }
        RowLayout {
            QQC2.Button {
                text: i18n("Backup settings…")
                icon.name: "document-save"
                onClicked: backupFile.open()
            }
            QQC2.Button {
                text: i18n("Restore settings…")
                icon.name: "document-open"
                onClicked: restoreFile.open()
            }
        }
        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: i18n("Settings, tiles and pinned icons go into one file. After picking the file, press OK/Apply to save or restore it.")
        }
        QQC2.Button {
            text: i18n("Reset all Win12 settings…")
            icon.name: "edit-reset"
            onClicked: allDialog.open()
        }
    }

    // T13: main.qml writes/reads the file (resetRequest "backup|<time>|<file>" / "restore|<time>|<file>").
    Dialogs.FileDialog {
        id: backupFile
        title: i18n("Backup Win12 settings")
        fileMode: Dialogs.FileDialog.SaveFile
        defaultSuffix: "json"
        selectedFile: StandardPaths.writableLocation(StandardPaths.HomeLocation) + "/win12-backup.json"
        nameFilters: [i18n("Win12 backup (*.json)")]
        onAccepted: page.cfg_resetRequest = "backup|" + Date.now() + "|" + decodeURIComponent(selectedFile.toString().replace("file://", ""))
    }
    Dialogs.FileDialog {
        id: restoreFile
        title: i18n("Restore Win12 settings")
        nameFilters: [i18n("Win12 backup (*.json)")]
        onAccepted: page.cfg_resetRequest = "restore|" + Date.now() + "|" + decodeURIComponent(selectedFile.toString().replace("file://", ""))
    }

    Kirigami.PromptDialog {
        id: allDialog
        title: i18n("Reset all Win12 settings?")
        subtitle: i18n("Every setting, tile and pinned icon goes back to the fresh-install default. If the XP taskbar theme is active, your previous Plasma Style is put back. Press OK in the settings window afterwards to finish; it then closes, so no old values are written back.")
        standardButtons: Kirigami.Dialog.Ok | Kirigami.Dialog.Cancel
        onAccepted: page.cfg_resetRequest = "all|" + Date.now()
    }
}

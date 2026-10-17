import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtQuick.Dialogs as Dialogs
import org.kde.kirigami as Kirigami
import org.kde.newstuff as NewStuff
import "../code/tiles.js" as Tiles
import org.kde.kcmutils as KCM
import org.kde.plasma.plasma5support as P5Support

// Icons page: KDE's own icon theme and wallpaper tools, no icons or wallpapers shipped (copyright).
KCM.SimpleKCM {
    id: page
    property string wallpaper: ""
    property alias cfg_xpIcons: xpBox.checked
    property string cfg_xpIconTheme
    // Default XP choice: the first theme with "xp" in its folder (main.qml also checks the Name= line).
    readonly property var xpThemes: (iconBox.model || []).filter(n => n.toLowerCase().includes("xp"))

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
        QQC2.ComboBox {
            id: iconBox
            Kirigami.FormData.label: i18n("Icon theme:")
            onActivated: page.run("/usr/lib/plasma-changeicons " + Tiles.shQuote(currentText))
        }
        QQC2.CheckBox {
            id: xpBox
            text: i18n("Use XP icons with the XP style")
        }
        QQC2.ComboBox {
            Kirigami.FormData.label: i18n("XP icon theme:")
            enabled: xpBox.checked
            model: iconBox.model
            currentIndex: (model || []).indexOf(page.cfg_xpIconTheme || page.xpThemes[0] || "")
            onActivated: page.cfg_xpIconTheme = currentText
        }
        QQC2.Label {
            Layout.fillWidth: true
            visible: page.cfg_xpIconTheme === "" && page.xpThemes.length === 0
            wrapMode: Text.Wrap
            text: i18n("No XP icon theme is installed. Get one with the button below (search for \"XP\").")
        }
        NewStuff.Button {
            Kirigami.FormData.label: i18n("Get more:")
            configFile: "icons.knsrc"
            text: i18n("Download icon themes…")
            onChangedEntriesChanged: executable.connectSource(page.themeList)
        }

        Item { Kirigami.FormData.isSection: true }
        NewStuff.Button {
            Kirigami.FormData.label: i18n("Wallpapers:")
            configFile: "wallpaper.knsrc"
            text: i18n("Download wallpapers…")
        }
        RowLayout {
            Kirigami.FormData.label: i18n("Your own wallpaper:")
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
    }
}

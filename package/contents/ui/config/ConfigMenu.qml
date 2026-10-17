pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM
import org.kde.plasma.plasma5support as P5Support
import "../code/tiles.js" as Tiles

// Start11Config "Configure menu": tiles and the apps list (Windows 10 style).
KCM.SimpleKCM {
    id: page
    property alias cfg_tileColumns: columns.value
    property alias cfg_tileGap: gap.value
    property alias cfg_tileIconScale: tileIcon.value
    property alias cfg_hideTileText: hideText.checked
    property alias cfg_tilesLocked: locked.checked
    property alias cfg_listIconSize: listIcon.value
    property alias cfg_listRowHeight: rowHeight.value
    property alias cfg_showRecentlyAdded: recent.checked
    property alias cfg_showMostUsed: mostUsed.checked
    property alias cfg_recentHideFolders: hideRecentFolders.checked
    property alias cfg_webFavicons: webFavicons.checked
    property alias cfg_categoryFolders: folders.checked
    property alias cfg_foldersAtBottom: foldersBottom.checked
    property alias cfg_autoExpand: autoExpand.checked
    property alias cfg_desktopInTree: desktopInTree.checked
    property alias cfg_allAppsAsMenu: allAppsAsMenu.checked
    property alias cfg_highlightNew: highlightNew.checked
    property alias cfg_hideAppList: hideList.checked
    property var cfg_hiddenApps: []
    property string cfg_filteredFolders
    property string cfg_resetRequest
    property string cfg_menuStyle   // read only
    property alias cfg_win12ListSide: listSide.currentIndex
    property alias cfg_win12Greeting: greeting.currentIndex
    property alias cfg_win12Chips: chips.checked
    property alias cfg_win12Clock: clock.checked

    P5Support.DataSource {
        id: executable
        engine: "executable"
        connectedSources: []
        onNewData: sourceName => disconnectSource(sourceName)
    }

    Kirigami.FormLayout {
        // T40: opens the menu in design mode (main.qml watches designRequest), written straight to the
        // widget's config like contents/extras/win12-settings so it needs no Apply.
        QQC2.Button {
            Kirigami.FormData.label: i18n("Section dividers:")
            text: i18n("Edit menu layout…")
            icon.name: "transform-move-horizontal"
            onClicked: executable.connectSource("" + Tiles.QDBUS + " org.kde.plasmashell /PlasmaShell evaluateScript '"
                + "panels().concat(desktops()).forEach(function (c) { c.widgets(\"org.win12.menu\").forEach(function (w) {"
                + " w.currentConfigGroup = [\"General\"]; w.writeConfig(\"designRequest\", \"" + Date.now() + "\"); w.reloadConfig(); }); });'")
        }
        Kirigami.Separator { Kirigami.FormData.isSection: true; Kirigami.FormData.label: i18n("Tiles") }

        QQC2.SpinBox {
            id: columns
            Kirigami.FormData.label: i18n("Show icons as tiles split into:")
            from: 3
            to: 8
            textFromValue: v => i18np("%1 column", "%1 columns", v)
        }
        QQC2.SpinBox {
            id: gap
            Kirigami.FormData.label: i18n("Space between tiles:")
            from: 0
            to: 16
            textFromValue: v => v + " px"
            valueFromText: t => parseInt(t) || 0
        }
        RowLayout {
            Kirigami.FormData.label: i18n("Tile icon size:")
            QQC2.Slider { id: tileIcon; from: 25; to: 90; stepSize: 1 }
            QQC2.Label { text: Math.round(tileIcon.value) + " %" }
        }
        QQC2.CheckBox { id: hideText; text: i18n("Hide caption on tiles (show it as a tooltip)") }
        QQC2.CheckBox { id: locked; text: i18n("Lock menu shortcut positions") }

        Kirigami.Separator { Kirigami.FormData.isSection: true; Kirigami.FormData.label: i18n("Apps list") }

        QQC2.CheckBox { id: hideList; text: i18n("Hide the apps list (tiles only)") }
        QQC2.SpinBox {
            id: listIcon
            Kirigami.FormData.label: i18n("Shortcut icons should be sized:")
            enabled: !hideList.checked
            from: 12
            to: 64
            textFromValue: v => v + " px"
            valueFromText: t => parseInt(t) || 22
        }
        QQC2.SpinBox {
            id: rowHeight
            Kirigami.FormData.label: i18n("Row height:")
            enabled: !hideList.checked
            from: 20
            to: 80
            textFromValue: v => v + " px"
            valueFromText: t => parseInt(t) || 36
        }
        QQC2.CheckBox { id: recent; enabled: !hideList.checked; text: i18n("Show recently added apps") }
        QQC2.CheckBox { id: hideRecentFolders; text: i18n("Hide folders in the recent list (Windows 11 style)") }
        QQC2.CheckBox { id: webFavicons; text: i18n("Fetch the favicon for pinned web links") }
        QQC2.CheckBox { id: mostUsed; enabled: !hideList.checked; text: i18n("Show my most used applications") }
        QQC2.CheckBox { id: highlightNew; enabled: !hideList.checked; text: i18n("Highlight new applications") }
        QQC2.CheckBox { id: folders; enabled: !hideList.checked; text: i18n("Show app categories as folders") }
        QQC2.CheckBox { id: foldersBottom; enabled: !hideList.checked && folders.checked; text: i18n("Place folders at the bottom of the list") }
        QQC2.CheckBox { id: autoExpand; enabled: !hideList.checked; text: i18n("Auto expand folders on hover (Windows 7, XP)") }
        QQC2.CheckBox { id: desktopInTree; enabled: !hideList.checked; text: i18n("Show the Desktop folder in All Programs (Windows 7, XP)") }
        QQC2.CheckBox { id: allAppsAsMenu; enabled: !hideList.checked && folders.checked; text: i18n("Show categories as a cascading menu (Windows 7, XP)") }

        Kirigami.Separator { Kirigami.FormData.isSection: true; Kirigami.FormData.label: i18n("Windows 12 style"); visible: page.cfg_menuStyle === "windows12" }
        QQC2.ComboBox {
            id: listSide
            visible: page.cfg_menuStyle === "windows12"
            Kirigami.FormData.label: i18n("Apps list side:")
            model: [i18n("Automatic (menu on the left: left, else right)"), i18n("Left"), i18n("Right")]
        }
        QQC2.ComboBox {
            id: greeting
            visible: page.cfg_menuStyle === "windows12"
            Kirigami.FormData.label: i18n("Greeting with picture:")
            model: [i18n("Automatic (when the list is on the left)"), i18n("On"), i18n("Off")]
        }
        QQC2.CheckBox { id: chips; visible: page.cfg_menuStyle === "windows12"; text: i18n("Show Documents, Pictures and Music above the cards") }
        QQC2.CheckBox { id: clock; visible: page.cfg_menuStyle === "windows12"; text: i18n("Show the clock and date above the cards") }

        Kirigami.Separator { Kirigami.FormData.isSection: true; Kirigami.FormData.label: i18n("Hidden apps") }

        QQC2.Label { visible: page.cfg_hiddenApps.length === 0; text: i18n("None. Right-click an app in the list and choose \"Hide from apps\".") }
        Repeater {
            model: page.cfg_hiddenApps
            delegate: RowLayout {
                required property string modelData
                QQC2.Label { text: parent.modelData.replace(/\.desktop$/, "") }
                QQC2.Button {
                    text: i18n("Show")
                    onClicked: page.cfg_hiddenApps = page.cfg_hiddenApps.filter(x => x !== parent.modelData)
                }
            }
        }
        QQC2.Button {
            visible: page.cfg_hiddenApps.length > 0
            text: i18n("Clear hidden list")
            onClicked: page.cfg_hiddenApps = []
        }

        // Manage filtered folders: used as App canvas groups and Windows 7 All Programs folders.
        Kirigami.Separator { Kirigami.FormData.isSection: true; Kirigami.FormData.label: i18n("Filtered folders") }

        Repeater {
            model: Tiles.parseFiltered(page.cfg_filteredFolders)
            delegate: RowLayout {
                required property var modelData
                required property int index
                QQC2.Label {
                    text: modelData.name + "  (" + modelData.path + (modelData.include ? ", +" + modelData.include : "")
                          + (modelData.exclude ? ", -" + modelData.exclude : "") + ")"
                }
                QQC2.Button {
                    text: i18n("Remove")
                    onClicked: {
                        const a = Tiles.parseFiltered(page.cfg_filteredFolders);
                        a.splice(index, 1);
                        page.cfg_filteredFolders = JSON.stringify(a);
                    }
                }
            }
        }
        QQC2.TextField { id: ffName; Kirigami.FormData.label: i18n("Name:"); placeholderText: i18n("PDFs") }
        QQC2.TextField { id: ffPath; Kirigami.FormData.label: i18n("Folder:"); placeholderText: "~/Downloads" }
        QQC2.TextField { id: ffInclude; Kirigami.FormData.label: i18n("Include (regex):"); placeholderText: "pdf$" }
        QQC2.TextField { id: ffExclude; Kirigami.FormData.label: i18n("Exclude (regex):") }
        QQC2.Button {
            text: i18n("Add filtered folder")
            icon.name: "list-add"
            enabled: ffName.text.trim() !== "" && ffPath.text.trim() !== ""
            onClicked: {
                const a = Tiles.parseFiltered(page.cfg_filteredFolders);
                a.push({ name: ffName.text.trim(), path: ffPath.text.trim(), include: ffInclude.text, exclude: ffExclude.text });
                page.cfg_filteredFolders = JSON.stringify(a);
                ffName.text = ffPath.text = ffInclude.text = ffExclude.text = "";
            }
        }

        Kirigami.Separator { Kirigami.FormData.isSection: true; Kirigami.FormData.label: i18n("Reset") }
        QQC2.Button {
            text: i18n("Reset icon positions…")
            icon.name: "edit-reset"
            onClicked: positionsDialog.open()
        }
    }

    Kirigami.PromptDialog {
        id: positionsDialog
        title: i18n("Reset icon positions?")
        subtitle: i18n("Tiles, pinned icons and pin names go back to the fresh-install layout. Press OK in the settings window afterwards to finish; it then closes, so no old values are written back.")
        standardButtons: Kirigami.Dialog.Ok | Kirigami.Dialog.Cancel
        onAccepted: page.cfg_resetRequest = "positions|" + Date.now()
    }
}

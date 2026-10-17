pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM
import "../code/tiles.js" as Tiles

// Start11Config "Configure menu": tiles and the apps list (Windows 10 style).
KCM.SimpleKCM {
    id: page
    property alias cfg_listIconSize: listIcon.value
    property alias cfg_listRowHeight: rowHeight.value
    property alias cfg_showRecentlyAdded: recent.checked
    property alias cfg_showMostUsed: mostUsed.checked
    property alias cfg_recentHideFolders: hideRecentFolders.checked
    property alias cfg_categoryFolders: folders.checked
    property alias cfg_foldersAtBottom: foldersBottom.checked
    property alias cfg_autoExpand: autoExpand.checked
    property alias cfg_desktopInTree: desktopInTree.checked
    property alias cfg_allAppsAsMenu: allAppsAsMenu.checked
    property alias cfg_highlightNew: highlightNew.checked
    property alias cfg_hideAppList: hideList.checked
    property var cfg_hiddenApps: []
    property string cfg_filteredFolders

    Kirigami.FormLayout {
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
        QQC2.CheckBox { id: mostUsed; enabled: !hideList.checked; text: i18n("Show my most used applications") }
        QQC2.CheckBox { id: highlightNew; enabled: !hideList.checked; text: i18n("Highlight new applications") }
        QQC2.CheckBox { id: folders; enabled: !hideList.checked; text: i18n("Show app categories as folders") }
        QQC2.CheckBox { id: foldersBottom; enabled: !hideList.checked && folders.checked; text: i18n("Place folders at the bottom of the list") }
        QQC2.CheckBox { id: autoExpand; enabled: !hideList.checked; text: i18n("Auto expand folders on hover (Windows 7, XP)") }
        QQC2.CheckBox { id: desktopInTree; enabled: !hideList.checked; text: i18n("Show the Desktop folder in All Programs (Windows 7, XP)") }
        QQC2.CheckBox { id: allAppsAsMenu; enabled: !hideList.checked && folders.checked; text: i18n("Show categories as a cascading menu (Windows 7, XP)") }

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
    }
}

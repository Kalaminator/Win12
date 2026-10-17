pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM
import org.kde.kquickcontrols as KQuickControls
import QtQuick.Dialogs as Dialogs
import QtCore

// Start11Config "Start Menu" page, here "Style": a card sets the menu, the taskbar and the windows (T43).
KCM.SimpleKCM {
    id: page
    property string cfg_menuStyle
    property alias cfg_menuSize: sizeBox.currentIndex
    property alias cfg_menuPosition: positionBox.currentIndex
    property alias cfg_centerTaskbar: centerTaskbarBox.checked
    property alias cfg_menuHeight: heightSpin.value
    property alias cfg_fullScreen: fullScreenBox.checked
    property alias cfg_keepPanelVisible: keepPanelBox.checked
    property real cfg_uiScale
    property alias cfg_win7Substyle: subBox.currentIndex
    property alias cfg_modernSubstyle: modernSubBox.currentIndex
    property string cfg_xpSubstyle
    property bool cfg_xpStartGreen
    property bool cfg_xpStartCustom
    property alias cfg_xpStartColor: xpStartColor.color
    property string cfg_xpColor
    property alias cfg_xpCustomColor: xpCustom.color
    property string cfg_resetRequest
    // T43 mix and match ("" = same as the style). "Taskbar: none" = taskbarFollowsStyle + xpTaskbarTheme off,
    // "Windows: none" = windowTheme off (the old Taskbar page switches, so saved configs keep working).
    property string cfg_menuLook
    property string cfg_taskbarLook
    property string cfg_windowLook
    property bool cfg_taskbarFollowsStyle
    property bool cfg_xpTaskbarTheme
    property bool cfg_windowTheme
    property string cfg_taskbarLayout
    property bool cfg_taskbarMagnetic
    function setMenu(v) { if (cfg_menuLook) cfg_menuLook = v; else cfg_menuStyle = v; }

    StyleKinds {
        id: kinds
        menuStyle: page.cfg_menuStyle; menuLook: page.cfg_menuLook; taskbarLook: page.cfg_taskbarLook
        taskbarFollowsStyle: page.cfg_taskbarFollowsStyle; windowLook: page.cfg_windowLook; windowTheme: page.cfg_windowTheme
    }

    // Vertical panel (Types.LeftEdge 5 / RightEdge 6): positions read Top/Middle/Bottom. `plasmoid` is the
    // applet in the config dialog; guarded in case it is missing.
    readonly property bool vertical: typeof plasmoid !== "undefined" && !!plasmoid && (plasmoid.location === 5 || plasmoid.location === 6)

    readonly property var built: ["windows7", "modern", "windows10", "windows11", "windows12", "app", "pro", "launcher", "xp", "plasma"]
    readonly property var styles: [
        // "modern" is the Windows 7 card's Modern look (picked below), not a card of its own.
        ["windows7", i18n("Windows 7 style"), i18n("The Windows 7 menu, classic or modern.")],
        ["windows10", i18n("Windows 10 style"), i18n("Windows 10 style with improvements.")],
        ["windows11", i18n("Windows 11 style"), i18n("Windows 11 style with improvements.")],
        ["windows12", i18n("Windows 12 style"), i18n("Concept-art menu: app list beside pinned cards and widgets.")],
        ["app", i18n("Windows App style"), i18n("Shortcuts at the top, groups and tabs, all apps always open.")],
        ["pro", i18n("Windows Pro style"), i18n("Two-tone: pinned apps, and a places panel on the right.")],
        ["launcher", i18n("Windows Launcher style"), i18n("Big account picture, search, and a large pinned grid.")],
        ["xp", i18n("Windows XP style"), i18n("The blue Luna menu, classic or modernised.")],
        ["plasma", i18n("KDE Plasma"), i18n("Plasma's own launcher look, taskbar and title bars.")]
    ]

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        Kirigami.Heading {
            level: 2
            text: i18n("Which style would you prefer?")
        }
        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: i18n("A style sets the Start menu, the taskbar and the window title bars together. Mix them below.")
        }

        Flow {
            Layout.fillWidth: true
            spacing: Kirigami.Units.largeSpacing

            Repeater {
                model: page.styles
                delegate: QQC2.ItemDelegate {
                    id: card
                    required property var modelData
                    readonly property bool ready: page.built.includes(modelData[0])
                    readonly property bool chosen: page.cfg_menuStyle === modelData[0]
                                                   || (modelData[0] === "windows7" && page.cfg_menuStyle === "modern")
                    width: Kirigami.Units.gridUnit * 11
                    height: Kirigami.Units.gridUnit * 11
                    enabled: ready
                    highlighted: chosen
                    onClicked: if (!chosen) page.cfg_menuStyle = modelData[0]

                    contentItem: ColumnLayout {
                        StylePreview {
                            Layout.fillWidth: true
                            Layout.preferredHeight: Kirigami.Units.gridUnit * 6.5
                            styleId: card.modelData[0]
                            opacity: card.ready ? 1 : 0.4
                        }
                        QQC2.Label {
                            Layout.fillWidth: true
                            text: card.modelData[1]
                            font.bold: card.chosen
                            elide: Text.ElideRight
                        }
                        QQC2.Label {
                            Layout.fillWidth: true
                            text: card.ready ? card.modelData[2] : i18n("Coming soon")
                            opacity: 0.7
                            font: Kirigami.Theme.smallFont
                            wrapMode: Text.WordWrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }

        Kirigami.FormLayout {
            Layout.fillWidth: true

            // T43: [value, label]; "" = same as the style, "none" = KDE Plasma (default), Plasma's own look.
            QQC2.ComboBox {
                Kirigami.FormData.label: i18n("Start menu:")
                readonly property var items: [["", i18n("Same as the style")], ["windows7", i18n("Windows 7 style")],
                    ["modern", i18n("Modern style")], ["windows10", i18n("Windows 10 style")], ["windows11", i18n("Windows 11 style")],
                    ["windows12", i18n("Windows 12 style")], ["app", i18n("Windows App style")], ["pro", i18n("Windows Pro style")],
                    ["launcher", i18n("Windows Launcher style")], ["xp", i18n("Windows XP style")], ["plasma", i18n("KDE Plasma (default)")]]
                model: items.map(i => i[1])
                currentIndex: Math.max(0, items.findIndex(i => i[0] === page.cfg_menuLook))
                onActivated: index => page.cfg_menuLook = items[index][0]
            }
            QQC2.ComboBox {
                Kirigami.FormData.label: i18n("Taskbar:")
                // main.qml taskbarPresets + the XP Plasma Style; the other styles use the Windows 11 preset.
                readonly property var items: [["", i18n("Same as the style")], ["none", i18n("KDE Plasma (default)")],
                    ["xp", i18n("Windows XP style")], ["windows7", i18n("Windows 7 style")], ["windows10", i18n("Windows 10 style")],
                    ["windows11", i18n("Windows 11 style")], ["windows12", i18n("Windows 12 style")]]
                model: items.map(i => i[1])
                currentIndex: !page.cfg_taskbarFollowsStyle ? 1 : Math.max(0, items.findIndex(i => i[0] === page.cfg_taskbarLook))
                onActivated: index => {
                    const v = items[index][0];
                    page.cfg_taskbarFollowsStyle = page.cfg_xpTaskbarTheme = v !== "none";
                    if (v !== "none") page.cfg_taskbarLook = v;
                }
            }
            QQC2.ComboBox {
                Kirigami.FormData.label: i18n("Windows:")
                // main.qml windowDecoration(): the other styles use the Windows 11 title bars.
                readonly property var items: [["", i18n("Same as the style")], ["none", i18n("KDE Plasma (default)")],
                    ["xp", i18n("Windows XP style")], ["windows7", i18n("Windows 7 style")], ["windows10", i18n("Windows 10 style")],
                    ["windows11", i18n("Windows 11 style")]]
                model: items.map(i => i[1])
                currentIndex: !page.cfg_windowTheme ? 1 : Math.max(0, items.findIndex(i => i[0] === page.cfg_windowLook))
                onActivated: index => {
                    const v = items[index][0];
                    page.cfg_windowTheme = v !== "none";
                    if (v !== "none") page.cfg_windowLook = v;
                }
            }
            QQC2.Label {
                text: i18n("KDE Plasma (default): your own Plasma Style and title bars come back.")
                enabled: false
            }
            TaskbarLayoutBox {
                visible: kinds.taskbarKind === "windows12"
                Kirigami.FormData.label: i18n("Taskbar layout:")
                cfgPage: page
            }
            Item { Kirigami.FormData.isSection: true }

            QQC2.ComboBox {
                visible: kinds.menuKind === "windows7" || kinds.menuKind === "modern"
                Kirigami.FormData.label: i18n("Look:")
                model: [i18n("Classic"), i18n("Modern")]
                currentIndex: kinds.menuKind === "modern" ? 1 : 0
                onActivated: index => page.setMenu(index === 1 ? "modern" : "windows7")
            }
            QQC2.ComboBox {
                id: subBox
                visible: kinds.menuKind === "windows7"
                Kirigami.FormData.label: i18n("Substyle:")
                model: [i18n("Windows 7 style"), i18n("Windows 7 style with icons"), i18n("Compact Windows 7 style"),
                        i18n("Grid Windows 7 style"), i18n("Grid Windows 7 style with icons"), i18n("Compact Grid Windows 7 style")]
            }
            QQC2.ComboBox {
                id: modernSubBox
                visible: kinds.menuKind === "modern"
                Kirigami.FormData.label: i18n("Substyle:")
                model: [i18n("Modern style"), i18n("Modern style with icons"), i18n("Compact Modern style"),
                        i18n("Grid Modern style"), i18n("Grid Modern style with icons"), i18n("Compact Grid Modern style")]
            }
            QQC2.ComboBox {
                visible: kinds.anyXp
                Kirigami.FormData.label: i18n("Substyle:")
                // Ids stay "classic"/"modern" for saved configs; "luna" = the real XP look.
                readonly property var ids: ["luna", "classic", "modern"]
                model: [i18n("Classic XP"), i18n("Modern classic"), i18n("Modern")]
                currentIndex: Math.max(0, ids.indexOf(page.cfg_xpSubstyle))
                onActivated: index => page.cfg_xpSubstyle = ids[index]
            }
            RowLayout {
                visible: kinds.anyXp
                Kirigami.FormData.label: i18n("Colour scheme:")
                QQC2.ComboBox {
                    readonly property var keys: ["blue", "olive", "silver", "custom"]
                    model: [i18n("Blue"), i18n("Olive Green"), i18n("Silver"), i18n("Custom")]
                    currentIndex: Math.max(0, keys.indexOf(page.cfg_xpColor))
                    onActivated: index => page.cfg_xpColor = keys[index]
                }
                KQuickControls.ColorButton { id: xpCustom; visible: page.cfg_xpColor === "custom"; showAlphaChannel: false }
            }
            RowLayout {
                visible: kinds.menuKind === "xp"
                Kirigami.FormData.label: i18n("Start button colour:")
                QQC2.ComboBox {
                    model: [i18n("XP green"), i18n("Colour scheme"), i18n("Custom")]
                    currentIndex: page.cfg_xpStartCustom ? 2 : page.cfg_xpStartGreen ? 0 : 1
                    onActivated: index => {
                        page.cfg_xpStartCustom = index === 2;
                        page.cfg_xpStartGreen = index === 0;
                    }
                }
                KQuickControls.ColorButton { id: xpStartColor; visible: page.cfg_xpStartCustom; showAlphaChannel: false }
            }
            QQC2.ComboBox {
                id: sizeBox
                Kirigami.FormData.label: i18n("What size should it be?")
                model: [i18n("Small"), i18n("Tall"), i18n("Wide (two tile columns)"), i18n("Large (two tile columns)")]
            }
            QQC2.ComboBox {
                id: positionBox
                Kirigami.FormData.label: i18n("Menu position:")
                model: page.vertical ? [i18n("Top"), i18n("Middle"), i18n("Bottom")] : [i18n("Left"), i18n("Center"), i18n("Right")]
            }
            QQC2.CheckBox {
                id: centerTaskbarBox
                visible: positionBox.currentIndex !== 0
                text: positionBox.currentIndex === 2
                    ? (page.vertical ? i18n("Also mirror the taskbar (Start button at the bottom)")
                                     : i18n("Also mirror the taskbar (Start button on the right, clock on the left)"))
                    : (page.vertical ? i18n("Also centre the Start button and the taskbar icons vertically")
                                     : i18n("Also centre the Start button and the taskbar icons"))
            }
            QQC2.SpinBox {
                id: heightSpin
                Kirigami.FormData.label: i18n("Custom height:")
                from: 0
                to: 1600
                stepSize: 20
                textFromValue: v => v === 0 ? i18n("From size") : v + " px"
                valueFromText: t => parseInt(t) || 0
            }
            QQC2.CheckBox {
                id: fullScreenBox
                text: i18n("Use full screen menu")
            }
            QQC2.CheckBox {
                id: keepPanelBox
                enabled: fullScreenBox.checked
                text: i18n("Keep the panel visible")
            }
            RowLayout {
                Kirigami.FormData.label: i18n("Adjust global menu scaling:")
                QQC2.Slider {
                    id: scaleSlider
                    from: 50
                    to: 200
                    stepSize: 5
                    value: page.cfg_uiScale * 100
                    onMoved: page.cfg_uiScale = value / 100
                }
                QQC2.Label {
                    text: i18n("%1% of original size", Math.round(scaleSlider.value))
                }
            }
        }

        Kirigami.Separator { Kirigami.FormData.isSection: true; Kirigami.FormData.label: i18n("Reset") }
        QQC2.Button {
            text: i18n("Reset all Win12 settings…")
            icon.name: "edit-reset"
            onClicked: allDialog.open()
        }

        Kirigami.Separator { Kirigami.FormData.isSection: true; Kirigami.FormData.label: i18n("Backup") }
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

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtQuick.Dialogs as Dialogs
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM
import org.kde.kquickcontrols as KQuickControls
import org.kde.plasma.plasma5support as P5Support
import "../code/tiles.js" as Tiles

// Start menu page: size and position of the menu, the tiles, the Windows 12 extras (proposal 3.2).
KCM.SimpleKCM {
    id: page
    property string cfg_menuStyle   // read only
    property string cfg_menuLook   // read only
    property string cfg_taskbarLook   // read only
    property bool cfg_taskbarFollowsStyle   // read only
    property string cfg_windowLook   // read only
    property bool cfg_windowTheme   // read only
    StyleKinds {
        id: kinds
        menuStyle: page.cfg_menuStyle; menuLook: page.cfg_menuLook; taskbarLook: page.cfg_taskbarLook
        taskbarFollowsStyle: page.cfg_taskbarFollowsStyle; windowLook: page.cfg_windowLook; windowTheme: page.cfg_windowTheme
    }
    property alias cfg_menuSize: sizeBox.currentIndex
    property alias cfg_menuPosition: positionBox.currentIndex
    property alias cfg_centerTaskbar: centerTaskbarBox.checked
    property alias cfg_menuHeight: heightSpin.value
    property alias cfg_fullScreen: fullScreenBox.checked
    property alias cfg_keepPanelVisible: keepPanelBox.checked
    property real cfg_uiScale
    property alias cfg_tileColumns: columns.value
    property alias cfg_tileGap: gap.value
    property alias cfg_tileIconScale: tileIcon.value
    property alias cfg_hideTileText: hideText.checked
    property alias cfg_tilesLocked: locked.checked
    property alias cfg_webFavicons: webFavicons.checked
    property alias cfg_tileColorMode: tileMode.currentIndex
    property alias cfg_tileColor: tileColor.color
    property alias cfg_tileOpacity: tileOpacity.value
    property string cfg_tileImage
    property alias cfg_tileImageOpacity: tileImageOpacity.value
    property string cfg_resetRequest
    property alias cfg_win12ListSide: listSide.currentIndex
    property alias cfg_win12Greeting: greeting.currentIndex
    property alias cfg_win12Chips: chips.checked
    property alias cfg_win12Clock: clock.checked

    // Vertical panel (Types.LeftEdge 5 / RightEdge 6): positions read Top/Middle/Bottom. `plasmoid` is the
    // applet in the config dialog; guarded in case it is missing.
    readonly property bool vertical: typeof plasmoid !== "undefined" && !!plasmoid && (plasmoid.location === 5 || plasmoid.location === 6)

    P5Support.DataSource {
        id: executable
        engine: "executable"
        connectedSources: []
        onNewData: sourceName => disconnectSource(sourceName)
    }

    Dialogs.FileDialog {
        id: tileImageFile
        title: i18n("Pick tile image…")
        nameFilters: [i18n("Images (*.png *.jpg *.jpeg *.webp *.svg)")]
        onAccepted: page.cfg_tileImage = decodeURIComponent(selectedFile.toString().replace("file://", ""))
    }

    Kirigami.FormLayout {
        Kirigami.Separator { Kirigami.FormData.isSection: true; Kirigami.FormData.label: i18n("Size and position") }

        QQC2.ComboBox {
            id: sizeBox
            Kirigami.FormData.label: i18n("Size:")
            model: [i18n("Small"), i18n("Tall"), i18n("Wide (two tile columns)"), i18n("Large (two tile columns)")]
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
        QQC2.ComboBox {
            id: positionBox
            Kirigami.FormData.label: i18n("Position:")
            model: page.vertical ? [i18n("Top"), i18n("Middle"), i18n("Bottom")] : [i18n("Left"), i18n("Center"), i18n("Right")]
        }
        QQC2.CheckBox {
            id: centerTaskbarBox
            visible: positionBox.currentIndex !== 0
            text: positionBox.currentIndex === 2
                ? (page.vertical ? i18n("Also mirror the taskbar (Start button at the bottom)")
                                 : i18n("Also mirror the taskbar (Start button on the right, clock on the left)"))
                : (page.vertical ? i18n("Also centre the Start button and the taskbar icons vertically")
                                 : i18n("Also align the Start button and taskbar icons with the menu"))
        }
        QQC2.CheckBox { id: fullScreenBox; text: i18n("Full screen menu") }
        QQC2.CheckBox { id: keepPanelBox; enabled: fullScreenBox.checked; text: i18n("Keep the panel visible") }
        RowLayout {
            Kirigami.FormData.label: i18n("Menu scale:")
            QQC2.Slider {
                id: scaleSlider
                from: 50
                to: 200
                stepSize: 5
                value: page.cfg_uiScale * 100
                onMoved: page.cfg_uiScale = value / 100
            }
            QQC2.Label { text: i18n("%1% of original size", Math.round(scaleSlider.value)) }
        }

        Kirigami.Separator { Kirigami.FormData.isSection: true; Kirigami.FormData.label: i18n("Tiles") }

        // T40: opens the menu in design mode (main.qml watches designRequest), written straight to the
        // widget's config like contents/extras/win12-settings so it needs no Apply.
        RowLayout {
            QQC2.Button {
                text: i18n("Edit menu layout…")
                icon.name: "transform-move-horizontal"
                onClicked: executable.connectSource("" + Tiles.QDBUS + " org.kde.plasmashell /PlasmaShell evaluateScript '"
                    + "panels().concat(desktops()).forEach(function (c) { c.widgets(\"org.win12.menu\").forEach(function (w) {"
                    + " w.currentConfigGroup = [\"General\"]; w.writeConfig(\"designRequest\", \"" + Date.now() + "\"); w.reloadConfig(); }); });'")
            }
            QQC2.Label { text: i18n("(applies at once)"); enabled: false }
        }
        QQC2.SpinBox {
            id: columns
            Kirigami.FormData.label: i18n("Tile columns:")
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
        QQC2.CheckBox { id: hideText; text: i18n("Hide tile captions (show them as tooltips)") }
        RowLayout {
            Kirigami.FormData.label: i18n("Tile colour:")
            QQC2.ComboBox { id: tileMode; model: [i18n("Soft (follows text colour)"), i18n("Accent colour"), i18n("Custom")] }
            KQuickControls.ColorButton { id: tileColor; visible: tileMode.currentIndex === 2; showAlphaChannel: false }
        }
        RowLayout {
            Kirigami.FormData.label: i18n("Tile background:")
            QQC2.Slider { id: tileOpacity; from: 0; to: 100; stepSize: 1 }
            QQC2.Label { text: i18n("%1% solid", Math.round(tileOpacity.value)) }
        }
        RowLayout {
            Kirigami.FormData.label: i18n("Tile image:")
            QQC2.Label { text: page.cfg_tileImage ? page.cfg_tileImage.split("/").pop() : i18n("None") }
            QQC2.Button { text: i18n("Pick image…"); icon.name: "document-open"; onClicked: tileImageFile.open() }
            QQC2.Button { icon.name: "edit-clear"; visible: page.cfg_tileImage !== ""; onClicked: page.cfg_tileImage = "" }
        }
        RowLayout {
            Kirigami.FormData.label: i18n("Tile image opacity:")
            enabled: page.cfg_tileImage !== ""
            QQC2.Slider { id: tileImageOpacity; from: 0; to: 100; stepSize: 1 }
            QQC2.Label { text: i18n("%1%", Math.round(tileImageOpacity.value)) }
        }
        QQC2.CheckBox { id: webFavicons; text: i18n("Fetch favicons for pinned web links") }
        QQC2.CheckBox { id: locked; text: i18n("Lock tile and pin positions") }
        QQC2.Button {
            text: i18n("Reset tile and pin positions…")
            icon.name: "edit-reset"
            onClicked: positionsDialog.open()
        }

        Kirigami.Separator { Kirigami.FormData.isSection: true; Kirigami.FormData.label: i18n("Windows 12"); visible: kinds.menuKind === "windows12" }
        QQC2.ComboBox {
            id: listSide
            visible: kinds.menuKind === "windows12"
            Kirigami.FormData.label: i18n("Apps list side:")
            model: [i18n("Automatic (menu on the left: left, else right)"), i18n("Left"), i18n("Right")]
        }
        QQC2.ComboBox {
            id: greeting
            visible: kinds.menuKind === "windows12"
            Kirigami.FormData.label: i18n("Greeting with picture:")
            model: [i18n("Automatic (when the list is on the left)"), i18n("On"), i18n("Off")]
        }
        QQC2.CheckBox { id: chips; visible: kinds.menuKind === "windows12"; text: i18n("Show Documents, Pictures and Music above the cards") }
        QQC2.CheckBox { id: clock; visible: kinds.menuKind === "windows12"; text: i18n("Show the clock and date above the cards") }
    }

    Kirigami.PromptDialog {
        id: positionsDialog
        title: i18n("Reset icon positions?")
        subtitle: i18n("Tiles, pinned icons and pin names go back to the fresh-install layout. Press OK in the settings window afterwards to finish; it then closes, so no old values are written back.")
        standardButtons: Kirigami.Dialog.Ok | Kirigami.Dialog.Cancel
        onAccepted: page.cfg_resetRequest = "positions|" + Date.now()
    }
}

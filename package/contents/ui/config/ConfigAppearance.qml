import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtQuick.Dialogs as Dialogs
import Qt.labs.folderlistmodel
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM
import org.kde.kquickcontrols as KQuickControls

// Appearance: colours, Windows 12 shade, transparency and corners, texture, fonts, effects (proposal 3.7).
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
    // XP keeps its own scheme colours (Style page); the Windows 12 rows tint menu, taskbar and title bars alike.
    readonly property bool colours: kinds.menuKind !== "xp"
    readonly property bool win12: kinds.anyKind("windows12")

    property int cfg_colorMode
    property alias cfg_menuColor: menuColor.color
    property alias cfg_textColor: textColor.color
    property int cfg_accentMode
    property color cfg_accentColor
    property var cfg_recentAccents: []
    property bool cfg_win10AccentStart
    property bool cfg_win10AccentTitles
    property bool cfg_win10Transparency
    property int cfg_win12Light
    property int cfg_win12Dark
    property int cfg_win12Transparency
    property int cfg_win12Blur
    property int cfg_win12Noise
    property alias cfg_menuOpacity: opacity.value
    property alias cfg_solidMenu: solid.checked
    property alias cfg_cornerRadius: radius.value
    property alias cfg_greyIcons: greyIcons.checked
    property alias cfg_animateMenu: animateMenu.checked
    property string cfg_texture
    property alias cfg_textureMode: textureMode.currentIndex
    property alias cfg_textureOpacity: textureOpacity.value
    property alias cfg_darkenTexture: darken.checked
    property alias cfg_colorizeTexture: colorize.checked
    property string cfg_fontFamily
    property alias cfg_fontSize: fontSize.value
    property string cfg_headerFontFamily
    property alias cfg_headerFontSize: headerFontSize.value
    property alias cfg_itemFontSize: itemFontSize.value

    // The standard Windows 10/11 accent palette, 8 per row like Windows.
    readonly property var windowsColours: ["#ffb900", "#ff8c00", "#f7630c", "#ca5010", "#da3b01", "#ef6950", "#d13438", "#ff4343",
        "#e74856", "#e81123", "#ea005e", "#c30052", "#e3008c", "#bf0077", "#c239b3", "#9a0089",
        "#0078d7", "#0063b1", "#8e8cd8", "#6b69d6", "#8764b8", "#744da9", "#b146c2", "#881798",
        "#0099bc", "#2d7d9a", "#00b7c3", "#038387", "#00b294", "#018574", "#00cc6a", "#10893e",
        "#7a7574", "#5d5a58", "#68768a", "#515c6b", "#567c73", "#486860", "#498205", "#107c10",
        "#767676", "#4c4a48", "#69797e", "#4a5459", "#647c64", "#525e54", "#847545", "#7e735f"]
    readonly property string current: cfg_accentMode === 1 ? cfg_accentColor.toString() : ""

    // Picking a colour = Custom accent, and it goes first in "Recent colours" (5 kept).
    function pick(hex) {
        hex = hex.toLowerCase();
        cfg_accentColor = hex;
        cfg_accentMode = 1;
        cfg_recentAccents = [hex].concat(Array.from(cfg_recentAccents).filter(c => c !== hex)).slice(0, 5);
    }

    component Swatch: QQC2.AbstractButton {
        required property string hex
        implicitWidth: Kirigami.Units.gridUnit * 2
        implicitHeight: implicitWidth
        Accessible.name: hex
        QQC2.ToolTip.text: hex
        QQC2.ToolTip.visible: hovered
        onClicked: page.pick(hex)
        background: Rectangle {
            color: parent.hex
            radius: 3
            border.width: parent.hex === page.current ? 2 : parent.hovered ? 1 : 0
            border.color: Kirigami.Theme.textColor
        }
        contentItem: Kirigami.Icon {
            source: "checkmark"
            visible: parent.hex === page.current
            color: Kirigami.ColorUtils.brightnessForColor(parent.hex) === Kirigami.ColorUtils.Dark ? "white" : "black"
            isMask: true
        }
    }

    FolderListModel {
        id: textures
        folder: Qt.resolvedUrl("../../images/MenuTextures/")
        nameFilters: ["*.png", "*.jpg"]
        showDirs: false
    }
    readonly property var textureNames: Array.from({ length: textures.count }, (_, i) => textures.get(i, "fileName"))

    Dialogs.FileDialog {
        id: textureFile
        title: i18n("Pick custom texture…")
        nameFilters: [i18n("Images (*.png *.jpg *.jpeg *.webp *.svg)")]
        onAccepted: page.cfg_texture = decodeURIComponent(selectedFile.toString().replace("file://", ""))
    }

    Dialogs.ColorDialog {
        id: colourDialog
        selectedColor: page.cfg_accentColor
        onAccepted: page.pick(selectedColor.toString())
    }

    Kirigami.FormLayout {
        Kirigami.Separator { Kirigami.FormData.isSection: true; Kirigami.FormData.label: i18n("Colours") }
        QQC2.Label {
            visible: !page.colours
            text: i18n("Windows XP uses its own colour scheme (Style page).")
            enabled: false
        }
        QQC2.ComboBox {
            visible: page.colours
            Kirigami.FormData.label: i18n("Mode:")
            // modes[index] = stored colorMode (0 Automatic, 1 Dark, 2 Light, 3 Custom), shown in Windows' order.
            readonly property var modes: [2, 1, 0, 3]
            model: [i18n("Light"), i18n("Dark"), i18n("Automatic (Plasma colours)"), i18n("Custom colours")]
            currentIndex: Math.max(0, modes.indexOf(page.cfg_colorMode))
            onActivated: index => page.cfg_colorMode = modes[index]
        }
        KQuickControls.ColorButton { id: menuColor; visible: page.colours && page.cfg_colorMode === 3; Kirigami.FormData.label: i18n("Menu colour:"); showAlphaChannel: false }
        KQuickControls.ColorButton { id: textColor; visible: page.colours && page.cfg_colorMode === 3; Kirigami.FormData.label: i18n("Text colour:"); showAlphaChannel: false }
        QQC2.ComboBox {
            visible: page.colours
            Kirigami.FormData.label: i18n("Accent colour:")
            model: [i18n("Automatic (Plasma accent colour)"), i18n("Custom")]
            currentIndex: page.cfg_accentMode
            onActivated: index => page.cfg_accentMode = index
        }
        Flow {
            Kirigami.FormData.label: i18n("Recent colours:")
            visible: page.colours && page.cfg_recentAccents.length > 0
            spacing: Kirigami.Units.smallSpacing
            Repeater {
                model: page.cfg_recentAccents
                Swatch { required property var modelData; hex: modelData }
            }
        }
        GridLayout {
            Kirigami.FormData.label: i18n("Windows colours:")
            visible: page.colours
            columns: 8
            rowSpacing: Kirigami.Units.smallSpacing
            columnSpacing: Kirigami.Units.smallSpacing
            Repeater {
                model: page.windowsColours
                Swatch { required property var modelData; hex: modelData }
            }
        }
        QQC2.Button {
            Kirigami.FormData.label: i18n("Custom colours:")
            visible: page.colours
            text: i18n("View colours")
            icon.name: "color-picker"
            onClicked: colourDialog.open()
        }
        QQC2.Switch {
            visible: page.colours
            text: i18n("Show accent colour on Start and taskbar")
            checked: page.cfg_win10AccentStart
            onToggled: page.cfg_win10AccentStart = checked
        }
        QQC2.Switch {
            visible: page.colours
            text: i18n("Show accent colour on title bars and window borders")
            checked: page.cfg_win10AccentTitles
            onToggled: page.cfg_win10AccentTitles = checked
        }
        QQC2.Label {
            visible: page.colours
            Layout.maximumWidth: Kirigami.Units.gridUnit * 24
            wrapMode: Text.WordWrap
            font: Kirigami.Theme.smallFont
            opacity: 0.7
            text: i18n("The taskbar and title bars follow these when their look is not \"Plasma's own\" (Style page).")
        }

        // W11 shade sliders (-1 = default look; the slider shows where that look sits)
        Kirigami.Separator { Kirigami.FormData.isSection: true; Kirigami.FormData.label: i18n("Windows 12 shade"); visible: page.win12 }
        RowLayout {
            visible: page.win12
            Kirigami.FormData.label: i18n("Light mode tint:")
            QQC2.Label { text: i18n("Soft tint") }
            QQC2.Slider { from: 0; to: 100; value: page.cfg_win12Light < 0 ? 80 : page.cfg_win12Light; onMoved: page.cfg_win12Light = Math.round(value) }
            QQC2.Label { text: i18n("Near white") }
        }
        RowLayout {
            visible: page.win12
            Kirigami.FormData.label: i18n("Dark mode shade:")
            QQC2.Label { text: i18n("Soft dark") }
            QQC2.Slider { from: 0; to: 100; value: page.cfg_win12Dark < 0 ? 50 : page.cfg_win12Dark; onMoved: page.cfg_win12Dark = Math.round(value) }
            QQC2.Label { text: i18n("Near black") }
        }

        Kirigami.Separator { Kirigami.FormData.isSection: true; Kirigami.FormData.label: i18n("Transparency and corners") }
        QQC2.Switch {
            visible: page.colours
            text: i18n("Transparency effects")
            checked: page.cfg_win10Transparency
            onToggled: page.cfg_win10Transparency = checked
        }
        QQC2.CheckBox { id: solid; text: i18n("Solid Start menu") }
        RowLayout {
            Kirigami.FormData.label: i18n("Start menu background:")
            enabled: !solid.checked
            QQC2.Slider { id: opacity; from: 0; to: 100; stepSize: 1 }
            QQC2.Label { text: i18n("%1% solid", Math.round(opacity.value)) }
        }
        RowLayout {
            visible: page.win12
            Kirigami.FormData.label: i18n("Windows 12 transparency:")
            QQC2.Label { text: i18n("Solid") }
            QQC2.Slider { from: 0; to: 90; value: page.cfg_win12Transparency < 0 ? 70 : page.cfg_win12Transparency; onMoved: page.cfg_win12Transparency = Math.round(value) }
            QQC2.Label { text: i18n("Clear") }
        }
        RowLayout {
            visible: page.win12
            Kirigami.FormData.label: i18n("Windows 12 blur:")
            QQC2.Label { text: i18n("Soft") }
            QQC2.Slider { from: 1; to: 15; stepSize: 1; value: page.cfg_win12Blur > 0 ? page.cfg_win12Blur : 3; onMoved: page.cfg_win12Blur = value }
            QQC2.Label { text: i18n("Strong") }
        }
        RowLayout {
            visible: page.win12
            Kirigami.FormData.label: i18n("Windows 12 grain:")
            QQC2.Label { text: i18n("Smooth") }
            QQC2.Slider { from: 0; to: 14; stepSize: 1; value: page.cfg_win12Noise; onMoved: page.cfg_win12Noise = value }
            QQC2.Label { text: i18n("Grainy") }
        }
        QQC2.Label {
            visible: page.win12
            Layout.maximumWidth: Kirigami.Units.gridUnit * 24
            wrapMode: Text.WordWrap
            enabled: false
            text: i18n("Taskbar, Start menu and title bars; transparency changes only the background, icons and text stay solid. Blur strength sets KWin's blur for all windows while the Windows 12 look is on; your own value comes back when you leave it.")
        }
        QQC2.SpinBox {
            id: radius
            Kirigami.FormData.label: i18n("Menu corner radius:")
            from: 0
            to: 30
            textFromValue: v => v === 0 ? i18n("Square") : v + " px"
            valueFromText: t => parseInt(t) || 0
        }

        Kirigami.Separator { Kirigami.FormData.isSection: true; Kirigami.FormData.label: i18n("Texture") }
        RowLayout {
            Kirigami.FormData.label: i18n("Texture:")
            QQC2.ComboBox {
                id: textureBox
                Layout.minimumWidth: Kirigami.Units.gridUnit * 12
                readonly property bool custom: page.cfg_texture.startsWith("/")
                model: [i18n("None")].concat(page.textureNames.map(n => n.replace(/(_x\d)?\.(png|jpg)$/, "")))
                    .concat(custom ? [page.cfg_texture.split("/").pop()] : [])
                currentIndex: page.cfg_texture === "" ? 0
                    : custom ? model.length - 1 : page.textureNames.indexOf(page.cfg_texture) + 1
                onActivated: i => {
                    if (i === 0) page.cfg_texture = "";
                    else if (i <= page.textureNames.length) page.cfg_texture = page.textureNames[i - 1];
                }
            }
            QQC2.Button { text: i18n("Pick custom texture…"); icon.name: "document-open"; onClicked: textureFile.open() }
        }
        QQC2.ComboBox {
            id: textureMode
            Kirigami.FormData.label: i18n("Placement:")
            enabled: page.cfg_texture !== ""
            model: [i18n("Tile"), i18n("Stretch to show entire texture"), i18n("Fit"), i18n("Fill")]
        }
        RowLayout {
            Kirigami.FormData.label: i18n("Fade texture:")
            enabled: page.cfg_texture !== ""
            QQC2.Slider { id: textureOpacity; from: 5; to: 100; stepSize: 1 }
            QQC2.Label { text: Math.round(textureOpacity.value) + " %" }
        }
        QQC2.CheckBox { id: darken; enabled: page.cfg_texture !== ""; text: i18n("Darken texture") }
        QQC2.CheckBox { id: colorize; enabled: page.cfg_texture !== ""; text: i18n("Recolour the texture with the menu colour") }
        Image {
            visible: page.cfg_texture !== ""
            Layout.preferredWidth: Kirigami.Units.gridUnit * 16
            Layout.preferredHeight: Kirigami.Units.gridUnit * 4
            fillMode: Image.Tile
            source: page.cfg_texture === "" ? ""
                : page.cfg_texture.startsWith("/") ? "file://" + page.cfg_texture
                : Qt.resolvedUrl("../../images/MenuTextures/" + page.cfg_texture)
        }

        Kirigami.Separator { Kirigami.FormData.isSection: true; Kirigami.FormData.label: i18n("Fonts") }
        QQC2.ComboBox {
            Kirigami.FormData.label: i18n("Menu font:")
            Layout.minimumWidth: Kirigami.Units.gridUnit * 14
            model: [i18n("** Default font **")].concat(Qt.fontFamilies())
            currentIndex: page.cfg_fontFamily === "" ? 0 : Math.max(0, model.indexOf(page.cfg_fontFamily))
            onActivated: i => page.cfg_fontFamily = i === 0 ? "" : model[i]
        }
        QQC2.SpinBox {
            id: fontSize
            Kirigami.FormData.label: i18n("Font size:")
            from: 7
            to: 24
            textFromValue: v => v + " px"
            valueFromText: t => parseInt(t) || 11
        }
        QQC2.ComboBox {
            Kirigami.FormData.label: i18n("Header font:")
            Layout.minimumWidth: Kirigami.Units.gridUnit * 14
            model: [i18n("** Menu font **")].concat(Qt.fontFamilies())
            currentIndex: page.cfg_headerFontFamily === "" ? 0 : Math.max(0, model.indexOf(page.cfg_headerFontFamily))
            onActivated: i => page.cfg_headerFontFamily = i === 0 ? "" : model[i]
        }
        QQC2.SpinBox {
            id: headerFontSize
            Kirigami.FormData.label: i18n("Header font size:")
            from: 0
            to: 32
            textFromValue: v => v === 0 ? i18n("Menu size") : v + " px"
            valueFromText: t => parseInt(t) || 0
        }
        QQC2.SpinBox {
            id: itemFontSize
            Kirigami.FormData.label: i18n("Item font size:")
            from: 0
            to: 24
            textFromValue: v => v === 0 ? i18n("Menu size") : v + " px"
            valueFromText: t => parseInt(t) || 0
        }

        Kirigami.Separator { Kirigami.FormData.isSection: true; Kirigami.FormData.label: i18n("Effects") }
        QQC2.CheckBox { id: animateMenu; text: i18n("Fade the menu in when it opens") }
        QQC2.CheckBox { id: greyIcons; text: i18n("Show app icons in grey") }
    }
}

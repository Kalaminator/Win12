// T39 "Colours" page like Windows 11 Settings > Personalisation > Colours (docs/research/win11-colours-reference.jpg).
// Every menu style but XP (XP keeps its scheme colours, Start Menu page). colorMode/accent keys shared with the Look page.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtQuick.Dialogs as Dialogs
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM

KCM.SimpleKCM {
    id: page

    property string cfg_menuStyle   // read only
    property int cfg_colorMode
    property int cfg_accentMode
    property color cfg_accentColor
    property var cfg_recentAccents: []
    property bool cfg_win10AccentStart
    property bool cfg_win10AccentTitles
    property bool cfg_win10Transparency

    // The standard Windows 10/11 accent palette, 8 per row like Windows.
    readonly property var windowsColours: ["#ffb900", "#ff8c00", "#f7630c", "#ca5010", "#da3b01", "#ef6950", "#d13438", "#ff4343",
        "#e74856", "#e81123", "#ea005e", "#c30052", "#e3008c", "#bf0077", "#c239b3", "#9a0089",
        "#0078d7", "#0063b1", "#8e8cd8", "#6b69d6", "#8764b8", "#744da9", "#b146c2", "#881798",
        "#0099bc", "#2d7d9a", "#00b7c3", "#038387", "#00b294", "#018574", "#00cc6a", "#10893e",
        "#7a7574", "#5d5a58", "#68768a", "#515c6b", "#567c73", "#486860", "#498205", "#107c10",
        "#767676", "#4c4a48", "#69797e", "#4a5459", "#647c64", "#525e54", "#847545", "#7e735f"]
    readonly property string current: cfg_accentMode === 1 ? cfg_accentColor.toString() : ""

    // Picking a colour = Manual accent, and it goes first in "Recent colours" (5 kept).
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

    Kirigami.FormLayout {
        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: page.cfg_menuStyle === "xp"
            text: i18n("The Windows XP style uses its own colour scheme (Start Menu page). These settings are for the other styles.")
        }
        QQC2.ComboBox {
            Kirigami.FormData.label: i18n("Choose your mode:")
            readonly property var modes: [2, 1, 0]   // Light, Dark, Automatic -> colorMode (3 = custom, Look page)
            model: [i18n("Light"), i18n("Dark"), i18n("Automatic (Plasma colours)")]
            currentIndex: modes.indexOf(page.cfg_colorMode)
            displayText: currentIndex < 0 ? i18n("Custom (Look page)") : currentText
            onActivated: index => page.cfg_colorMode = modes[index]
        }
        QQC2.ComboBox {
            Kirigami.FormData.label: i18n("Accent colour:")
            model: [i18n("Automatic (Plasma accent colour)"), i18n("Manual")]
            currentIndex: page.cfg_accentMode
            onActivated: index => page.cfg_accentMode = index
        }
        Flow {
            Kirigami.FormData.label: i18n("Recent colours:")
            visible: page.cfg_recentAccents.length > 0
            spacing: Kirigami.Units.smallSpacing
            Repeater {
                model: page.cfg_recentAccents
                Swatch { required property var modelData; hex: modelData }
            }
        }
        GridLayout {
            Kirigami.FormData.label: i18n("Windows colours:")
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
            text: i18n("View colours")
            icon.name: "color-picker"
            onClicked: colourDialog.open()
        }
        QQC2.Switch {
            text: i18n("Show accent colour on Start and taskbar")
            checked: page.cfg_win10AccentStart
            onToggled: page.cfg_win10AccentStart = checked
        }
        QQC2.Switch {
            text: i18n("Show accent colour on title bars and window borders")
            checked: page.cfg_win10AccentTitles
            onToggled: page.cfg_win10AccentTitles = checked
        }
        QQC2.Switch {
            text: i18n("Transparency effects")
            checked: page.cfg_win10Transparency
            onToggled: page.cfg_win10Transparency = checked
        }
        QQC2.Label {
            Layout.maximumWidth: Kirigami.Units.gridUnit * 24
            wrapMode: Text.WordWrap
            font: Kirigami.Theme.smallFont
            opacity: 0.7
            text: i18n("The taskbar and title bars follow these when their look is not \"Plasma's own\" (Style page).")
        }
    }

    Dialogs.ColorDialog {
        id: colourDialog
        selectedColor: page.cfg_accentColor
        onAccepted: page.pick(selectedColor.toString())
    }
}

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtQuick.Dialogs as Dialogs
import org.kde.kirigami as Kirigami
import "../code/tiles.js" as Tiles
import org.kde.kcmutils as KCM
import org.kde.kquickcontrols as KQuickControls
import org.kde.plasma.plasma5support as P5Support

// Taskbar page: Windows 12 layout and islands, the generated taskbar colour and texture, auto-hide, the
// taskbar search box, the Plasma panel (proposal 3.5).
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
        taskbarTheme: page.cfg_taskbarTheme
    }
    property string cfg_taskbarTheme
    property alias cfg_taskbarColor: tbColor.color
    property alias cfg_taskbarAlpha: tbAlpha.value
    property alias cfg_taskbarRadius: tbRadius.value
    property alias cfg_taskbarLine: tbLine.checked
    property string cfg_taskbarImage
    property string cfg_taskbarImageMode
    property alias cfg_taskbarImageAlpha: tbImageAlpha.value
    property alias cfg_taskbarImageTint: tbImageTint.checked
    property string cfg_taskbarGradient
    property alias cfg_taskbarGradientStrength: tbGradStrength.value
    property string cfg_taskbarButtons
    property alias cfg_taskbarFocusedSolid: tbSolid.checked
    property alias cfg_taskbarHiding: tbHiding.currentIndex
    property alias cfg_taskbarFloating: w12Float.checked
    property alias cfg_taskbarPopAnim: w12Pop.currentIndex
    property alias cfg_taskbarWindows: w12Windows.currentIndex
    property alias cfg_taskbarRound: w12Round.value
    property string cfg_taskbarLayout
    property string cfg_islandScreen
    property bool cfg_islandAllApps
    property int cfg_taskbarGap
    property int cfg_menuGap
    property bool cfg_taskbarMagnetic
    property alias cfg_taskbarSearch: searchBox.checked
    property alias cfg_taskbarSearchWidth: searchWidth.value

    readonly property bool w12: kinds.taskbarKind === "windows12"
    // W15: gap mode = docked, always-visible single bar (no auto-hide, no pop-up animation); same test as main.qml
    readonly property bool w12GapMode: w12 && w12Float.checked && w12Windows.currentIndex === 1 && cfg_taskbarLayout === "single"
    // Colour .. Task buttons: only the Override colour is the user's own; the Windows looks bring their preset.
    readonly property bool customColour: customBox.checked && !cfg_taskbarFollowsStyle

    P5Support.DataSource {
        id: executable
        engine: "executable"
        connectedSources: []
        onNewData: sourceName => disconnectSource(sourceName)
    }
    function run(cmd) { executable.connectSource(cmd); }

    Dialogs.FileDialog {
        id: tbImageFile
        title: i18n("Pick taskbar texture…")
        nameFilters: [i18n("Images (*.png *.jpg *.jpeg)")]
        onAccepted: page.cfg_taskbarImage = decodeURIComponent(selectedFile.toString().replace("file://", ""))
    }

    Kirigami.FormLayout {
        QQC2.Label {
            visible: kinds.taskbarKind === ""
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: i18n("Your own Plasma Style is in use. Pick a Windows taskbar on the Style page to change its layout and colours.")
            enabled: false
        }

        // W4: Windows 12 taskbar (floating bar, islands, gaps, corners)
        Kirigami.Separator { Kirigami.FormData.isSection: true; Kirigami.FormData.label: i18n("Windows 12 taskbar"); visible: page.w12 }
        TaskbarLayoutBox { visible: page.w12; cfgPage: page; Kirigami.FormData.label: i18n("Layout:") }
        // spec 10: islands on one monitor (by connector name; "" follows the main one) or on all of them
        QQC2.ComboBox {
            id: screenBox
            visible: page.w12
            enabled: page.cfg_taskbarLayout !== "single"
            Kirigami.FormData.label: i18n("Show islands on:")
            readonly property var names: Array.prototype.map.call(Qt.application.screens, s => s.name)
            readonly property var values: [""].concat(names.slice(1), ["all"])   // index 0 = the main monitor, saved as ""
            model: [i18n("%1 (main)", names[0] || "")].concat(names.slice(1), [i18n("All monitors")])
            currentIndex: Math.max(0, values.indexOf(page.cfg_islandScreen === names[0] ? "" : page.cfg_islandScreen))
            onActivated: index => page.cfg_islandScreen = values[index]
        }
        QQC2.ComboBox {
            visible: page.w12
            enabled: screenBox.enabled && page.cfg_islandScreen === "all"
            Kirigami.FormData.label: i18n("Open apps on other monitors:")
            model: [i18n("Only apps on that monitor"), i18n("All apps on every monitor")]
            currentIndex: page.cfg_islandAllApps ? 1 : 0
            onActivated: index => page.cfg_islandAllApps = index === 1
        }
        QQC2.Label {
            visible: page.w12
            Layout.maximumWidth: Kirigami.Units.gridUnit * 24
            wrapMode: Text.WordWrap
            enabled: false
            text: i18n("All monitors: every monitor gets Start and the app islands; the tray island stays on the main monitor. Moving an app icon to another island moves it on every monitor. Pin apps from the main monitor.")
        }
        QQC2.CheckBox { id: w12Float; visible: page.w12; text: i18n("Floating bar") }
        // W12 gaps (px): floating bar/islands to the screen edge, Start menu to the taskbar
        RowLayout {
            visible: page.w12
            enabled: w12Float.checked
            Kirigami.FormData.label: i18n("Bottom spacing:")
            QQC2.Label { text: i18n("None") }
            QQC2.Slider { from: 0; to: 24; stepSize: 1; value: page.cfg_taskbarGap; onMoved: page.cfg_taskbarGap = value }
            QQC2.Label { text: i18n("%1 px", page.cfg_taskbarGap) }
        }
        // W15 fill (auto-hiding island) | keep a gap above an always-visible island
        QQC2.ComboBox {
            id: w12Windows
            visible: page.w12
            enabled: w12Float.checked
            Kirigami.FormData.label: i18n("Maximised windows:")
            model: [i18n("Fill the screen"), i18n("Keep a gap above the taskbar")]
        }
        QQC2.Label {
            visible: page.w12
            Layout.maximumWidth: Kirigami.Units.gridUnit * 24
            wrapMode: Text.Wrap
            enabled: false
            text: i18n("Fill the screen: maximized windows use the whole screen and the bar auto-hides as an island over them. Keep a gap: the bar always shows and maximized windows stop one bottom spacing above it (single bar only; split islands always fill the screen).")
        }
        RowLayout {
            visible: page.w12
            Kirigami.FormData.label: i18n("Menu gap:")
            QQC2.Label { text: i18n("None") }
            QQC2.Slider { from: 0; to: 24; stepSize: 1; value: page.cfg_menuGap; onMoved: page.cfg_menuGap = value }
            QQC2.Label { text: i18n("%1 px", page.cfg_menuGap) }
        }
        // W7: floating = auto-hidden over full-screen windows (Auto-hide Off); pop-up animation = KWin effect
        QQC2.ComboBox {
            id: w12Pop
            visible: page.w12 && !page.w12GapMode
            enabled: w12Float.checked
            Kirigami.FormData.label: i18n("Pop-up animation:")
            model: [i18n("Slide (Plasma)"), i18n("Fade"), i18n("Zoom"), i18n("Rise")]
        }
        RowLayout {
            visible: page.w12
            Kirigami.FormData.label: i18n("Corner roundness:")
            QQC2.Label { text: i18n("Square") }
            QQC2.Slider { id: w12Round; from: 0; to: 100; stepSize: 1 }
            QQC2.Label { text: i18n("Round") }
        }

        // ponytail: no visible rule; taskbarGenerated || !taskbarFollowsStyle is always true (proposal 3.5 rule).
        Kirigami.Separator { Kirigami.FormData.isSection: true; Kirigami.FormData.label: i18n("Taskbar colour and texture") }
        QQC2.CheckBox {
            id: customBox
            visible: !page.cfg_taskbarFollowsStyle
            text: i18n("Use a custom taskbar colour instead of your Plasma Style")
            checked: page.cfg_taskbarTheme === "custom"
            onToggled: page.cfg_taskbarTheme = checked ? "custom" : "off"
        }
        QQC2.Label { visible: !page.customColour; text: i18n("Available with a custom taskbar colour."); enabled: false }
        KQuickControls.ColorButton { id: tbColor; enabled: page.customColour; Kirigami.FormData.label: i18n("Colour:"); showAlphaChannel: false }
        RowLayout {
            enabled: page.customColour
            Kirigami.FormData.label: i18n("Opacity:")
            QQC2.Slider { id: tbAlpha; from: 0; to: 100; stepSize: 1 }
            QQC2.Label { text: i18n("%1% opaque", Math.round(tbAlpha.value)) }
        }
        QQC2.SpinBox {
            id: tbRadius
            enabled: page.customColour
            Kirigami.FormData.label: i18n("Corner radius:")
            from: 2
            to: 24
            textFromValue: v => v <= 2 ? i18n("Square") : v + " px"
            valueFromText: t => parseInt(t) || 2
        }
        QQC2.CheckBox { id: tbLine; enabled: page.customColour; text: i18n("Light line on the taskbar edge") }
        // T34: task button look in the generated tasks.svg (presets pick their own; "Plasma" keeps the style's)
        QQC2.ComboBox {
            enabled: page.customColour
            Kirigami.FormData.label: i18n("Task buttons:")
            textRole: "text"
            valueRole: "value"
            model: [{ value: "plasma", text: i18n("Plasma Style") }, { value: "win10", text: i18n("Windows 10 (bar under active)") },
                    { value: "win11", text: i18n("Windows 11 (pill under active)") }, { value: "win7", text: i18n("Windows 7 (glass)") }]
            Component.onCompleted: currentIndex = Math.max(0, indexOfValue(page.cfg_taskbarButtons))
            onActivated: page.cfg_taskbarButtons = currentValue
        }
        // W14: gradient in the generated panel background (every Windows-like look and the custom colour)
        RowLayout {
            enabled: kinds.taskbarGenerated
            Kirigami.FormData.label: i18n("Gradient:")
            QQC2.ComboBox {
                textRole: "text"
                valueRole: "value"
                model: [{ value: "none", text: i18n("None") }, { value: "left-right", text: i18n("Left to right") },
                        { value: "right-left", text: i18n("Right to left") }, { value: "top-bottom", text: i18n("Top to bottom") },
                        { value: "bottom-top", text: i18n("Bottom to top") }]
                Component.onCompleted: currentIndex = Math.max(0, indexOfValue(page.cfg_taskbarGradient))
                onActivated: page.cfg_taskbarGradient = currentValue
            }
            QQC2.Slider { id: tbGradStrength; enabled: page.cfg_taskbarGradient !== "none"; from: 0; to: 100; stepSize: 5 }
            QQC2.Label { text: i18n("Gradient strength: %1%", Math.round(tbGradStrength.value)) }
        }
        // T33: texture drawn inside the generated panel background (xptheme.py image=...); W14 also for the
        // Windows-like looks (Windows 12 bar and islands included)
        RowLayout {
            enabled: kinds.taskbarGenerated
            Kirigami.FormData.label: i18n("Texture:")
            QQC2.Label { text: page.cfg_taskbarImage ? page.cfg_taskbarImage.split("/").pop() : i18n("None") }
            QQC2.Button { text: i18n("Pick image…"); icon.name: "document-open"; onClicked: tbImageFile.open() }
            QQC2.Button { icon.name: "edit-clear"; visible: page.cfg_taskbarImage !== ""; onClicked: page.cfg_taskbarImage = "" }
        }
        QQC2.ComboBox {
            enabled: kinds.taskbarGenerated && page.cfg_taskbarImage !== ""
            Kirigami.FormData.label: i18n("Placement:")
            textRole: "text"
            valueRole: "value"
            model: [{ value: "tile", text: i18n("Tile") }, { value: "stretch", text: i18n("Stretch") },
                    { value: "top", text: i18n("Align top") }, { value: "middle", text: i18n("Align middle") },
                    { value: "bottom", text: i18n("Align bottom") }]
            Component.onCompleted: currentIndex = Math.max(0, indexOfValue(page.cfg_taskbarImageMode))
            onActivated: page.cfg_taskbarImageMode = currentValue
        }
        RowLayout {
            enabled: kinds.taskbarGenerated && page.cfg_taskbarImage !== ""
            Kirigami.FormData.label: i18n("Texture opacity:")
            QQC2.Slider { id: tbImageAlpha; from: 0; to: 100; stepSize: 1 }
            QQC2.Label { text: i18n("%1% opaque", Math.round(tbImageAlpha.value)) }
        }
        QQC2.CheckBox {
            id: tbImageTint
            enabled: kinds.taskbarGenerated && page.cfg_taskbarImage !== ""
            text: i18n("Tint with the accent colour")
        }
        QQC2.CheckBox {
            id: tbSolid
            enabled: kinds.taskbarGenerated
            text: i18n("Paint the active button less transparent")
        }

        Kirigami.Separator { Kirigami.FormData.isSection: true; Kirigami.FormData.label: i18n("Behaviour") }
        // native panel visibility (panel.hiding); Off = docked. KDE Plasma look / own Plasma Style: the user's
        // own panel, not touched (W12), so the row hides and the Plasma panel group below covers it.
        QQC2.ComboBox {
            id: tbHiding
            visible: kinds.taskbarKind !== "" && kinds.taskbarKind !== "plasma" && !page.w12GapMode
            Kirigami.FormData.label: i18n("Auto-hide:")
            model: [i18n("Off"), i18n("Hide when a window covers it"), i18n("Always auto-hide")]
            // W7: the floating Windows 12 bar auto-hides even on Off (Plasma docks a floating bar under maximized windows)
            readonly property bool w12Hides: page.w12 && w12Float.checked
            displayText: currentIndex === 0 && w12Hides ? i18n("Off (the floating bar still auto-hides)") : currentText
        }
        QQC2.Label {
            visible: tbHiding.visible
            Layout.maximumWidth: Kirigami.Units.gridUnit * 24
            wrapMode: Text.Wrap
            text: tbHiding.w12Hides && tbHiding.currentIndex === 0
                ? i18n("The floating Windows 12 bar always auto-hides, so it stays an island over maximized windows. Turn off \"Floating bar\" for a fixed one.")
                : i18n("A hidden taskbar shows again when the mouse touches the screen edge.")
            enabled: false
        }
        QQC2.CheckBox { id: searchBox; text: i18n("Show a search box on the taskbar") }
        QQC2.SpinBox {
            id: searchWidth
            enabled: searchBox.checked
            Kirigami.FormData.label: i18n("Search box width:")
            from: 100
            to: 600
            stepSize: 10
            textFromValue: v => v + " px"
            valueFromText: t => parseInt(t) || 220
        }

        Kirigami.Separator { Kirigami.FormData.isSection: true; Kirigami.FormData.label: i18n("Plasma panel") }
        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: i18n("Panel height, position and task grouping are set by Plasma itself.")
            enabled: false
        }
        RowLayout {
            QQC2.Button {
                icon.name: "configure-toolbars"
                text: i18n("Edit panel…")
                // Plasma 6.7 has no "edit mode" shortcut; the shell's editMode D-Bus property works.
                // (Task manager settings can't be opened by script: right-click the taskbar for them.)
                onClicked: page.run("" + Tiles.QDBUS + " org.kde.plasmashell /PlasmaShell org.freedesktop.DBus.Properties.Set org.kde.PlasmaShell editMode true")
            }
            QQC2.Label { text: i18n("(applies at once)"); enabled: false }
        }
    }
}

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtQuick.Dialogs as Dialogs
import org.kde.kirigami as Kirigami
import "../code/tiles.js" as Tiles
import org.kde.kcmutils as KCM
import org.kde.kquickcontrols as KQuickControls
import org.kde.plasma.plasma5support as P5Support

// Taskbar page: the generated Plasma Style keys (T29-T35) plus shortcuts to what stock Plasma covers.
KCM.SimpleKCM {
    id: page
    property string cfg_menuStyle   // read only, to show the XP option
    property string cfg_taskbarLook   // read only (T43, picked on the Style page)
    property alias cfg_xpFirefoxTitlebar: ffBox.checked
    property bool cfg_windowTheme   // read only (T43: the Style page "Windows:" combo)
    property alias cfg_windowOpacity: winAlpha.value
    property alias cfg_windowBlur: winBlur.checked
    property int cfg_windowRadius
    property int cfg_titleHeight
    property string cfg_windowImage
    property string cfg_windowImageMode
    property alias cfg_windowImageAlpha: winImageAlpha.value
    property alias cfg_windowImageTint: winImageTint.checked
    property bool cfg_taskbarFollowsStyle
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
    // W15: gap mode = docked, always-visible single bar (no auto-hide, no pop-up animation)
    readonly property bool w12GapMode: (cfg_taskbarLook || cfg_menuStyle) === "windows12" && w12Float.checked
        && w12Windows.currentIndex === 1 && cfg_taskbarLayout === "single"
    property alias cfg_taskbarRound: w12Round.value
    property string cfg_taskbarLayout
    property string cfg_islandScreen
    property bool cfg_islandAllApps
    property int cfg_win12Light
    property int cfg_win12Dark
    property int cfg_win12Transparency
    property int cfg_win12Blur
    property int cfg_win12Noise
    property int cfg_taskbarGap
    property int cfg_menuGap
    property bool cfg_taskbarMagnetic
    property alias cfg_taskbarSearch: searchBox.checked
    property alias cfg_taskbarSearchWidth: searchWidth.value

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
    Dialogs.FileDialog {
        id: winImageFile
        title: i18n("Pick title bar picture…")
        nameFilters: [i18n("Images (*.png *.jpg *.jpeg)")]
        onAccepted: page.cfg_windowImage = decodeURIComponent(selectedFile.toString().replace("file://", ""))
    }

    Kirigami.FormLayout {
        // T43: which taskbar and window look is used moved to the Style page ("Taskbar:" / "Windows:").
        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: i18n("The taskbar and window looks are picked on the Style page.")
            enabled: false
        }
        QQC2.CheckBox {
            id: ffBox
            visible: page.cfg_menuStyle === "xp"
            text: i18n("Use the system title bar in Firefox with XP themes")
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.text: i18n("Firefox draws its own title bar, so the XP one does not show. Restart Firefox for this to take effect.")
        }
        // T41: glass options for the windowTheme decoration (main.qml glassSuffix), every style.
        ColumnLayout {
            enabled: page.cfg_windowTheme
            Kirigami.FormData.label: i18n("Window frames:")
            RowLayout {
                QQC2.Slider { id: winAlpha; from: 30; to: 100; stepSize: 5 }
                QQC2.Label { text: i18n("%1% opaque", Math.round(winAlpha.value)) }
            }
            QQC2.CheckBox { id: winBlur; enabled: winAlpha.value < 100; text: i18n("Blur behind translucent frames") }
            RowLayout {
                QQC2.CheckBox {
                    id: ownRadius
                    text: i18n("Corner radius:")
                    checked: page.cfg_windowRadius >= 0
                    onToggled: page.cfg_windowRadius = checked ? radiusSpin.value : -1
                }
                QQC2.SpinBox {
                    id: radiusSpin
                    enabled: ownRadius.checked
                    from: 0
                    to: 12
                    value: Math.max(0, page.cfg_windowRadius < 0 ? 8 : page.cfg_windowRadius)
                    textFromValue: v => v === 0 ? i18n("Square") : v + " px"
                    valueFromText: t => parseInt(t) || 0
                    onValueModified: page.cfg_windowRadius = value
                }
            }
            RowLayout {
                QQC2.CheckBox {
                    id: ownTitle
                    text: i18n("Title bar height:")
                    checked: page.cfg_titleHeight > 0
                    onToggled: page.cfg_titleHeight = checked ? titleSpin.value : 0
                }
                QQC2.SpinBox {
                    id: titleSpin
                    enabled: ownTitle.checked
                    from: 20
                    to: 48
                    value: page.cfg_titleHeight > 0 ? page.cfg_titleHeight : 30
                    textFromValue: v => v + " px"
                    valueFromText: t => parseInt(t) || 30
                    onValueModified: page.cfg_titleHeight = value
                }
            }
            // W14: picture over the title bar (xptheme.py --deco --image), same opacity/blur as the frame
            RowLayout {
                QQC2.Label { text: i18n("Title bar picture:") }
                QQC2.Label { text: page.cfg_windowImage ? page.cfg_windowImage.split("/").pop() : i18n("None") }
                QQC2.Button { text: i18n("Pick image…"); icon.name: "document-open"; onClicked: winImageFile.open() }
                QQC2.Button { icon.name: "edit-clear"; visible: page.cfg_windowImage !== ""; onClicked: page.cfg_windowImage = "" }
            }
            RowLayout {
                enabled: page.cfg_windowImage !== ""
                QQC2.ComboBox {
                    textRole: "text"
                    valueRole: "value"
                    model: [{ value: "stretch", text: i18n("Stretch") }, { value: "tile", text: i18n("Tile") }]
                    Component.onCompleted: currentIndex = Math.max(0, indexOfValue(page.cfg_windowImageMode))
                    onActivated: page.cfg_windowImageMode = currentValue
                }
                QQC2.Slider { id: winImageAlpha; from: 0; to: 100; stepSize: 1 }
                QQC2.Label { text: i18n("%1% opaque", Math.round(winImageAlpha.value)) }
                QQC2.CheckBox { id: winImageTint; text: i18n("Tint with the accent colour") }
            }
        }
        // W4: Windows 12 taskbar group (floating bar, corners, colour, layout)
        ColumnLayout {
            visible: (page.cfg_taskbarLook || page.cfg_menuStyle) === "windows12"
            Kirigami.FormData.label: i18n("Windows 12 taskbar:")
            QQC2.CheckBox { id: w12Float; text: i18n("Floating bar") }
            // W7: floating = auto-hidden over full-screen windows (Auto-hide Off); pop-up animation = KWin effect
            RowLayout {
                enabled: w12Float.checked
                visible: !page.w12GapMode
                QQC2.Label { text: i18n("Taskbar pop-up animation:") }
                QQC2.ComboBox { id: w12Pop; model: [i18n("Slide (Plasma)"), i18n("Fade"), i18n("Zoom"), i18n("Rise")] }
            }
            RowLayout {
                QQC2.Label { text: i18n("Square") }
                QQC2.Slider { id: w12Round; from: 0; to: 100; stepSize: 1 }
                QQC2.Label { text: i18n("Round") }
            }
            RowLayout {
                enabled: customBox.checked && !page.cfg_taskbarFollowsStyle
                QQC2.Label { text: i18n("Bar colour:") }
                KQuickControls.ColorButton {
                    showAlphaChannel: false
                    color: page.cfg_taskbarColor
                    onColorChanged: if (color != page.cfg_taskbarColor) page.cfg_taskbarColor = color
                }
                QQC2.Slider {
                    from: 0; to: 100; stepSize: 1
                    value: page.cfg_taskbarAlpha
                    onMoved: page.cfg_taskbarAlpha = value
                }
                QQC2.Label { text: i18n("%1% opaque", page.cfg_taskbarAlpha) }
            }
            QQC2.Label {
                text: i18n("Colour and opacity follow \"Override the taskbar colour\" below.")
                enabled: false
            }
            // W11 colour sliders (-1 = default look; the slider shows where that look sits)
            GridLayout {
                columns: 4
                QQC2.Label { text: i18n("How light:") }
                QQC2.Label { text: i18n("Soft tint") }
                QQC2.Slider { from: 0; to: 100; value: page.cfg_win12Light < 0 ? 80 : page.cfg_win12Light; onMoved: page.cfg_win12Light = Math.round(value) }
                QQC2.Label { text: i18n("Near white") }
                QQC2.Label { text: i18n("How dark:") }
                QQC2.Label { text: i18n("Soft dark") }
                QQC2.Slider { from: 0; to: 100; value: page.cfg_win12Dark < 0 ? 50 : page.cfg_win12Dark; onMoved: page.cfg_win12Dark = Math.round(value) }
                QQC2.Label { text: i18n("Near black") }
                QQC2.Label { text: i18n("Transparency:") }
                QQC2.Label { text: i18n("Solid") }
                QQC2.Slider { from: 0; to: 90; value: page.cfg_win12Transparency < 0 ? 70 : page.cfg_win12Transparency; onMoved: page.cfg_win12Transparency = Math.round(value) }
                QQC2.Label { text: i18n("Clear") }
                QQC2.Label { text: i18n("Blur strength:") }
                QQC2.Label { text: i18n("Soft") }
                QQC2.Slider { from: 1; to: 15; stepSize: 1; value: page.cfg_win12Blur > 0 ? page.cfg_win12Blur : 3; onMoved: page.cfg_win12Blur = value }
                QQC2.Label { text: i18n("Strong") }
                QQC2.Label { text: i18n("Grain:") }
                QQC2.Label { text: i18n("Smooth") }
                QQC2.Slider { from: 0; to: 14; stepSize: 1; value: page.cfg_win12Noise; onMoved: page.cfg_win12Noise = value }
                QQC2.Label { text: i18n("Grainy") }
                // W12 gaps (px): floating bar/islands to the screen edge, Start menu to the taskbar
                QQC2.Label { text: i18n("Bottom spacing:") }
                QQC2.Label { text: i18n("None") }
                QQC2.Slider { enabled: w12Float.checked; from: 0; to: 24; stepSize: 1; value: page.cfg_taskbarGap; onMoved: page.cfg_taskbarGap = value }
                QQC2.Label { text: i18n("%1 px", page.cfg_taskbarGap) }
                // W15 fill (auto-hiding island) | keep a gap above an always-visible island
                QQC2.Label { text: i18n("Windows:") }
                QQC2.ComboBox {
                    id: w12Windows
                    enabled: w12Float.checked
                    Layout.columnSpan: 3
                    model: [i18n("Fill the screen"), i18n("Keep a gap above the taskbar")]
                }
                QQC2.Label {
                    Layout.columnSpan: 4
                    Layout.maximumWidth: Kirigami.Units.gridUnit * 24
                    wrapMode: Text.Wrap
                    enabled: false
                    text: i18n("Fill the screen: maximized windows use the whole screen and the bar auto-hides as an island over them. Keep a gap: the bar always shows and maximized windows stop one bottom spacing above it (single bar only; split islands always fill the screen).")
                }
                QQC2.Label { text: i18n("Menu gap:") }
                QQC2.Label { text: i18n("None") }
                QQC2.Slider { from: 0; to: 24; stepSize: 1; value: page.cfg_menuGap; onMoved: page.cfg_menuGap = value }
                QQC2.Label { text: i18n("%1 px", page.cfg_menuGap) }
            }
            QQC2.Label {
                Layout.maximumWidth: Kirigami.Units.gridUnit * 24
                wrapMode: Text.WordWrap
                enabled: false
                text: i18n("Taskbar, Start menu and title bars; transparency changes only the background, icons and text stay solid. Blur strength sets KWin's blur for all windows while the Windows 12 look is on; your own value comes back when you leave it.")
            }
            QQC2.Label { text: i18n("Taskbar layout:") }
            TaskbarLayoutBox { cfgPage: page }
            // spec 10: islands on one monitor (by connector name; "" follows the main one) or on all of them
            QQC2.Label { text: i18n("Show islands on:"); visible: screenBox.visible }
            QQC2.ComboBox {
                id: screenBox
                visible: page.cfg_taskbarLayout !== "single"
                readonly property var names: Array.prototype.map.call(Qt.application.screens, s => s.name)
                readonly property var values: [""].concat(names.slice(1), ["all"])   // index 0 = the main monitor, saved as ""
                model: [i18n("%1 (main)", names[0] || "")].concat(names.slice(1), [i18n("All monitors")])
                currentIndex: Math.max(0, values.indexOf(page.cfg_islandScreen === names[0] ? "" : page.cfg_islandScreen))
                onActivated: index => page.cfg_islandScreen = values[index]
            }
            QQC2.Label { text: i18n("Open apps on other monitors:"); visible: appsBox.visible }
            QQC2.ComboBox {
                id: appsBox
                visible: screenBox.visible && page.cfg_islandScreen === "all"
                model: [i18n("Only apps on that monitor"), i18n("All apps on every monitor")]
                currentIndex: page.cfg_islandAllApps ? 1 : 0
                onActivated: index => page.cfg_islandAllApps = index === 1
            }
            QQC2.Label {
                visible: screenBox.visible
                Layout.maximumWidth: Kirigami.Units.gridUnit * 24
                wrapMode: Text.WordWrap
                enabled: false
                text: i18n("All monitors: every monitor gets Start and the app islands; the tray island stays on the main monitor. Moving an app icon to another island moves it on every monitor. Pin apps from the main monitor.")
            }
        }
        Item { Kirigami.FormData.isSection: true }
        QQC2.CheckBox {
            id: customBox
            enabled: !page.cfg_taskbarFollowsStyle
            text: i18n("Override the taskbar colour")
            checked: page.cfg_taskbarTheme === "custom"
            onToggled: page.cfg_taskbarTheme = checked ? "custom" : "off"
        }
        ColumnLayout {
            enabled: customBox.checked && !page.cfg_taskbarFollowsStyle
            Kirigami.FormData.label: i18n("Custom taskbar:")
            KQuickControls.ColorButton { id: tbColor; showAlphaChannel: false }
            RowLayout {
                QQC2.Slider { id: tbAlpha; from: 0; to: 100; stepSize: 1 }
                QQC2.Label { text: i18n("%1% opaque", Math.round(tbAlpha.value)) }
            }
            QQC2.SpinBox {
                id: tbRadius
                from: 2
                to: 24
                textFromValue: v => v <= 2 ? i18n("Square") : v + " px"
                valueFromText: t => parseInt(t) || 2
            }
            QQC2.CheckBox { id: tbLine; text: i18n("Light line on the taskbar edge") }
        }
        // W14: gradient in the generated panel background (every Windows-like look and the custom colour)
        RowLayout {
            enabled: page.cfg_taskbarFollowsStyle || customBox.checked
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
        ColumnLayout {
            enabled: page.cfg_taskbarFollowsStyle || customBox.checked
            Kirigami.FormData.label: i18n("Texture:")
            RowLayout {
                QQC2.Label { text: page.cfg_taskbarImage ? page.cfg_taskbarImage.split("/").pop() : i18n("None") }
                QQC2.Button { text: i18n("Pick image…"); icon.name: "document-open"; onClicked: tbImageFile.open() }
                QQC2.Button { icon.name: "edit-clear"; visible: page.cfg_taskbarImage !== ""; onClicked: page.cfg_taskbarImage = "" }
            }
            ColumnLayout {
                enabled: page.cfg_taskbarImage !== ""
                QQC2.ComboBox {
                    textRole: "text"
                    valueRole: "value"
                    model: [{ value: "tile", text: i18n("Tile") }, { value: "stretch", text: i18n("Stretch") },
                            { value: "top", text: i18n("Align top") }, { value: "middle", text: i18n("Align middle") },
                            { value: "bottom", text: i18n("Align bottom") }]
                    Component.onCompleted: currentIndex = Math.max(0, indexOfValue(page.cfg_taskbarImageMode))
                    onActivated: page.cfg_taskbarImageMode = currentValue
                }
                RowLayout {
                    QQC2.Slider { id: tbImageAlpha; from: 0; to: 100; stepSize: 1 }
                    QQC2.Label { text: i18n("%1% opaque", Math.round(tbImageAlpha.value)) }
                }
                QQC2.CheckBox { id: tbImageTint; text: i18n("Tint with the accent colour") }
            }
        }
        // T34: task button look in the generated tasks.svg (presets pick their own; "Plasma" keeps the style's)
        QQC2.ComboBox {
            enabled: customBox.checked && !page.cfg_taskbarFollowsStyle
            Kirigami.FormData.label: i18n("Task buttons:")
            textRole: "text"
            valueRole: "value"
            model: [{ value: "plasma", text: i18n("Plasma Style") }, { value: "win10", text: i18n("Windows 10 (bar under active)") },
                    { value: "win11", text: i18n("Windows 11 (pill under active)") }, { value: "win7", text: i18n("Windows 7 (glass)") }]
            Component.onCompleted: currentIndex = Math.max(0, indexOfValue(page.cfg_taskbarButtons))
            onActivated: page.cfg_taskbarButtons = currentValue
        }
        QQC2.CheckBox {
            id: tbSolid
            enabled: customBox.checked || page.cfg_taskbarFollowsStyle
            text: i18n("Paint the active button less transparent")
        }
        // native panel visibility (panel.hiding); Off = docked. KDE Plasma look: the user's own panel, not touched (W12)
        QQC2.ComboBox {
            id: tbHiding
            visible: !kdeLook && !page.w12GapMode
            readonly property bool kdeLook: (page.cfg_taskbarLook || page.cfg_menuStyle) === "plasma"
            Kirigami.FormData.label: i18n("Auto-hide:")
            model: [i18n("Off"), i18n("Hide when a window covers it"), i18n("Always auto-hide")]
            // W7: the floating Windows 12 bar auto-hides even on Off (Plasma docks a floating bar under maximized windows)
            readonly property bool w12Hides: (page.cfg_taskbarLook || page.cfg_menuStyle) === "windows12" && w12Float.checked
            displayText: currentIndex === 0 && w12Hides ? i18n("Off (the floating bar still auto-hides)") : currentText
        }
        QQC2.Label {
            visible: !page.w12GapMode
            Kirigami.FormData.label: tbHiding.kdeLook ? i18n("Auto-hide:") : ""
            text: tbHiding.kdeLook ? i18n("The KDE Plasma look keeps your own panel settings (Edit Mode > Panel Settings).")
                : tbHiding.w12Hides && tbHiding.currentIndex === 0
                ? i18n("The floating Windows 12 bar always auto-hides, so it stays an island over maximized windows. Turn off \"Floating bar\" for a fixed one.")
                : i18n("A hidden taskbar shows again when the mouse touches the screen edge.")
            enabled: false
        }

        Item { Kirigami.FormData.isSection: true }
        QQC2.CheckBox {
            id: searchBox
            Kirigami.FormData.label: i18n("Search:")
            text: i18n("Show search on taskbar")
        }
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

        Item { Kirigami.FormData.isSection: true }
        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: i18n("Panel height, position, floating, auto-hide and task grouping are handled by Plasma itself.")
        }
        QQC2.Button {
            Kirigami.FormData.label: i18n("Plasma settings:")
            icon.name: "configure-toolbars"
            text: i18n("Edit panel…")
            // Plasma 6.7 has no "edit mode" shortcut; the shell's editMode D-Bus property works.
            // (Task manager settings can't be opened by script: right-click the taskbar for them.)
            onClicked: page.run("" + Tiles.QDBUS + " org.kde.plasmashell /PlasmaShell org.freedesktop.DBus.Properties.Set org.kde.PlasmaShell editMode true")
        }
    }
}

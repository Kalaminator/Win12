import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtQuick.Effects
import QtQuick.Shapes
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.kirigami as Kirigami
import org.kde.plasma.extras as PlasmaExtras
import "common"

// The start button in the panel. Start11 button images hold 3 frames stacked
// top to bottom: normal, hover, pressed.
MouseArea {
    id: button

    readonly property var cfg: Plasmoid.configuration
    readonly property bool vertical: Plasmoid.formFactor === PlasmaCore.Types.Vertical
    readonly property int frame: pressed || root.menuOpen ? 2 : (onFace ? 1 : 0)
    readonly property string imageSource: cfg.buttonImage.startsWith("/")
        ? "file://" + cfg.buttonImage
        : Qt.resolvedUrl("../images/StartButtons/" + cfg.buttonImage)

    readonly property real side: vertical ? width : height
    // XP style in a horizontal panel: the green "start" pill replaces the normal button.
    readonly property bool xp: root.menuKind === "xp" && !vertical
    // KDE Plasma style: Kickoff's plain start-here-kde icon button, whatever the Start button page says.
    readonly property bool plasma: root.menuKind === "plasma"
    readonly property int mode: plasma ? 2 : cfg.buttonMode
    readonly property string iconSource: cfg.buttonIcon === "" || plasma ? "start-here-kde"
              : cfg.buttonIcon.startsWith("/") ? "file://" + cfg.buttonIcon : cfg.buttonIcon
    readonly property real buttonWidth: xp ? xpPill.implicitWidth : (mode === 0 && sheet.status === Image.Ready
        ? Math.round(side * sheet.sourceSize.width / (sheet.sourceSize.height / 3)) : side)
    // Start11 "Show search on taskbar" (T30): a field right of the button, horizontal panels only.
    readonly property bool showSearch: cfg.taskbarSearch && !vertical
    Layout.preferredWidth: vertical ? -1 : buttonWidth + (showSearch ? cfg.taskbarSearchWidth + 4 : 0)
    Layout.preferredHeight: vertical ? width : -1
    Layout.minimumWidth: Layout.preferredWidth
    Layout.minimumHeight: Layout.preferredHeight

    hoverEnabled: true
    readonly property bool onFace: containsMouse && (vertical || mouseX < buttonWidth)
    property bool wasExpanded
    acceptedButtons: Qt.LeftButton | Qt.RightButton
    onPressed: wasExpanded = root.menuOpen
    onClicked: mouse => {
        if (mouse.button === Qt.RightButton) openMenu(button, mouse.x, mouse.y);
        else if (vertical || mouse.x < buttonWidth) root.menuOpen = !wasExpanded;
    }

    // Right-click menu: Win12 hosts widgets (ContainmentItem), so Plasma no longer shows the applet's
    // contextualActions on its own; show them here, plus "Configure…".
    function openMenu(item, x, y) {
        buttonMenu.visualParent = item;
        buttonMenu.open(x, y);
    }
    PlasmaExtras.Menu {
        id: buttonMenu
        visualParent: button
        placement: PlasmaExtras.Menu.TopPosedLeftAlignedPopup
    }
    Instantiator {
        model: Array.from(Plasmoid.contextualActions).concat([Plasmoid.internalAction("configure")])
        delegate: PlasmaExtras.MenuItem {
            required property var modelData
            action: modelData
        }
        onObjectAdded: (index, object) => buttonMenu.addMenuItem(object)
        onObjectRemoved: (index, object) => buttonMenu.removeMenuItem(object)
    }

    // The button's own area; the search field sits to its right.
    Item {
        id: face
        width: button.vertical ? parent.width : button.buttonWidth
        height: parent.height
    }

    // Typing opens the menu on the search results (SearchView); the menu's own search box then
    // carries on (both show root.searchText). Esc or closing the menu empties it.
    Metrics { id: m }
    QQC2.TextField {
        id: field
        visible: button.showSearch
        x: button.buttonWidth + 4
        width: button.cfg.taskbarSearchWidth
        height: Math.min(parent.height - 4, m.px(32))
        anchors.verticalCenter: parent.verticalCenter
        text: root.searchText
        placeholderText: i18n("Type here to search")
        color: m.text
        placeholderTextColor: Qt.rgba(m.text.r, m.text.g, m.text.b, 0.6)
        font.family: m.font
        font.pixelSize: m.fontSize
        leftPadding: height
        verticalAlignment: TextInput.AlignVCenter
        background: Rectangle {
            color: Qt.rgba(m.bg.r, m.bg.g, m.bg.b, m.bgAlpha)
            border.width: 1
            border.color: field.activeFocus ? m.accent : Qt.rgba(m.text.r, m.text.g, m.text.b, 0.25)
            radius: 3
        }
        Kirigami.Icon {
            anchors.left: parent.left
            anchors.leftMargin: Math.round((parent.height - height) / 2)
            anchors.verticalCenter: parent.verticalCenter
            width: Math.round(parent.height * 0.5)
            height: width
            source: "search"
            color: m.text
            isMask: true
        }
        // A panel only takes keyboard focus while an applet asks for input.
        property int oldStatus: PlasmaCore.Types.ActiveStatus
        onPressed: {
            if (Plasmoid.status !== PlasmaCore.Types.AcceptingInputStatus) oldStatus = Plasmoid.status;
            Plasmoid.status = PlasmaCore.Types.AcceptingInputStatus;
            forceActiveFocus();
        }
        onActiveFocusChanged: if (!activeFocus) Plasmoid.status = oldStatus
        onTextEdited: {
            root.searchText = text;
            if (text !== "") root.menuOpen = true;
        }
        Keys.onReturnPressed: root.menuOpen = true
        Keys.onEnterPressed: root.menuOpen = true
        Keys.onEscapePressed: {
            root.searchText = "";
            root.menuOpen = false;
        }
    }

    // Plasma draws an "expanded" mark (widgets/tabbar active-tab) under the button while the menu is open;
    // XP's button has none (it peeked out at the pill's round corner as a yellow line). Win12 is a containment,
    // which libplasma always reports as expanded, so the mark is also hidden while the menu is closed.
    readonly property Item expandedMark: {
        for (let p = parent; p; p = p.parent)
            if (p.expandedFeedback !== undefined) return p.expandedFeedback;
        return null;
    }
    Binding {
        when: button.expandedMark !== null && (button.xp || !root.menuOpen)
        target: button.expandedMark
        property: "visible"
        value: false
    }

    // T42 Menu position Center/Right: the popup's visualParent (main.qml full.place() moves and sizes it).
    readonly property Item popupAnchor: popupAnchorItem
    Item { id: popupAnchorItem; }

    Accessible.name: Plasmoid.title
    Accessible.role: Accessible.Button

    // Mode 0: Start11 image sheet.
    Item {
        anchors.fill: face
        clip: true
        visible: !button.xp && button.mode === 0
        Image {
            id: sheet
            source: button.imageSource
            width: parent.width
            height: parent.height * 3
            y: -button.frame * parent.height
            fillMode: Image.Stretch
            smooth: true
            mipmap: true
        }
    }

    // Mode 1: four squares, top left one in the accent colour.
    Item {
        id: squares
        visible: !button.xp && button.mode === 1
        anchors.centerIn: face
        readonly property real size: Math.round(button.side * 0.42)
        width: size
        height: size
        scale: button.pressed ? 0.88 : 1
        Behavior on scale { NumberAnimation { duration: 90 } }
        Grid {
            columns: 2
            spacing: Math.max(1, Math.round(squares.size * 0.08))
            Repeater {
                model: 4
                Rectangle {
                    required property int index
                    width: (squares.size - parent.spacing) / 2
                    height: width
                    color: index === 0 ? button.cfg.squaresAccent : button.cfg.squaresColor
                }
            }
        }
    }
    Rectangle {
        anchors.fill: face
        visible: !button.xp && button.mode !== 0
        radius: 4
        color: Kirigami.Theme.textColor
        opacity: button.onFace || root.menuOpen ? 0.1 : 0
    }

    // Mode 2: any icon. Pressing squeezes it a little, like the Windows 11 Start button.
    Kirigami.Icon {
        visible: !button.xp && button.mode === 2
        anchors.fill: face
        anchors.margins: Math.round(button.side * (1 - button.cfg.buttonIconScale / 100) / 2)
        source: button.iconSource
        active: button.onFace
        scale: button.pressed && button.onFace ? 0.82 : 1
        Behavior on scale { NumberAnimation { duration: 110; easing.type: Easing.OutQuad } }
    }

    // XP: green pill (classic: square left, round right; modern: fully round, glow on hover).
    // Colour: xpStartCustom = xpStartColor; else xpStartGreen keeps it green like XP; else scheme colour.
    // It reaches the panel's left edge when the button is the panel's first item.
    Item {
        id: xpPill
        visible: button.xp
        anchors.fill: face
        anchors.leftMargin: -reach
        anchors.topMargin: -reachTop
        anchors.bottomMargin: -reachBottom
        // Gaps between the bar's edges and the button (panel padding): the pill grows into them, so it is
        // as tall as the bar; to the left only when the button is the first thing on the panel.
        property real reach: 0
        property real reachTop: 0
        property real reachBottom: 0
        function measure() {
            const p = button.mapToItem(null, 0, 0);
            const small = v => v > 0 && v < button.height / 2 ? v : 0;
            reach = p.x > 0 && p.x < button.height ? p.x : 0;
            // ponytail: the panel window has invisible room on its inner side (floating/shadow), so only
            // the gap on the screen-edge side is measured; the bar's padding is the same on the other side.
            const win = button.Window.window;
            const edge = Plasmoid.location === PlasmaCore.Types.TopEdge ? small(p.y)
                       : win ? small(win.height - p.y - button.height) : 0;
            reachTop = edge;
            reachBottom = edge;
        }
        Component.onCompleted: Qt.callLater(measure)
        Connections {
            target: button
            function onWidthChanged() { Qt.callLater(xpPill.measure) }
            function onHeightChanged() { Qt.callLater(xpPill.measure) }
            function onXChanged() { Qt.callLater(xpPill.measure) }
        }
        Connections {
            target: button.parent
            function onXChanged() { Qt.callLater(xpPill.measure) }
        }
        // The panel window resizes after the button when the bar gets thinner or thicker.
        Connections {
            target: button.Window.window
            function onHeightChanged() { remeasure.restart() }
            function onWidthChanged() { remeasure.restart() }
        }
        Timer { id: remeasure; interval: 300; onTriggered: xpPill.measure() }
        Timer { interval: 1500; running: button.xp; onTriggered: xpPill.measure() }
        MouseArea {
            width: xpPill.reach
            height: parent.height
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onPressed: button.wasExpanded = root.menuOpen
            onClicked: mouse => {
                if (mouse.button === Qt.RightButton) openMenu(button, mouse.x, mouse.y);
                else root.menuOpen = !button.wasExpanded;
            }
        }
        readonly property bool modern: button.cfg.xpSubstyle === "modern"
        // Luna (Classic XP): saturated XP green (olive scheme: its muted green), glossier gradient.
        readonly property bool luna: button.cfg.xpSubstyle === "luna"
        readonly property var schemeBase: ({ blue: "#2f6fd8", olive: "#7a924f", silver: "#9a9ab4" })
        readonly property color base: button.cfg.xpStartCustom ? button.cfg.xpStartColor
            : button.cfg.xpStartGreen ? (luna ? (button.cfg.xpColor === "olive" ? "#4f7c3e" : "#1f9420") : "#3c9a3c")
            : button.cfg.xpColor === "custom" ? button.cfg.xpCustomColor
            : (schemeBase[button.cfg.xpColor] || schemeBase.blue)
        // Luna hover: XP's bright glow (lighter and more saturated).
        readonly property color tone: button.frame === 2 ? Qt.darker(base, 1.25)
            : button.frame === 1 ? (luna ? Qt.hsla(base.hslHue, Math.min(1, base.hslSaturation * 1.2),
                                                   Math.min(0.85, base.hslLightness * 1.3), 1) : Qt.lighter(base, 1.12))
            : base
        readonly property real r: modern ? height / 2 : height * 0.45
        // Luna: extra room for the long elliptical end(s).
        readonly property real lunaPad: luna ? Math.round(height * (leftR > 0 ? 0.3 : 0.2)) : 0
        implicitWidth: xpRow.implicitWidth + lunaPad + Math.round(height * (modern ? 0.7 : luna ? (rightR > 0 ? 1 : 0.8) : 0.75))
        // Classic: square on the menu's side (T42 menuPosition): Left = square left, Center = round both
        // sides, Right = square right. Modern stays round on both sides.
        readonly property real leftR: modern || button.cfg.menuPosition !== 0 ? r : 0
        readonly property real rightR: modern || button.cfg.menuPosition !== 2 ? r : 0
        // Luna: a round end is a flat half ellipse (0.22 x the height wide), like XP's button (measured).
        function lunaPath(x0, y0, w, h) {
            const ry = h / 2, rx = Math.min(h * 0.22, w / 2), l = leftR > 0, rt = rightR > 0;
            return "M " + (l ? x0 + rx : x0) + " " + y0 + " H " + (rt ? x0 + w - rx : x0 + w)
                 + (rt ? " A " + rx + " " + ry + " 0 0 1 " + (x0 + w - rx) + " " + (y0 + h) : " V " + (y0 + h))
                 + " H " + (l ? x0 + rx : x0)
                 + (l ? " A " + rx + " " + ry + " 0 0 1 " + (x0 + rx) + " " + y0 : "") + " Z";
        }
        // Luna gloss: XP green at lightness k and saturation s, relative to the tone (measured at #1f9420).
        function shade(k, s) {
            return Qt.hsla(tone.hslHue, Math.min(1, tone.hslSaturation * s), Math.min(0.95, tone.hslLightness * k), 1);
        }
        // Luna flag pane (u, v in 0..1 over the flag) as a polygon, waved and tilted like XP's flag.
        function flagPath(u0, v0, u1, v1, w, h) {
            const pt = (u, v) => ((u + (0.5 - v) * 0.2) * w).toFixed(2) + " "
                + ((0.1 + v * 0.8 + 0.06 * u - 0.07 * Math.sin(2 * Math.PI * u) * (1 - 0.4 * v)) * h).toFixed(2);
            let d = "M " + pt(u0, v0);
            for (let i = 1; i <= 6; i++) d += " L " + pt(u0 + (u1 - u0) * i / 6, v0);
            for (let i = 1; i <= 6; i++) d += " L " + pt(u1, v0 + (v1 - v0) * i / 6);
            for (let i = 1; i <= 6; i++) d += " L " + pt(u1 - (u1 - u0) * i / 6, v1);
            for (let i = 1; i < 6; i++) d += " L " + pt(u0, v1 - (v1 - v0) * i / 6);
            return d + " Z";
        }

        // Soft shadow on the right edge.
        Rectangle {
            visible: !xpPill.luna
            anchors.fill: shape
            anchors.leftMargin: 2
            anchors.rightMargin: -2
            radius: xpPill.r
            topLeftRadius: xpPill.leftR
            bottomLeftRadius: xpPill.leftR
            topRightRadius: xpPill.rightR
            bottomRightRadius: xpPill.rightR
            color: "#000000"
            opacity: xpPill.modern ? 0.18 : 0.35
        }
        // Modern hover glow.
        Rectangle {
            visible: xpPill.modern
            anchors.fill: shape
            anchors.margins: -2
            radius: xpPill.r + 2
            color: "transparent"
            border.width: 2
            border.color: Qt.lighter(xpPill.base, 1.7)
            opacity: button.onFace ? 0.6 : 0
            Behavior on opacity { NumberAnimation { duration: 120 } }
        }
        Rectangle {
            id: shape
            anchors.fill: parent
            anchors.rightMargin: 2
            anchors.topMargin: xpPill.modern ? 2 : 0
            anchors.bottomMargin: anchors.topMargin
            radius: xpPill.r
            topLeftRadius: xpPill.leftR
            bottomLeftRadius: xpPill.leftR
            topRightRadius: xpPill.rightR
            bottomRightRadius: xpPill.rightR
            border.width: 1
            border.color: Qt.darker(xpPill.tone, xpPill.modern ? 1.25 : 1.5)
            visible: !xpPill.luna
            gradient: classicGrad
            Gradient {
                id: classicGrad
                GradientStop { position: 0; color: Qt.lighter(xpPill.tone, xpPill.modern ? 1.2 : 1.4) }
                GradientStop { position: xpPill.modern ? 0.4 : 0.15; color: Qt.lighter(xpPill.tone, xpPill.modern ? 1.05 : 1.1) }
                GradientStop { position: 0.6; color: xpPill.tone }
                GradientStop { position: 1; color: Qt.darker(xpPill.tone, xpPill.modern ? 1.15 : 1.4) }
            }
        }
        // Luna body: soft shadow, glossy fill with a faint darker edge, darker towards the round end(s), hover line.
        Shape {
            id: lunaShape
            visible: xpPill.luna
            anchors.fill: shape
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                strokeColor: "transparent"
                fillColor: Qt.alpha("#000000", 0.15)
                PathSvg { path: xpPill.lunaPath(2.5, 0, lunaShape.width - 0.5, lunaShape.height) }
            }
            ShapePath {
                strokeColor: "transparent"
                fillColor: Qt.alpha("#000000", 0.2)
                PathSvg { path: xpPill.lunaPath(1, 0, lunaShape.width, lunaShape.height) }
            }
            ShapePath {
                strokeWidth: 1
                strokeColor: Qt.alpha(Qt.darker(xpPill.tone, 1.3), 0.6)
                // Measured from XP: pale top highlight, darker band, bright green below, darker bottom line.
                // Pressed: the top turns dark (pushed in, shadow from the top edge).
                fillGradient: LinearGradient {
                    y2: lunaShape.height
                    GradientStop { position: 0; color: button.frame === 2 ? xpPill.shade(0.6, 1.2) : xpPill.shade(1.8, 0.55) }
                    GradientStop { position: 0.07; color: button.frame === 2 ? xpPill.shade(0.75, 1.3) : xpPill.shade(1.6, 0.63) }
                    GradientStop { position: 0.14; color: xpPill.shade(1.25, 0.8) }
                    GradientStop { position: 0.22; color: xpPill.shade(0.94, 1.4) }
                    GradientStop { position: 0.36; color: xpPill.shade(0.83, 1.5) }
                    GradientStop { position: 0.62; color: xpPill.shade(1, 1.55) }
                    GradientStop { position: 0.76; color: xpPill.shade(1.04, 1.55) }
                    GradientStop { position: 0.9; color: xpPill.shade(0.88, 1.55) }
                    GradientStop { position: 0.95; color: xpPill.shade(0.78, 1.55) }
                    GradientStop { position: 1; color: xpPill.shade(0.8, 1) }
                }
                PathSvg { path: xpPill.lunaPath(0.5, 0.5, lunaShape.width - 1.5, lunaShape.height - 1) }
            }
            ShapePath {
                strokeColor: "transparent"
                fillGradient: LinearGradient {
                    x2: lunaShape.width
                    GradientStop { position: 0; color: Qt.alpha("#000000", xpPill.leftR > 0 ? 0.4 : 0) }
                    GradientStop { position: 0.18; color: "transparent" }
                    GradientStop { position: 0.8; color: "transparent" }
                    GradientStop { position: 1; color: Qt.alpha("#000000", xpPill.rightR > 0 ? 0.4 : 0) }
                }
                PathSvg { path: xpPill.lunaPath(0.5, 0.5, lunaShape.width - 1.5, lunaShape.height - 1) }
            }
            ShapePath {
                strokeWidth: 2
                strokeColor: Qt.alpha("#ffffff", button.frame === 1 ? 0.4 : 0)
                fillColor: "transparent"
                PathSvg { path: xpPill.lunaPath(2, 2, lunaShape.width - 5, lunaShape.height - 4) }
            }
        }
        Row {
            id: xpRow
            // Pressed: the label sinks 1 px, like XP's pushed-in button.
            transform: Translate { y: button.frame === 2 ? 1 : 0 }
            anchors.verticalCenter: shape.verticalCenter
            x: xpPill.reach + xpPill.lunaPad + Math.round(xpPill.height * (xpPill.modern ? 0.3 : 0.2))
            spacing: Math.round(xpPill.height * (xpPill.luna ? 0.18 : 0.12))
            Kirigami.Icon {
                visible: !xpPill.luna
                anchors.verticalCenter: parent.verticalCenter
                width: Math.round(xpPill.height * 0.6)
                height: width
                source: button.iconSource
            }
            // Luna: XP's waving four-colour flag, drawn, in place of the button icon: dark backing (gaps and
            // shadow), then the panes with a light top.
            Shape {
                id: flag
                visible: xpPill.luna
                anchors.verticalCenter: parent.verticalCenter
                width: Math.round(xpPill.height * 0.68)
                height: Math.round(xpPill.height * 0.66)
                preferredRendererType: Shape.CurveRenderer
                ShapePath {
                    strokeColor: "transparent"
                    fillColor: Qt.alpha("#000000", 0.45)
                    PathSvg { path: xpPill.flagPath(-0.02, -0.02, 1.05, 1.06, flag.width, flag.height) }
                }
                component Pane: ShapePath {
                    property real u0
                    property real v0
                    property color light
                    property color dark
                    strokeColor: "transparent"
                    fillGradient: LinearGradient {
                        x1: (u0 + 0.1) * flag.width
                        y1: v0 * flag.height
                        x2: (u0 + 0.35) * flag.width
                        y2: (v0 + 0.5) * flag.height
                        GradientStop { position: 0; color: light }
                        GradientStop { position: 1; color: dark }
                    }
                    PathSvg { path: xpPill.flagPath(u0, v0, u0 + 0.47, v0 + 0.47, flag.width, flag.height) }
                }
                Pane { u0: 0; v0: 0; light: "#ffa060"; dark: "#e8481c" }
                Pane { u0: 0.53; v0: 0; light: "#c0ec5c"; dark: "#5aa80c" }
                Pane { u0: 0; v0: 0.53; light: "#78c4ff"; dark: "#1c70e0" }
                Pane { u0: 0.53; v0: 0.53; light: "#fff070"; dark: "#f4a400" }
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: i18n("start")
                color: "#ffffff"
                font.bold: true
                font.italic: true
                font.pixelSize: Math.round(xpPill.height * (xpPill.luna ? 0.66 : 0.5))
                style: xpPill.luna ? Text.Normal : Text.Raised
                styleColor: "#80000000"
                // Luna: XP's soft dark drop shadow.
                layer.enabled: xpPill.luna
                layer.effect: MultiEffect {
                    shadowEnabled: true
                    shadowColor: "#000000"
                    shadowOpacity: 0.75
                    shadowBlur: 0.25
                    shadowHorizontalOffset: 1.5
                    shadowVerticalOffset: 1.5
                }
            }
        }
    }

    // Classic XP tray strip: lighter, with a bevelled left edge, from the tray's left edge to the panel's end
    // (over the clock, as on XP). The tray draws no background and Plasma Styles have no element for it, so it
    // is drawn here and reparented into the panel's containment item: behind the applets, over the panel
    // background (same trick as popupAnchor). Colours measured from real XP screenshots (silver estimated).
    // ponytail: horizontal panels and the three XP schemes only; the tray is found by duck typing (plasmoid.pluginName).
    Rectangle {
        id: trayStrip
        // [gradient stops top -> bottom as [position, colour]], [dark left edge, light left edge]
        readonly property var schemes: ({
            blue: [[[0.01, "#0c59b9"], [0.06, "#139ee9"], [0.1, "#18b5f2"], [0.14, "#139beb"], [0.19, "#1290e8"],
                    [0.63, "#0d8dea"], [0.81, "#0d9ff1"], [0.88, "#0f9eed"], [0.91, "#119be9"], [0.94, "#1392e2"],
                    [0.97, "#137ed7"], [1, "#095bc9"]], ["#1042af", "#18bbff"]],
            olive: [[[0, "#fcf7da"], [0.1, "#ebe3b4"], [0.9, "#ebe3b4"], [1, "#d7c288"]], ["#978c4a", "#f5ecab"]],
            silver: [[[0, "#ffffff"], [0.1, "#f3f3f7"], [0.45, "#e6e6ee"], [0.9, "#dcdce6"], [1, "#c4c4d4"]], ["#9a9ab0", "#ffffff"]]
        })
        readonly property var c: schemes[button.cfg.xpColor] || null
        readonly property bool wanted: button.xp && xpPill.luna && root.xpTheme !== "" && c !== null
        property Item tray: null
        visible: wanted && tray !== null
        z: -1
        // ponytail: 12 fixed stops (the most a scheme has); shorter schemes repeat their last stop
        function st(i) { const g = c ? c[0] : [[1, "transparent"]]; return g[Math.min(i, g.length - 1)]; }
        gradient: Gradient {
            GradientStop { position: trayStrip.st(0)[0]; color: trayStrip.st(0)[1] }
            GradientStop { position: trayStrip.st(1)[0]; color: trayStrip.st(1)[1] }
            GradientStop { position: trayStrip.st(2)[0]; color: trayStrip.st(2)[1] }
            GradientStop { position: trayStrip.st(3)[0]; color: trayStrip.st(3)[1] }
            GradientStop { position: trayStrip.st(4)[0]; color: trayStrip.st(4)[1] }
            GradientStop { position: trayStrip.st(5)[0]; color: trayStrip.st(5)[1] }
            GradientStop { position: trayStrip.st(6)[0]; color: trayStrip.st(6)[1] }
            GradientStop { position: trayStrip.st(7)[0]; color: trayStrip.st(7)[1] }
            GradientStop { position: trayStrip.st(8)[0]; color: trayStrip.st(8)[1] }
            GradientStop { position: trayStrip.st(9)[0]; color: trayStrip.st(9)[1] }
            GradientStop { position: trayStrip.st(10)[0]; color: trayStrip.st(10)[1] }
            GradientStop { position: trayStrip.st(11)[0]; color: trayStrip.st(11)[1] }
        }
        Rectangle { width: 1; height: parent.height; color: trayStrip.c ? trayStrip.c[1][0] : "transparent" }
        Rectangle { x: 1; width: 1; height: parent.height; color: trayStrip.c ? trayStrip.c[1][1] : "transparent" }
        function place() {
            if (!wanted) return;
            const find = (it, d) => {
                if (!it || d > 16) return null;
                if (it.plasmoid && it.plasmoid.pluginName === "org.kde.plasma.systemtray") return it;
                for (const ch of it.children) { const f = find(ch, d + 1); if (f) return f; }
                return null;
            };
            const t = tray || find(button.Window.contentItem, 0);
            let p = t ? t.parent : null;
            while (p && !p.plasmoid) p = p.parent;     // the panel's containment item
            if (!p) return;
            tray = t;
            if (parent !== p) parent = p;
            const q = t.mapToItem(p, 0, 0);
            x = Math.round(q.x);
            y = 0;
            width = p.width - x;
            height = p.height;
        }
        onWantedChanged: Qt.callLater(place)
        Timer { interval: 1500; running: trayStrip.wanted; onTriggered: trayStrip.place() }
        Connections {
            target: trayStrip.tray
            function onWidthChanged() { Qt.callLater(trayStrip.place) }
        }
        Connections {
            target: trayStrip.tray ? trayStrip.tray.parent : null
            function onXChanged() { Qt.callLater(trayStrip.place) }
            function onWidthChanged() { Qt.callLater(trayStrip.place) }
        }
        Connections {
            target: trayStrip.parent
            function onWidthChanged() { Qt.callLater(trayStrip.place) }
        }
    }
}

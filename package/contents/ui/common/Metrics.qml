import QtQuick
import org.kde.plasma.plasmoid
import org.kde.kirigami as Kirigami

// Sizes and colours shared by every style. Sizes are logical px
// (measured from Start11 at 150% and divided by 1.5), times the user scale.
QtObject {
    id: m
    readonly property var cfg: Plasmoid.configuration

    readonly property real s: cfg.uiScale
    function px(v) { return Math.round(v * s); }

    // Windows 10 parts
    readonly property real rail: px(cfg.railMode === 2 ? 200 : 48)
    readonly property real railExpanded: px(220)
    readonly property real list: cfg.hideAppList ? 0 : px(276)
    readonly property real gap: px(cfg.tileGap)
    readonly property real unit: px(22)                  // small tile side
    readonly property int columns: Math.max(3, Math.min(8, cfg.tileColumns))
    readonly property real groupWidth: columns * 2 * unit + (columns * 2 - 1) * gap
    readonly property int groupCols: cfg.menuSize >= 2 ? 2 : 1   // Wide, Large

    // Lists and icons
    readonly property real row: px(cfg.listRowHeight)
    readonly property real listIcon: px(cfg.listIconSize)
    readonly property real railIcon: px(cfg.railIconSize)
    readonly property real pad: px(16)

    // Menu height from "What size should it be?" (or a custom height).
    function heightFor(sizes) {
        if (root.fillScreen) return root.fullRect.height;   // main.qml: screen (minus panel)
        return cfg.menuHeight > 0 ? px(cfg.menuHeight) : px(sizes[cfg.menuSize] ?? sizes[1]);
    }

    // Colours: automatic (Plasma), force dark, force light, custom.
    property int mode: cfg.colorMode     // a style can force a mode for one part (Win7 white pane)
    // T38/T39 Colours page "Show accent colour on Start and taskbar" / "Transparency effects", every style but XP
    // (a part with a forced mode, like the Win7 white pane, sets accentStart: false).
    readonly property bool win10: root.menuKind !== "xp"
    property bool accentStart: win10 && cfg.win10AccentStart
    readonly property color bg: root.menuKind === "windows12" && accentStart ? "#" + root.win12Hex
                              : accentStart ? "#" + root.accentStartHex
                              : root.menuKind === "windows12" && mode !== 3 && (root.winDark ? cfg.win12Dark : cfg.win12Light) >= 0 ? "#" + root.win12Hex   // W11
                              : mode === 0 ? Kirigami.Theme.backgroundColor
                              : mode === 1 ? "#1c1b22" : mode === 2 ? "#f3f3f3" : cfg.menuColor
    readonly property color text: accentStart ? "#ffffff" : mode === 0 ? Kirigami.Theme.textColor
                                : mode === 1 ? "#ffffff" : mode === 2 ? "#1b1b1b" : cfg.textColor
    readonly property color accent: cfg.accentMode === 1 ? cfg.accentColor : (root.userAccent !== "" ? root.userAccent : Kirigami.Theme.highlightColor)
    readonly property color tile: cfg.tileColorMode === 1 ? accent
                                : cfg.tileColorMode === 2 ? cfg.tileColor : text
    readonly property real tileAlpha: cfg.tileColorMode === 0 ? cfg.tileOpacity / 100
                                    : Math.max(0.05, cfg.tileOpacity / 100 * 4)
    readonly property real bgAlpha: cfg.solidMenu || (win10 && !cfg.win10Transparency) ? 1
        : root.menuKind !== "windows12" ? cfg.menuOpacity / 100
        : cfg.win12Transparency < 0 ? cfg.menuOpacity / 100 * 0.35 : 1 - cfg.win12Transparency / 100   // W8 glass (88 % -> 31 %), W11 slider

    readonly property string font: cfg.fontFamily !== "" ? cfg.fontFamily : Kirigami.Theme.defaultFont.family
    readonly property real fontSize: px(cfg.fontSize)
    // 0 / empty = same as the menu font
    readonly property string headerFont: cfg.headerFontFamily !== "" ? cfg.headerFontFamily : font
    readonly property real headerFontSize: cfg.headerFontSize > 0 ? px(cfg.headerFontSize) : fontSize
    readonly property real itemFontSize: cfg.itemFontSize > 0 ? px(cfg.itemFontSize) : fontSize
}

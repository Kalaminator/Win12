import QtQuick

// One rule set for style gating on every settings page (proposal section 4). A page binds its cfg_ values in
// and tests only these; never cfg_menuStyle directly, so mix and match (menuLook etc.) is honoured everywhere.
QtObject {
    property string menuStyle
    property string menuLook
    property string taskbarLook
    property bool taskbarFollowsStyle
    property string windowLook
    property bool windowTheme
    property string taskbarTheme

    readonly property string menuKind: menuLook || menuStyle
    readonly property string taskbarKind: taskbarFollowsStyle ? (taskbarLook || menuStyle) : ""   // "" = the user's own Plasma Style
    readonly property string windowKind: windowTheme ? (windowLook || menuStyle) : ""             // "" = the user's own title bars
    readonly property bool anyXp: menuKind === "xp" || taskbarKind === "xp" || windowKind === "xp"
    readonly property bool taskbarGenerated: taskbarFollowsStyle || taskbarTheme === "custom"
    // The Modern style is the Windows 7 file with a flat look (main.qml styleFile): one family for the 7 options.
    readonly property bool win7Family: menuKind === "windows7" || menuKind === "modern"
    function anyKind(k) { return menuKind === k || taskbarKind === k || windowKind === k; }
}

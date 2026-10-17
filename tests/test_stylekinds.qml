import QtQuick
import "../package/contents/ui/config"

// Gating rules of the settings pages (proposal section 4). Run:
//   QT_FORCE_STDERR_LOGGING=1 QT_QPA_PLATFORM=offscreen /usr/lib/qt6/bin/qml tests/test_stylekinds.qml 2>&1 | grep stylekinds   -> stylekinds OK
Item {
    // Mix and match: Windows 12 card, Windows 7 menu, the user's own Plasma taskbar, Windows 12 title bars.
    property StyleKinds mix: StyleKinds { menuStyle: "windows12"; menuLook: "windows7"; taskbarFollowsStyle: false; windowTheme: true }
    // Plain XP card: everything follows it.
    property StyleKinds xp: StyleKinds { menuStyle: "xp"; taskbarFollowsStyle: true; windowTheme: true }
    // Windows 10 card, own title bars, custom taskbar colour (Override on).
    property StyleKinds custom: StyleKinds { menuStyle: "windows10"; taskbarFollowsStyle: false; windowTheme: false; taskbarTheme: "custom" }
    Component.onCompleted: {
        const checks = [
            mix.menuKind === "windows7", mix.win7Family, mix.taskbarKind === "", mix.windowKind === "windows12",
            !mix.anyXp, mix.anyKind("windows12"), mix.anyKind("windows7"), !mix.anyKind("xp"), !mix.taskbarGenerated,
            xp.menuKind === "xp", xp.taskbarKind === "xp", xp.windowKind === "xp", xp.anyXp, xp.taskbarGenerated, !xp.win7Family,
            custom.menuKind === "windows10", custom.taskbarKind === "", custom.windowKind === "", !custom.anyXp, custom.taskbarGenerated
        ];
        const bad = checks.map((ok, i) => ok ? -1 : i).filter(i => i >= 0);
        console.log(bad.length ? "stylekinds FAIL at " + bad.join(",") : "stylekinds OK");
        Qt.exit(bad.length ? 1 : 0);
    }
}

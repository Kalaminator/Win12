import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.plasma5support as P5Support
import "../code/tiles.js" as Tiles

// Windows 12 taskbar layout (keys taskbarLayout, taskbarMagnetic), shown on the Taskbar page (one instance).
// `cfgPage` is the config page holding cfg_taskbarLayout and cfg_taskbarMagnetic.
ColumnLayout {
    id: box
    required property var cfgPage
    readonly property var values: ["single", "split-left", "split-right", "islands"]
    // W17: the patched KDE packages (same check as main.qml kdePatched).
    property bool kdePatched: true   // until the check answers
    P5Support.DataSource {
        engine: "executable"
        connectedSources: [Tiles.PATCH_CHECK]
        onNewData: (sourceName, data) => { disconnectSource(sourceName); box.kdePatched = String(data.stdout).trim() === "ok"; }
    }
    QQC2.Label {
        visible: !box.kdePatched
        Layout.maximumWidth: Kirigami.Units.gridUnit * 24
        wrapMode: Text.Wrap
        color: box.cfgPage.cfg_taskbarLayout === "islands" ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.textColor
        text: i18n("3 Islands and moving apps between islands need Win12's patched KDE packages: run kde/build.sh from the Win12 source folder, then log out and in. On a stock KDE the One bar, the Split layouts and the Start position work, but a change takes a few seconds.")
    }
    QQC2.ComboBox {
        model: [i18n("One floating bar"), i18n("Split: widgets left, apps centre, tray right"),
                i18n("Split: Start and apps left, tray right"), i18n("3 Islands: pinned apps, open apps, tray")]
        currentIndex: Math.max(0, box.values.indexOf(box.cfgPage.cfg_taskbarLayout))
        onActivated: index => box.cfgPage.cfg_taskbarLayout = box.values[index]
    }
    QQC2.Label {
        text: i18n("Split layouts put the tray, clock and weather into separate floating panels (islands). 3 Islands also splits pinned and open apps; drag an app icon to another island to move it there. Touching the bottom edge shows all of them.")
        Layout.maximumWidth: Kirigami.Units.gridUnit * 24
        wrapMode: Text.Wrap
        enabled: false
    }
    QQC2.CheckBox {
        enabled: box.cfgPage.cfg_taskbarLayout !== "single"
        text: i18n("Magnetic islands")
        checked: box.cfgPage.cfg_taskbarMagnetic
        onToggled: box.cfgPage.cfg_taskbarMagnetic = checked
    }
    QQC2.Label {
        text: i18n("On: the islands sit close together in the middle of the screen. Off: pushed to the screen edges.")
        enabled: false
    }
}

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

// Windows 12 taskbar layout (keys taskbarLayout, taskbarMagnetic), shown on the Style and Taskbar pages.
// `cfgPage` is the config page holding cfg_taskbarLayout and cfg_taskbarMagnetic.
ColumnLayout {
    id: box
    required property var cfgPage
    readonly property var values: ["single", "split-left", "split-right"]
    QQC2.ComboBox {
        model: [i18n("One floating bar"), i18n("Split: widgets left, apps centre, tray right"),
                i18n("Split: Start and apps left, tray right")]
        currentIndex: Math.max(0, box.values.indexOf(box.cfgPage.cfg_taskbarLayout))
        onActivated: index => box.cfgPage.cfg_taskbarLayout = box.values[index]
    }
    QQC2.Label {
        text: i18n("Split layouts put the tray, clock and weather into separate floating panels (islands). Touching the bottom edge shows all of them.")
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

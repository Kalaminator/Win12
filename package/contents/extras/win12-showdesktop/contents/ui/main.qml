import QtQuick
import QtQuick.Layouts
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.kirigami as Kirigami
import org.kde.plasma.plasma5support as P5Support

// Windows taskbar looks: the show desktop button is a thin strip with no icon at the end of the bar.
// Win12's code/showdesktop.js swaps it in for Plasma's Show Desktop widget and back for the KDE Plasma look.
// Click = KWin's Show Desktop shortcut (a toggle: a second click brings the windows back).
// The size hints sit on the PlasmoidItem too: without them the panel gives the strip a full icon slot.
PlasmoidItem {
    id: root
    readonly property bool vertical: Plasmoid.formFactor === PlasmaCore.Types.Vertical
    readonly property int strip: 6
    Plasmoid.backgroundHints: PlasmaCore.Types.NoBackground
    Plasmoid.icon: "user-desktop"
    toolTipMainText: i18n("Show desktop")
    preferredRepresentation: fullRepresentation
    Layout.minimumWidth: vertical ? -1 : strip; Layout.preferredWidth: vertical ? -1 : strip; Layout.maximumWidth: vertical ? -1 : strip
    Layout.minimumHeight: vertical ? strip : -1; Layout.preferredHeight: vertical ? strip : -1; Layout.maximumHeight: vertical ? strip : -1
    P5Support.DataSource { id: sh; engine: "executable"; onNewData: s => disconnectSource(s) }
    fullRepresentation: MouseArea {
        Layout.minimumWidth: root.vertical ? -1 : root.strip; Layout.preferredWidth: root.vertical ? -1 : root.strip; Layout.maximumWidth: root.vertical ? -1 : root.strip
        Layout.minimumHeight: root.vertical ? root.strip : -1; Layout.preferredHeight: root.vertical ? root.strip : -1; Layout.maximumHeight: root.vertical ? root.strip : -1
        hoverEnabled: true
        Accessible.role: Accessible.Button
        Accessible.name: i18n("Show desktop")
        onClicked: sh.connectSource("$(command -v qdbus6 || command -v qdbus-qt6 || { [ -x /usr/lib/qt6/bin/qdbus ] && echo /usr/lib/qt6/bin/qdbus; } || echo qdbus)"
            + " org.kde.kglobalaccel /component/kwin invokeShortcut 'Show Desktop' # " + Date.now())
        Rectangle {
            anchors.fill: parent
            color: Kirigami.Theme.textColor
            opacity: parent.pressed ? 0.25 : parent.containsMouse ? 0.12 : 0
        }
    }
}

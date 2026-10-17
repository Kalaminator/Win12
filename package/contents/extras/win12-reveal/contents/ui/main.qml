import QtQuick
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import QtQuick.Layouts
import org.kde.plasma.plasma5support as P5Support

// W16: 0 px helper that islands.js puts in every panel of a Windows 12 split (fill mode). The KWin script
// win12islandreveal writes its "reveal" key when the pointer touches the bottom edge; NeedsAttention keeps an
// auto-hidden panel shown, so all the islands pop up and hide together.
// The size hints sit on the PlasmoidItem too: without them the panel gives the helper a full icon slot.
PlasmoidItem {
    id: root
    // An app dragged out of a task manager onto this island becomes a new Icon widget: ../code/unpin.js unpins it
    // from the other islands' task managers, so it moves instead of being copied. Skipped: widgets restored at start
    // (armed after 5 s) and the island engine's moves (it holds win12-panel.lock, flock -n gives up).
    // ponytail: a drop during the engine's 30 s magnetic check (~0.1 s) is skipped too and keeps Plasma's copy.
    property bool armed: false
    Timer { interval: 5000; running: true; onTriggered: root.armed = true }
    Connections {
        target: Plasmoid.containment
        function onAppletAdded(applet) {
            if (!root.armed || applet.pluginName !== "org.kde.plasma.icon") return;
            const q = s => "'" + s.replace(/'/g, "'\\''") + "'";
            const file = decodeURIComponent(Qt.resolvedUrl("../code/unpin.js").toString().replace("file://", ""));
            const qdbus = "$(command -v qdbus6 || command -v qdbus-qt6 || { [ -x /usr/lib/qt6/bin/qdbus ] && echo /usr/lib/qt6/bin/qdbus; } || echo qdbus)";
            sh.connectSource("flock -n \"${XDG_RUNTIME_DIR:-/tmp}/win12-panel.lock\" sh -c "
                + q(qdbus + " org.kde.plasmashell /PlasmaShell evaluateScript \"$(sed s/@ID@/" + applet.id + "/ " + q(file) + ")\""));
        }
    }
    P5Support.DataSource { id: sh; engine: "executable"; onNewData: s => disconnectSource(s) }
    Plasmoid.status: Plasmoid.configuration.reveal ? PlasmaCore.Types.NeedsAttentionStatus : PlasmaCore.Types.PassiveStatus
    Plasmoid.backgroundHints: PlasmaCore.Types.NoBackground
    preferredRepresentation: fullRepresentation
    Layout.minimumWidth: 0; Layout.preferredWidth: 0; Layout.maximumWidth: 0
    Layout.minimumHeight: 0; Layout.preferredHeight: 0; Layout.maximumHeight: 0
    fullRepresentation: Item {
        Layout.minimumWidth: 0; Layout.preferredWidth: 0; Layout.maximumWidth: 0
        Layout.minimumHeight: 0; Layout.preferredHeight: 0; Layout.maximumHeight: 0
    }
}

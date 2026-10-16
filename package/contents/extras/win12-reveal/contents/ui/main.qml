import QtQuick
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import QtQuick.Layouts

// W16: 0 px helper that islands.js puts in every panel of a Windows 12 split (fill mode). The KWin script
// win12islandreveal writes its "reveal" key when the pointer touches the bottom edge; NeedsAttention keeps an
// auto-hidden panel shown, so all the islands pop up and hide together.
// The size hints sit on the PlasmoidItem too: without them the panel gives the helper a full icon slot.
PlasmoidItem {
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

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtQuick.Dialogs as Dialogs
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM

// Windows page: title bars and frames; the XP Firefox title bar (proposal 3.6).
KCM.SimpleKCM {
    id: page
    property string cfg_menuStyle   // read only
    property string cfg_menuLook   // read only
    property string cfg_taskbarLook   // read only
    property bool cfg_taskbarFollowsStyle   // read only
    property string cfg_windowLook   // read only
    property bool cfg_windowTheme   // read only
    StyleKinds {
        id: kinds
        menuStyle: page.cfg_menuStyle; menuLook: page.cfg_menuLook; taskbarLook: page.cfg_taskbarLook
        taskbarFollowsStyle: page.cfg_taskbarFollowsStyle; windowLook: page.cfg_windowLook; windowTheme: page.cfg_windowTheme
    }
    property alias cfg_windowOpacity: winAlpha.value
    property alias cfg_windowBlur: winBlur.checked
    property int cfg_windowRadius
    property int cfg_titleHeight
    property string cfg_windowImage
    property string cfg_windowImageMode
    property alias cfg_windowImageAlpha: winImageAlpha.value
    property alias cfg_windowImageTint: winImageTint.checked
    property alias cfg_xpFirefoxTitlebar: ffBox.checked

    Dialogs.FileDialog {
        id: winImageFile
        title: i18n("Pick title bar picture…")
        nameFilters: [i18n("Images (*.png *.jpg *.jpeg)")]
        onAccepted: page.cfg_windowImage = decodeURIComponent(selectedFile.toString().replace("file://", ""))
    }

    Kirigami.FormLayout {
        Kirigami.Separator { Kirigami.FormData.isSection: true; Kirigami.FormData.label: i18n("Title bars and frames") }
        QQC2.Label {
            visible: !page.cfg_windowTheme
            text: i18n("Pick a Windows title bar on the Style page to change these.")
            enabled: false
        }
        // T41: glass options for the windowTheme decoration (main.qml glassSuffix), every style.
        RowLayout {
            enabled: page.cfg_windowTheme
            Kirigami.FormData.label: i18n("Opacity:")
            QQC2.Slider { id: winAlpha; from: 30; to: 100; stepSize: 5 }
            QQC2.Label { text: i18n("%1% opaque", Math.round(winAlpha.value)) }
        }
        QQC2.CheckBox {
            id: winBlur
            enabled: page.cfg_windowTheme && winAlpha.value < 100
            text: i18n("Blur behind translucent frames")
        }
        RowLayout {
            enabled: page.cfg_windowTheme
            Kirigami.FormData.label: i18n("Corner radius:")
            QQC2.CheckBox {
                id: ownRadius
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
            enabled: page.cfg_windowTheme
            Kirigami.FormData.label: i18n("Title bar height:")
            QQC2.CheckBox {
                id: ownTitle
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
            enabled: page.cfg_windowTheme
            Kirigami.FormData.label: i18n("Title bar picture:")
            QQC2.Label { text: page.cfg_windowImage ? page.cfg_windowImage.split("/").pop() : i18n("None") }
            QQC2.Button { text: i18n("Pick image…"); icon.name: "document-open"; onClicked: winImageFile.open() }
            QQC2.Button { icon.name: "edit-clear"; visible: page.cfg_windowImage !== ""; onClicked: page.cfg_windowImage = "" }
        }
        QQC2.ComboBox {
            enabled: page.cfg_windowTheme && page.cfg_windowImage !== ""
            Kirigami.FormData.label: i18n("Placement:")
            textRole: "text"
            valueRole: "value"
            model: [{ value: "stretch", text: i18n("Stretch") }, { value: "tile", text: i18n("Tile") }]
            Component.onCompleted: currentIndex = Math.max(0, indexOfValue(page.cfg_windowImageMode))
            onActivated: page.cfg_windowImageMode = currentValue
        }
        RowLayout {
            enabled: page.cfg_windowTheme && page.cfg_windowImage !== ""
            Kirigami.FormData.label: i18n("Picture opacity:")
            QQC2.Slider { id: winImageAlpha; from: 0; to: 100; stepSize: 1 }
            QQC2.Label { text: i18n("%1% opaque", Math.round(winImageAlpha.value)) }
        }
        QQC2.CheckBox {
            id: winImageTint
            enabled: page.cfg_windowTheme && page.cfg_windowImage !== ""
            text: i18n("Tint with the accent colour")
        }

        Kirigami.Separator { Kirigami.FormData.isSection: true; Kirigami.FormData.label: i18n("Windows XP"); visible: kinds.windowKind === "xp" }
        QQC2.CheckBox {
            id: ffBox
            visible: kinds.windowKind === "xp"
            text: i18n("Use the system title bar in Firefox")
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.text: i18n("Firefox draws its own title bar, so the XP one does not show. Restart Firefox for this to take effect.")
        }
    }
}

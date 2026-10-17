pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtQuick.Dialogs as Dialogs
import Qt.labs.folderlistmodel
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM
import org.kde.kquickcontrols as KQuickControls

// Start11 "Adjust colour, transparency, ..." and "Menu texture and appearance settings".
KCM.SimpleKCM {
    id: page
    property alias cfg_colorMode: colorMode.currentIndex
    property alias cfg_menuColor: menuColor.color
    property alias cfg_textColor: textColor.color
    property alias cfg_accentMode: accentMode.currentIndex
    property alias cfg_accentColor: accentColor.color
    property alias cfg_menuOpacity: opacity.value
    property alias cfg_solidMenu: solid.checked
    property alias cfg_cornerRadius: radius.value
    property alias cfg_greyIcons: greyIcons.checked
    property alias cfg_animateMenu: animateMenu.checked
    property string cfg_texture
    property alias cfg_textureMode: textureMode.currentIndex
    property alias cfg_textureOpacity: textureOpacity.value
    property alias cfg_darkenTexture: darken.checked
    property alias cfg_colorizeTexture: colorize.checked
    property string cfg_fontFamily
    property alias cfg_fontSize: fontSize.value
    property string cfg_headerFontFamily
    property alias cfg_headerFontSize: headerFontSize.value
    property alias cfg_itemFontSize: itemFontSize.value

    FolderListModel {
        id: textures
        folder: Qt.resolvedUrl("../../images/MenuTextures/")
        nameFilters: ["*.png", "*.jpg"]
        showDirs: false
    }
    readonly property var textureNames: Array.from({ length: textures.count }, (_, i) => textures.get(i, "fileName"))

    Dialogs.FileDialog {
        id: textureFile
        title: i18n("Pick custom texture…")
        nameFilters: [i18n("Images (*.png *.jpg *.jpeg *.webp *.svg)")]
        onAccepted: page.cfg_texture = decodeURIComponent(selectedFile.toString().replace("file://", ""))
    }


    Kirigami.FormLayout {
        Kirigami.Separator { Kirigami.FormData.isSection: true; Kirigami.FormData.label: i18n("Colour") }

        QQC2.ComboBox {
            id: colorMode
            Kirigami.FormData.label: i18n("Start menu colours:")
            model: [i18n("Automatic (Plasma colours)"), i18n("Force dark mode menu"), i18n("Force light mode menu"), i18n("Custom colour")]
        }
        KQuickControls.ColorButton { id: menuColor; visible: colorMode.currentIndex === 3; Kirigami.FormData.label: i18n("Menu colour:"); showAlphaChannel: false }
        KQuickControls.ColorButton { id: textColor; visible: colorMode.currentIndex === 3; Kirigami.FormData.label: i18n("Text colour:"); showAlphaChannel: false }
        RowLayout {
            Kirigami.FormData.label: i18n("Selected item accent colour:")
            QQC2.ComboBox { id: accentMode; model: [i18n("Plasma accent colour"), i18n("Custom")] }
            KQuickControls.ColorButton { id: accentColor; visible: accentMode.currentIndex === 1; showAlphaChannel: false }
        }

        Kirigami.Separator { Kirigami.FormData.isSection: true; Kirigami.FormData.label: i18n("Transparency") }

        QQC2.CheckBox { id: solid; text: i18n("Make the start menu solid") }
        RowLayout {
            Kirigami.FormData.label: i18n("Menu background transparency level:")
            enabled: !solid.checked
            QQC2.Slider { id: opacity; from: 0; to: 100; stepSize: 1 }
            QQC2.Label { text: i18n("%1% solid", Math.round(opacity.value)) }
        }
        QQC2.SpinBox {
            id: radius
            Kirigami.FormData.label: i18n("Round the corners of the menu:")
            from: 0
            to: 30
            textFromValue: v => v === 0 ? i18n("Square") : v + " px"
            valueFromText: t => parseInt(t) || 0
        }

        QQC2.CheckBox { id: greyIcons; Kirigami.FormData.label: i18n("Icons:"); text: i18n("Show app icons in grey") }
        QQC2.CheckBox { id: animateMenu; Kirigami.FormData.label: i18n("Animation:"); text: i18n("Fade the menu in when it opens") }

        Kirigami.Separator { Kirigami.FormData.isSection: true; Kirigami.FormData.label: i18n("Texture") }

        RowLayout {
            Kirigami.FormData.label: i18n("Apply an image to the background:")
            QQC2.ComboBox {
                id: textureBox
                Layout.minimumWidth: Kirigami.Units.gridUnit * 12
                readonly property bool custom: page.cfg_texture.startsWith("/")
                model: [i18n("None")].concat(page.textureNames.map(n => n.replace(/(_x\d)?\.(png|jpg)$/, "")))
                    .concat(custom ? [page.cfg_texture.split("/").pop()] : [])
                currentIndex: page.cfg_texture === "" ? 0
                    : custom ? model.length - 1 : page.textureNames.indexOf(page.cfg_texture) + 1
                onActivated: i => {
                    if (i === 0) page.cfg_texture = "";
                    else if (i <= page.textureNames.length) page.cfg_texture = page.textureNames[i - 1];
                }
            }
            QQC2.Button { text: i18n("Pick custom texture…"); icon.name: "document-open"; onClicked: textureFile.open() }
        }
        QQC2.ComboBox {
            id: textureMode
            Kirigami.FormData.label: i18n("Texture alignment:")
            enabled: page.cfg_texture !== ""
            model: [i18n("Tile"), i18n("Stretch to show entire texture"), i18n("Fit"), i18n("Fill")]
        }
        RowLayout {
            Kirigami.FormData.label: i18n("Fade texture:")
            enabled: page.cfg_texture !== ""
            QQC2.Slider { id: textureOpacity; from: 5; to: 100; stepSize: 1 }
            QQC2.Label { text: Math.round(textureOpacity.value) + " %" }
        }
        QQC2.CheckBox { id: darken; enabled: page.cfg_texture !== ""; text: i18n("Darken texture") }
        QQC2.CheckBox { id: colorize; enabled: page.cfg_texture !== ""; text: i18n("Recolour the texture with the menu colour") }
        Image {
            visible: page.cfg_texture !== ""
            Layout.preferredWidth: Kirigami.Units.gridUnit * 16
            Layout.preferredHeight: Kirigami.Units.gridUnit * 4
            fillMode: Image.Tile
            source: page.cfg_texture === "" ? ""
                : page.cfg_texture.startsWith("/") ? "file://" + page.cfg_texture
                : Qt.resolvedUrl("../../images/MenuTextures/" + page.cfg_texture)
        }

        Kirigami.Separator { Kirigami.FormData.isSection: true; Kirigami.FormData.label: i18n("Fonts") }

        QQC2.ComboBox {
            Kirigami.FormData.label: i18n("Menu font:")
            Layout.minimumWidth: Kirigami.Units.gridUnit * 14
            model: [i18n("** Default font **")].concat(Qt.fontFamilies())
            currentIndex: page.cfg_fontFamily === "" ? 0 : Math.max(0, model.indexOf(page.cfg_fontFamily))
            onActivated: i => page.cfg_fontFamily = i === 0 ? "" : model[i]
        }
        QQC2.SpinBox {
            id: fontSize
            Kirigami.FormData.label: i18n("Font size:")
            from: 7
            to: 24
            textFromValue: v => v + " px"
            valueFromText: t => parseInt(t) || 11
        }
        QQC2.ComboBox {
            Kirigami.FormData.label: i18n("Header font:")
            Layout.minimumWidth: Kirigami.Units.gridUnit * 14
            model: [i18n("** Menu font **")].concat(Qt.fontFamilies())
            currentIndex: page.cfg_headerFontFamily === "" ? 0 : Math.max(0, model.indexOf(page.cfg_headerFontFamily))
            onActivated: i => page.cfg_headerFontFamily = i === 0 ? "" : model[i]
        }
        QQC2.SpinBox {
            id: headerFontSize
            Kirigami.FormData.label: i18n("Header font size:")
            from: 0
            to: 32
            textFromValue: v => v === 0 ? i18n("Menu size") : v + " px"
            valueFromText: t => parseInt(t) || 0
        }
        QQC2.SpinBox {
            id: itemFontSize
            Kirigami.FormData.label: i18n("Item font size:")
            from: 0
            to: 24
            textFromValue: v => v === 0 ? i18n("Menu size") : v + " px"
            valueFromText: t => parseInt(t) || 0
        }
    }
}

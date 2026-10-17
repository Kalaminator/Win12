pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtQuick.Dialogs as Dialogs
import Qt.labs.folderlistmodel
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM
import org.kde.kquickcontrols as KQuickControls
import org.kde.iconthemes as KIconThemes

// Start button & shortcuts: the button and its right-click menu, the left-hand rail, the power button and menu (proposal 3.4).
KCM.SimpleKCM {
    id: page
    property int cfg_buttonMode
    property string cfg_buttonImage
    property string cfg_buttonIcon
    property alias cfg_buttonIconScale: iconScale.value
    property alias cfg_squaresColor: squaresColor.color
    property alias cfg_squaresAccent: squaresAccent.color
    property var cfg_buttonMenuItems: []
    property bool cfg_xpStartGreen
    property bool cfg_xpStartCustom
    property alias cfg_xpStartColor: xpStartColor.color
    property alias cfg_railMode: modeBox.currentIndex
    property alias cfg_railIconSize: iconSize.value
    property var cfg_railItems: []
    property var cfg_powerItems: []
    property string cfg_powerButtonAction
    property alias cfg_railAlignBottom: alignBottom.checked
    property alias cfg_smallPlaces: smallPlaces.checked
    property alias cfg_powerButtonIcon: powerIcon.checked
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

    readonly property var menuChoices: [
        ["settings", i18n("Settings")], ["files", i18n("File Explorer")], ["monitor", i18n("System Monitor")],
        ["terminal", i18n("Terminal")], ["run", i18n("Run…")], ["desktop", i18n("Show desktop")], ["editpanel", i18n("Edit panel")], ["autohide", i18n("Auto-hide panel")], ["power", i18n("Lock, sign out, sleep, restart, shut down")]
    ]

    readonly property var builtins: ({
        "user": [i18n("User picture"), "user-identity"],
        "documents": [i18n("Documents"), "folder-documents"],
        "downloads": [i18n("Downloads"), "folder-download"],
        "music": [i18n("Music"), "folder-music"],
        "pictures": [i18n("Pictures"), "folder-pictures"],
        "videos": [i18n("Videos"), "folder-videos"],
        "home": [i18n("Personal folder"), "user-home"],
        "network": [i18n("Network"), "network-workgroup"],
        "computer": [i18n("Computer"), "computer"],
        "trash": [i18n("Trash"), "user-trash"],
        "recent": [i18n("Recent files"), "document-open-recent"],
        "games": [i18n("Games"), "applications-games"],
        "printers": [i18n("Printers"), "printer"],
        "users": [i18n("Users"), "system-users"],
        "updates": [i18n("Updates"), "system-software-update"],
        "separator": [i18n("Separator"), "list-remove"],
        "files": [i18n("File Explorer"), "system-file-manager"],
        "personalization": [i18n("Personalization"), "preferences-desktop-theme-global"],
        "settings": [i18n("Settings"), "configure"],
        "terminal": [i18n("Terminal"), "utilities-terminal"],
        "monitor": [i18n("System Monitor"), "utilities-system-monitor"],
        "run": [i18n("Run…"), "system-run"],
        "win12": [i18n("Win12 Settings"), "configure"],
        "power": [i18n("Power"), "system-shutdown"]
    })
    readonly property var powerChoices: [
        ["lock-screen", i18n("Lock")], ["logout", i18n("Sign out")], ["switch-user", i18n("Switch user")],
        ["suspend", i18n("Sleep")], ["hibernate", i18n("Hibernate")], ["reboot", i18n("Restart")],
        ["shutdown", i18n("Shut down")]
    ]

    function label(id) {
        return id.startsWith("custom|") ? id.split("|")[1] : (builtins[id] ? builtins[id][0] : id);
    }
    function iconOf(id) {
        return id.startsWith("custom|") ? (id.split("|")[2] || "folder") : (builtins[id] ? builtins[id][1] : "");
    }
    function move(i, d) {
        const l = cfg_railItems.slice();
        if (i + d < 0 || i + d >= l.length) return;
        [l[i], l[i + d]] = [l[i + d], l[i]];
        cfg_railItems = l;
    }
    function removeAt(i) {
        const l = cfg_railItems.slice();
        l.splice(i, 1);
        cfg_railItems = l;
    }
    function add(id) {
        // New items go just above "Power", like Start11's list.
        const l = cfg_railItems.slice();
        const p = l.indexOf("power");
        l.splice(p >= 0 ? p : l.length, 0, id);
        cfg_railItems = l;
    }

    FolderListModel {
        id: buttons
        folder: Qt.resolvedUrl("../../images/StartButtons/")
        nameFilters: ["*.png"]
        showDirs: false
    }

    KIconThemes.IconDialog {
        id: iconDialog
        onIconNameChanged: name => page.cfg_buttonIcon = name
    }
    Dialogs.FileDialog {
        id: imageFile
        title: i18n("Pick custom image…")
        nameFilters: [i18n("Images (*.png *.svg *.jpg *.webp)")]
        onAccepted: {
            const path = decodeURIComponent(selectedFile.toString().replace("file://", ""));
            if (page.cfg_buttonMode === 0) page.cfg_buttonImage = path;
            else page.cfg_buttonIcon = path;
        }
    }

    Kirigami.FormLayout {
        Kirigami.Separator { Kirigami.FormData.isSection: true; Kirigami.FormData.label: i18n("Start button") }
        ColumnLayout {
            Kirigami.FormData.label: i18n("Start button:")
            QQC2.RadioButton { text: i18n("Icon"); checked: page.cfg_buttonMode === 2; onToggled: page.cfg_buttonMode = 2 }
            QQC2.RadioButton { text: i18n("Picture"); checked: page.cfg_buttonMode === 0; onToggled: page.cfg_buttonMode = 0 }
            QQC2.RadioButton { text: i18n("Four squares"); checked: page.cfg_buttonMode === 1; onToggled: page.cfg_buttonMode = 1 }
        }

        // ---- Icon ----
        RowLayout {
            visible: page.cfg_buttonMode === 2
            Kirigami.FormData.label: i18n("Icon:")
            QQC2.Button {
                icon.source: page.cfg_buttonIcon === "" ? "start-here-kde"
                    : page.cfg_buttonIcon.startsWith("/") ? "file://" + page.cfg_buttonIcon : page.cfg_buttonIcon
                text: page.cfg_buttonIcon === "" ? i18n("System default") : page.cfg_buttonIcon.split("/").pop()
                onClicked: iconDialog.open()
            }
            QQC2.Button { text: i18n("Pick custom image…"); icon.name: "document-open"; onClicked: imageFile.open() }
            QQC2.Button {
                visible: page.cfg_buttonIcon !== ""
                text: i18n("Reset icon")
                icon.name: "edit-undo"
                onClicked: page.cfg_buttonIcon = ""
            }
        }
        RowLayout {
            visible: page.cfg_buttonMode === 2
            Kirigami.FormData.label: i18n("Icon size:")
            QQC2.Slider { id: iconScale; from: 30; to: 100; stepSize: 1 }
            QQC2.Label { text: Math.round(iconScale.value) + " %" }
        }

        // ---- Image sheet: shows frame 0 (normal) of each 3-frame image ----
        GridLayout {
            visible: page.cfg_buttonMode === 0
            Kirigami.FormData.label: i18n("Picture:")
            columns: 6
            Repeater {
                model: buttons
                delegate: QQC2.Button {
                    id: b
                    required property string fileName
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 3.5
                    Layout.preferredHeight: Kirigami.Units.gridUnit * 3.5
                    checkable: true
                    checked: page.cfg_buttonImage === fileName
                    onClicked: page.cfg_buttonImage = fileName
                    QQC2.ToolTip.text: fileName.replace(".png", "")
                    QQC2.ToolTip.visible: hovered
                    contentItem: Item {
                        clip: true
                        Image {
                            anchors.horizontalCenter: parent.horizontalCenter
                            source: Qt.resolvedUrl("../../images/StartButtons/" + b.fileName)
                            readonly property real k: Math.min(parent.width / Math.max(1, sourceSize.width),
                                                               parent.height / Math.max(1, sourceSize.height / 3))
                            width: sourceSize.width * k
                            height: sourceSize.height * k
                            y: (parent.height - height / 3) / 2
                            smooth: true
                            mipmap: true
                        }
                    }
                }
            }
        }
        QQC2.Button {
            visible: page.cfg_buttonMode === 0
            text: page.cfg_buttonImage.startsWith("/") ? page.cfg_buttonImage.split("/").pop() : i18n("Use a custom picture…")
            icon.name: "document-open"
            onClicked: imageFile.open()
        }
        QQC2.Label {
            visible: page.cfg_buttonMode === 0
            text: i18n("A custom picture holds 3 images stacked top to bottom: normal, mouse over, pressed.")
            opacity: 0.7
            font: Kirigami.Theme.smallFont
            wrapMode: Text.WordWrap
            Layout.maximumWidth: Kirigami.Units.gridUnit * 22
        }

        // ---- Squares ----
        KQuickControls.ColorButton {
            id: squaresAccent; visible: page.cfg_buttonMode === 1; showAlphaChannel: false
            Kirigami.FormData.label: i18n("First square:")
        }
        KQuickControls.ColorButton {
            id: squaresColor; visible: page.cfg_buttonMode === 1; showAlphaChannel: false
            Kirigami.FormData.label: i18n("Other squares:")
        }

        // ---- XP Start button colour (moved from the Style page) ----
        RowLayout {
            visible: kinds.menuKind === "xp"
            Kirigami.FormData.label: i18n("XP Start button colour:")
            QQC2.ComboBox {
                model: [i18n("XP green"), i18n("Colour scheme"), i18n("Custom")]
                currentIndex: page.cfg_xpStartCustom ? 2 : page.cfg_xpStartGreen ? 0 : 1
                onActivated: index => {
                    page.cfg_xpStartCustom = index === 2;
                    page.cfg_xpStartGreen = index === 0;
                }
            }
            KQuickControls.ColorButton { id: xpStartColor; visible: page.cfg_xpStartCustom; showAlphaChannel: false }
        }

        Kirigami.Separator { Kirigami.FormData.isSection: true; Kirigami.FormData.label: i18n("Right-click menu") }

        ColumnLayout {
            Kirigami.FormData.label: i18n("Show in the right-click menu:")
            Repeater {
                model: page.menuChoices
                delegate: QQC2.CheckBox {
                    required property var modelData
                    text: modelData[1]
                    checked: page.cfg_buttonMenuItems.includes(modelData[0])
                    onToggled: {
                        const set = new Set(page.cfg_buttonMenuItems);
                        checked ? set.add(modelData[0]) : set.delete(modelData[0]);
                        page.cfg_buttonMenuItems = page.menuChoices.map(c => c[0]).filter(x => set.has(x));
                    }
                }
            }
        }

        Kirigami.Separator { Kirigami.FormData.isSection: true; Kirigami.FormData.label: i18n("Menu shortcuts") }
        QQC2.ComboBox {
            id: modeBox
            Kirigami.FormData.label: i18n("Shortcut labels:")
            model: [i18n("Only show icons"), i18n("Show names when the mouse is over it"), i18n("Always show icons and names")]
        }
        QQC2.SpinBox {
            id: iconSize
            Kirigami.FormData.label: i18n("Icon size:")
            from: 12
            to: 48
            textFromValue: v => v + " px"
            valueFromText: t => parseInt(t) || 18
        }

        QQC2.CheckBox {
            id: alignBottom; visible: kinds.menuKind === "windows10"
            text: i18n("Align the icons to the bottom")
        }
        QQC2.CheckBox {
            id: smallPlaces; visible: kinds.win7Family || kinds.menuKind === "xp" || kinds.menuKind === "pro"
            text: i18n("Small place rows")
        }
        ColumnLayout {
            Kirigami.FormData.label: i18n("Top to bottom:")
            Layout.minimumWidth: Kirigami.Units.gridUnit * 20
            Repeater {
                model: page.cfg_railItems
                delegate: RowLayout {
                    id: entry
                    required property string modelData
                    required property int index
                    Kirigami.Icon { source: page.iconOf(entry.modelData); Layout.preferredWidth: Kirigami.Units.iconSizes.small; Layout.preferredHeight: Layout.preferredWidth }
                    QQC2.Label { text: page.label(entry.modelData); Layout.fillWidth: true }
                    QQC2.ToolButton { icon.name: "arrow-up"; enabled: entry.index > 0; onClicked: page.move(entry.index, -1) }
                    QQC2.ToolButton { icon.name: "arrow-down"; enabled: entry.index < page.cfg_railItems.length - 1; onClicked: page.move(entry.index, 1) }
                    QQC2.ToolButton { icon.name: "list-remove"; onClicked: page.removeAt(entry.index) }
                }
            }
            RowLayout {
                QQC2.ComboBox {
                    id: addBox
                    Layout.fillWidth: true
                    textRole: "text"
                    valueRole: "id"
                    model: Object.keys(page.builtins).filter(id => id === "separator" || !page.cfg_railItems.includes(id))
                        .map(id => ({ id: id, text: page.builtins[id][0] }))
                }
                QQC2.Button { text: i18n("Add"); icon.name: "list-add"; enabled: addBox.count > 0; onClicked: page.add(addBox.currentValue) }
            }
            QQC2.Button { text: i18n("Add custom location…"); icon.name: "folder-new"; onClicked: customDialog.open() }
        }


        Kirigami.Separator { Kirigami.FormData.isSection: true; Kirigami.FormData.label: i18n("Power") }
        QQC2.ComboBox {
            Kirigami.FormData.label: i18n("Clicking the power button:")
            textRole: "text"
            valueRole: "id"
            model: [{ id: "menu", text: i18n("Show the power menu") }].concat(page.powerChoices.map(c => ({ id: c[0], text: c[1] })))
            Component.onCompleted: currentIndex = Math.max(0, indexOfValue(page.cfg_powerButtonAction))
            onActivated: page.cfg_powerButtonAction = currentValue
        }
        ColumnLayout {
            Kirigami.FormData.label: i18n("Show in the power menu:")
            Repeater {
                model: page.powerChoices
                delegate: QQC2.CheckBox {
                    required property var modelData
                    text: modelData[1]
                    checked: page.cfg_powerItems.includes(modelData[0])
                    onToggled: {
                        const set = new Set(page.cfg_powerItems);
                        checked ? set.add(modelData[0]) : set.delete(modelData[0]);
                        page.cfg_powerItems = page.powerChoices.map(c => c[0]).filter(x => set.has(x));
                    }
                }
            }
        }
        QQC2.CheckBox {
            id: powerIcon; visible: kinds.win7Family
            text: i18n("Show an icon on the Shut down button")
        }
    }

    // "Add custom location..." - a folder, a web address, or a command.
    KIconThemes.IconDialog {
        id: customIconDialog
        onIconNameChanged: name => customIcon.text = name
    }
    Kirigami.PromptDialog {
        id: customDialog
        title: i18n("Add custom location")
        standardButtons: Kirigami.Dialog.Ok | Kirigami.Dialog.Cancel
        onAccepted: {
            if (customName.text && customTarget.text) {
                const target = customKind.currentIndex === 1 ? "cmd:" + customTarget.text : customTarget.text;
                page.add(["custom", customName.text.replace(/\|/g, "/"), customIcon.text, target].join("|"));
            }
            customName.text = customTarget.text = customIcon.text = "";
        }
        Kirigami.FormLayout {
            QQC2.TextField { id: customName; Kirigami.FormData.label: i18n("Name:") }
            QQC2.ComboBox { id: customKind; Kirigami.FormData.label: i18n("Opens:"); model: [i18n("Folder or web address"), i18n("Command")] }
            QQC2.TextField {
                id: customTarget
                Kirigami.FormData.label: customKind.currentIndex === 1 ? i18n("Command:") : i18n("Location:")
                placeholderText: customKind.currentIndex === 1 ? "kate" : "/home/you/Projects  or  https://…"
            }
            RowLayout {
                Kirigami.FormData.label: i18n("Icon:")
                QQC2.TextField { id: customIcon; placeholderText: "folder" }
                QQC2.Button { icon.name: "document-open"; onClicked: customIconDialog.open() }
            }
        }
    }
}

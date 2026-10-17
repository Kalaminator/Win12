pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM

// Start11 "How should search work?"
KCM.SimpleKCM {
    id: page
    property alias cfg_groupResults: group.checked
    property alias cfg_showWebSearch: web.checked
    property alias cfg_searchFilterIcons: filterIcons.checked
    property alias cfg_classicSearch: classic.checked
    property alias cfg_searchHistoryEnabled: history.checked
    property var cfg_searchHistory: []
    property alias cfg_webSearchUrl: webUrl.text
    property var cfg_searchRunners: []

    // KRunner plugins. Empty list in the config = all of them.
    readonly property var runners: [
        ["krunner_services", i18n("Apps")],
        ["krunner_systemsettings", i18n("Settings")],
        ["baloosearch", i18n("Files (file search index)")],
        ["krunner_recentdocuments", i18n("Recent documents")],
        ["krunner_placesrunner", i18n("Places")],
        ["calculator", i18n("Calculator")],
        ["unitconverter", i18n("Unit converter")],
        ["krunner_shell", i18n("Commands")],
        ["krunner_webshortcuts", i18n("Web shortcuts (gg:, wp:, ...)")],
        ["krunner_bookmarksrunner", i18n("Browser bookmarks")],
        ["krunner_kwin", i18n("Open windows")],
        ["krunner_sessions", i18n("Sessions and power")],
        ["krunner_charrunner", i18n("Special characters")],
        ["krunner_dictionary", i18n("Dictionary")]
    ]
    readonly property bool all: cfg_searchRunners.length === 0

    Kirigami.FormLayout {
        Kirigami.Separator { Kirigami.FormData.isSection: true; Kirigami.FormData.label: i18n("Results") }
        QQC2.CheckBox { id: group; text: i18n("Group results by type (Apps, Files, Settings …)") }
        QQC2.CheckBox { id: web; text: i18n("Show the \"Search the internet\" button") }
        QQC2.CheckBox { id: filterIcons; text: i18n("Show filter icons above the results (Apps, Documents, Web …)") }
        QQC2.CheckBox { id: classic; text: i18n("Classic search: dense one-line results, no Best match card") }
        QQC2.TextField {
            id: webUrl
            enabled: web.checked
            Kirigami.FormData.label: i18n("Search the internet with:")
            Layout.minimumWidth: Kirigami.Units.gridUnit * 18
            placeholderText: "https://duckduckgo.com/?q=%s"
        }
        QQC2.Label {
            text: i18n("%s is replaced by what you typed.")
            opacity: 0.7
            font: Kirigami.Theme.smallFont
        }

        Kirigami.Separator { Kirigami.FormData.isSection: true; Kirigami.FormData.label: i18n("History") }
        QQC2.CheckBox { id: history; text: i18n("Remember recent searches") }
        QQC2.Button {
            text: i18n("Clear search history")
            enabled: page.cfg_searchHistory.length > 0
            onClicked: page.cfg_searchHistory = []
        }

        Kirigami.Separator { Kirigami.FormData.isSection: true; Kirigami.FormData.label: i18n("What to search") }

        QQC2.CheckBox {
            text: i18n("Use everything KRunner can search")
            checked: page.all
            onToggled: page.cfg_searchRunners = checked ? [] : page.runners.slice(0, 3).map(r => r[0])
        }
        QQC2.Label {
            text: i18n("Off: starts with Apps, Settings and Files; tick more below.")
            enabled: false
            font: Kirigami.Theme.smallFont
        }
        Repeater {
            model: page.runners
            delegate: QQC2.CheckBox {
                required property var modelData
                text: modelData[1]
                enabled: !page.all
                checked: page.all || page.cfg_searchRunners.includes(modelData[0])
                onToggled: {
                    const set = new Set(page.cfg_searchRunners);
                    checked ? set.add(modelData[0]) : set.delete(modelData[0]);
                    page.cfg_searchRunners = page.runners.map(r => r[0]).filter(x => set.has(x));
                }
            }
        }
    }
}

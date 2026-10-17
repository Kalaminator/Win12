import org.kde.plasma.configuration

// The Win12 settings window: nine pages, one topic each (docs/research/settings-reorg-proposal.md).
ConfigModel {
    ConfigCategory {
        name: i18n("Style")
        icon: "preferences-desktop-theme-global"
        source: "config/ConfigStyle.qml"
    }
    ConfigCategory {
        name: i18n("Start menu")
        icon: "view-grid"
        source: "config/ConfigStartMenu.qml"
    }
    ConfigCategory {
        name: i18n("Apps list")
        icon: "view-list-details"
        source: "config/ConfigAppsList.qml"
    }
    ConfigCategory {
        name: i18n("Start button & shortcuts")
        icon: "start-here-kde"
        source: "config/ConfigButton.qml"
    }
    ConfigCategory {
        name: i18n("Taskbar")
        icon: "preferences-desktop-plasma"
        source: "config/ConfigTaskbar.qml"
    }
    ConfigCategory {
        name: i18n("Windows")
        icon: "preferences-system-windows"
        source: "config/ConfigWindows.qml"
    }
    ConfigCategory {
        name: i18n("Appearance")
        icon: "preferences-desktop-color"
        source: "config/ConfigAppearance.qml"
    }
    ConfigCategory {
        name: i18n("Search")
        icon: "search"
        source: "config/ConfigSearch.qml"
    }
    ConfigCategory {
        name: i18n("Advanced")
        icon: "configure"
        source: "config/ConfigAdvanced.qml"
    }
}

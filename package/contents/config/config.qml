import org.kde.plasma.configuration

// The Win12 settings window, laid out like Start11Config.
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
        name: i18n("Layout")
        icon: "view-list-details"
        source: "config/ConfigMenu.qml"
    }
    ConfigCategory {
        name: i18n("Shortcuts")
        icon: "bookmarks"
        source: "config/ConfigRail.qml"
    }
    ConfigCategory {
        name: i18n("Look")
        icon: "preferences-desktop-color"
        source: "config/ConfigLook.qml"
    }
    ConfigCategory {
        name: i18n("Colours")
        icon: "color-management"
        source: "config/ConfigColours.qml"
    }
    ConfigCategory {
        name: i18n("Search")
        icon: "search"
        source: "config/ConfigSearch.qml"
    }
    ConfigCategory {
        name: i18n("Taskbar")
        icon: "preferences-desktop-icons"
        source: "config/ConfigTaskbar.qml"
    }
    ConfigCategory {
        name: i18n("Icons")
        icon: "preferences-desktop-icons"
        source: "config/ConfigIcons.qml"
    }
    ConfigCategory {
        name: i18n("Start Button")
        icon: "start-here-kde"
        source: "config/ConfigButton.qml"
    }
}

#include "ribbonspec.h"
#include <KLocalizedString>

namespace Ribbon {
namespace {
using S = Size;
Button B(const char *action, const KLocalizedString &text, const char *icon, S size = S::Small, bool menu = false)
{
    return {QString::fromLatin1(action), text.toString(), QString::fromLatin1(icon), size, menu};
}
}

const QList<Tab> &spec()
{
    static const QList<Tab> t = {
        {QStringLiteral("file"), i18n("File"), {
            {QString(), {
                B("file_new_window", ki18n("New window"), "window-new"),
                B("file_terminal", ki18n("Open terminal here"), "utilities-terminal"),
                B("file_options", ki18n("Options"), "configure"),
                B("file_help", ki18n("Help"), "help-contents"),
                B("file_close", ki18n("Close"), "window-close"),
            }},
        }, true},
        {QStringLiteral("home"), i18n("Home"), {
            {i18n("Clipboard"), {
                B("home_pin", ki18n("Pin to Quick access"), "bookmark-new", S::Big),
                B("home_copy", ki18n("Copy"), "edit-copy"),
                B("home_paste", ki18n("Paste"), "edit-paste"),
                B("home_cut", ki18n("Cut"), "edit-cut"),
                B("home_copy_path", ki18n("Copy path"), "edit-copy"),
                B("home_paste_shortcut", ki18n("Paste shortcut"), "edit-link"),
                B("home_undo", ki18n("Undo"), "edit-undo"),
                B("home_redo", ki18n("Redo"), "edit-redo"),
            }},
            {i18n("Organize"), {
                B("home_move_to", ki18n("Move to"), "edit-cut", S::Big),
                B("home_copy_to", ki18n("Copy to"), "edit-copy"),
                B("home_delete", ki18n("Delete"), "edit-delete", S::Small, true),
                B("home_rename", ki18n("Rename"), "edit-rename"),
            }},
            {i18n("New"), {
                B("home_new_folder", ki18n("New folder"), "folder-new", S::Big),
                B("home_new_item", ki18n("New item"), "document-new", S::Small, true),
            }},
            {i18n("Open"), {
                B("home_properties", ki18n("Properties"), "document-properties", S::Big),
                B("home_open", ki18n("Open"), "document-open", S::Small, true),
                B("home_edit", ki18n("Edit"), "document-edit"),
                B("home_history", ki18n("History"), "view-history", S::Small, true),
            }},
            {i18n("Select"), {
                B("home_select_all", ki18n("Select all"), "edit-select-all", S::Big),
                B("home_select_none", ki18n("Select none"), "edit-select-none"),
                B("home_select_invert", ki18n("Invert selection"), "edit-select-invert"),
            }},
        }},
        {QStringLiteral("share"), i18n("Share"), {
            {i18n("Send"), {
                B("share_share", ki18n("Share"), "document-share", S::Big, true),
                B("share_email", ki18n("Email"), "mail-send"),
                B("share_zip", ki18n("Zip"), "archive-insert"),
                B("share_print", ki18n("Print"), "document-print"),
                B("share_terminal", ki18n("Open terminal"), "utilities-terminal"),
                B("share_security", ki18n("Advanced security"), "security-high"),
            }},
        }},
        {QStringLiteral("view"), i18n("View"), {
            {i18n("Panes"), {
                B("view_nav_pane", ki18n("Navigation pane"), "view-split-left-right", S::Big),
                B("view_preview_pane", ki18n("Preview pane"), "view-preview"),
                B("view_details_pane", ki18n("Details pane"), "view-list-details"),
            }},
            {i18n("Layout"), {
                B("view_layout_xlarge", ki18n("Extra large icons"), "view-list-icons"),
                B("view_layout_large", ki18n("Large icons"), "view-list-icons"),
                B("view_layout_medium", ki18n("Medium icons"), "view-list-icons"),
                B("view_layout_small", ki18n("Small icons"), "view-list-icons"),
                B("view_layout_list", ki18n("List"), "view-list-text"),
                B("view_layout_details", ki18n("Details"), "view-list-details"),
                B("view_layout_tiles", ki18n("Tiles"), "view-list-icons"),
                B("view_layout_content", ki18n("Content"), "view-form-table"),
            }},
            {i18n("Current view"), {
                B("view_sort", ki18n("Sort by"), "view-sort", S::Big, true),
                B("view_group", ki18n("Group by"), "object-group", S::Small, true),
                B("view_columns", ki18n("Add columns"), "view-form-table", S::Small, true),
            }},
            {i18n("Tags"), {
                B("tags_add", ki18n("Add tag"), "tag-new", S::Big, true),
                B("tags_remove", ki18n("Remove tag"), "tag-delete", S::Small, true),
                B("tags_manage", ki18n("Manage tags…"), "tag-properties"),
                B("tags_group_name", ki18n("Group: tag name"), "object-group"),
                B("tags_group_colour", ki18n("Group: tag colour"), "color-management"),
                B("tags_flags", ki18n("Show tag flags"), "flag"),
            }, 2},
            {i18n("Show/hide"), {
                B("view_checkboxes", ki18n("Item check boxes"), "checkbox"),
                B("view_extensions", ki18n("File name extensions"), "document-properties"),
                B("view_hidden", ki18n("Hidden items"), "view-hidden", S::Big),
            }},
            {i18n("Split view"), {
                B("view_split", ki18n("Split view"), "view-split-left-right", S::Big),
            }},
        }},
        {QStringLiteral("advanced"), i18n("Advanced"), {
            {i18n("Select"), {
                B("adv_select_rule", ki18n("Select by rule"), "edit-find", S::Big),
            }, 3},
            {i18n("Rename"), {
                B("adv_rename_many", ki18n("Rename many"), "edit-rename", S::Big),
                B("adv_rename_undo", ki18n("Undo last rename"), "edit-undo"),
            }, 3},
            {i18n("Bulk actions"), {
                B("adv_select_matches", ki18n("Select matches"), "edit-select-all"),
                B("adv_copy_matches", ki18n("Copy matches to…"), "edit-copy"),
                B("adv_move_matches", ki18n("Move matches to…"), "edit-cut"),
                B("adv_rename_matches", ki18n("Rename matches…"), "edit-rename"),
                B("adv_tag_matches", ki18n("Tag matches…"), "tag-new"),
                B("adv_trash_matches", ki18n("Trash matches"), "user-trash", S::Big),
            }, 3},
            {i18n("Automate"), {}, 4},
        }, false, 3},
    };
    return t;
}

QStringList actionIds()
{
    QStringList r;
    for (const Tab &t : spec())
        for (const Group &g : t.groups)
            for (const Button &b : g.buttons)
                r << b.action;
    return r;
}

const Button *button(const QString &actionId)
{
    for (const Tab &t : spec())
        for (const Group &g : t.groups)
            for (const Button &b : g.buttons)
                if (b.action == actionId)
                    return &b;
    return nullptr;
}
}

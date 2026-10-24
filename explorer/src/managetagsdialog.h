#pragma once
#include "newtagdialog.h"
#include "tagsearch.h"
#include <QDialog>
#include <QSet>
#include <QUrl>
#include <functional>
#include <optional>

class QLabel;
class QListWidget;
class QPushButton;
class TagRegistry;
class TagStore;

// Manage tags: rename, recolour or delete a registry tag in the files that carry it. "Everywhere" =
// what Baloo indexes plus a walk of the current folder tree (no whole-disk walk).
class ManageTagsDialog : public QDialog
{
    Q_OBJECT
public:
    ManageTagsDialog(TagRegistry *registry, TagStore *store, const QUrl &currentFolder, QWidget *parent);
    void setExecNewTagDialog(std::function<bool(NewTagDialog &)> exec) { m_exec = std::move(exec); }
    void setEngineForTests(TagSearch::Engine e) { m_engine = e; } // Walk: only the folder walk, Baloo is not consulted
    void setConfirmDelete(std::function<bool(const Tag &)> c) { m_confirm = std::move(c); } // test seam
    void reject() override; // ignored while a run is in progress
    void applyTo(const Tag &from, const std::optional<Tag> &to); // to == nullopt: delete

Q_SIGNALS:
    void applied(int filesChanged);

private:
    void edit(bool word);
    void fill();
    void searchDone(bool ok, const QString &error, bool isWalk);
    void setBusy(bool);
    TagRegistry *m_registry;
    TagStore *m_store;
    QUrl m_folder;
    QListWidget *m_list;
    QLabel *m_status;
    QPushButton *m_rename;
    QPushButton *m_recolour;
    QPushButton *m_delete;
    QPushButton *m_close = nullptr;
    std::function<bool(const Tag &)> m_confirm;
    std::function<bool(NewTagDialog &)> m_exec = [](NewTagDialog &d) { return d.exec() == QDialog::Accepted; };
    std::optional<TagSearch::Engine> m_engine;
    TagSearch m_walk;
    TagSearch m_baloo;
    int m_pending = 0;
    bool m_failed = false; // the folder walk failed or a write failed
    QString m_balooError;
    QString m_error;
    QSet<QUrl> m_done; // a file found by both searches is changed once
    Tag m_from;
    std::optional<Tag> m_to;
};

#include "managetagsdialog.h"
#include "tagregistry.h"
#include "tagribbon.h"
#include "tagstore.h"
#include <KLocalizedString>
#include <KMessageBox>
#include <QSignalBlocker>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

ManageTagsDialog::ManageTagsDialog(TagRegistry *registry, TagStore *store, const QUrl &currentFolder, QWidget *parent)
    : QDialog(parent)
    , m_registry(registry)
    , m_store(store)
    , m_folder(currentFolder)
    , m_list(new QListWidget(this))
    , m_status(new QLabel(this))
    , m_rename(new QPushButton(i18n("Rename…"), this))
    , m_recolour(new QPushButton(i18n("Recolour…"), this))
    , m_delete(new QPushButton(i18n("Delete everywhere"), this))
    , m_walk(this)
    , m_baloo(this)
{
    setWindowTitle(i18n("Manage tags"));
    m_rename->setObjectName(QStringLiteral("rename"));
    m_recolour->setObjectName(QStringLiteral("recolour"));
    m_delete->setObjectName(QStringLiteral("delete"));
    m_status->setObjectName(QStringLiteral("status"));
    m_list->setObjectName(QStringLiteral("list"));

    auto *note = new QLabel(i18n("Changes apply to indexed locations and to %1 (with subfolders)", m_folder.isLocalFile() ? m_folder.toLocalFile() : m_folder.toDisplayString()), this);
    note->setWordWrap(true);
    auto *box = new QDialogButtonBox(QDialogButtonBox::Close, this);
    m_close = box->button(QDialogButtonBox::Close);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    m_confirm = [this](const Tag &t) {
        return KMessageBox::warningContinueCancel(this, i18n("Remove the tag \"%1\" from all files it is on?", t.display()), i18n("Delete tag"), KStandardGuiItem::del()) == KMessageBox::Continue;
    };
    auto *buttons = new QVBoxLayout;
    buttons->addWidget(m_rename);
    buttons->addWidget(m_recolour);
    buttons->addWidget(m_delete);
    buttons->addStretch();
    auto *row = new QHBoxLayout;
    row->addWidget(m_list, 1);
    row->addLayout(buttons);
    auto *lay = new QVBoxLayout(this);
    lay->addLayout(row);
    lay->addWidget(note);
    lay->addWidget(m_status);
    lay->addWidget(box);

    fill();
    connect(m_registry, &TagRegistry::changed, this, &ManageTagsDialog::fill);
    connect(m_list, &QListWidget::currentRowChanged, this, [this] { setBusy(m_pending > 0); });
    connect(m_rename, &QPushButton::clicked, this, [this] { edit(true); });
    connect(m_recolour, &QPushButton::clicked, this, [this] { edit(false); });
    connect(m_delete, &QPushButton::clicked, this, [this] {
        if (m_list->currentRow() >= 0 && m_confirm(m_registry->tags().at(m_list->currentRow())))
            applyTo(m_registry->tags().at(m_list->currentRow()), std::nullopt);
    });
    for (TagSearch *s : {&m_walk, &m_baloo}) {
        connect(s, &TagSearch::found, this, [this](const KFileItemList &items) {
            // ponytail: writes run on the UI thread, one batch (<= 200 files) per event-loop turn; worker thread if thousands of files carry the tag
            QList<QUrl> changed;
            {
                QSignalBlocker block(m_store); // one tagsChanged per batch, not per file
                for (const KFileItem &i : items) {
                    if (m_done.contains(i.url()))
                        continue;
                    m_done.insert(i.url());
                    const TagStore::Error e = m_to ? m_store->replace(i.url(), m_from, *m_to) : m_store->remove({i.url()}, m_from);
                    if (e == TagStore::Error::None)
                        changed << i.url();
                    else {
                        m_failed = true;
                        m_error = TagStore::errorText(e);
                    }
                }
            }
            if (!changed.isEmpty())
                Q_EMIT m_store->tagsChanged(changed);
            m_status->setText(i18np("%1 file changed…", "%1 files changed…", m_done.size()));
        });
        connect(s, &TagSearch::finished, this, [this, s](bool ok, const QString &error) { searchDone(ok, error, s == &m_walk); });
    }
    setBusy(false);
}

void ManageTagsDialog::reject()
{
    if (!m_pending)
        QDialog::reject();
}

void ManageTagsDialog::fill()
{
    const int row = m_list->currentRow();
    m_list->clear();
    for (const Tag &t : m_registry->tags())
        m_list->addItem(new QListWidgetItem(TagRibbon::icon(t), t.display()));
    if (m_list->count())
        m_list->setCurrentRow(qBound(0, row, qMax(0, m_list->count() - 1)));
}

void ManageTagsDialog::setBusy(bool busy)
{
    const bool pick = !busy && m_list->currentRow() >= 0;
    m_rename->setEnabled(pick);
    m_recolour->setEnabled(pick);
    m_delete->setEnabled(pick);
    m_list->setEnabled(!busy);
    m_close->setEnabled(!busy);
}

void ManageTagsDialog::edit(bool word)
{
    if (m_list->currentRow() < 0)
        return;
    const Tag from = m_registry->tags().at(m_list->currentRow());
    NewTagDialog d(this);
    d.setWindowTitle(word ? i18n("Rename tag") : i18n("Recolour tag"));
    d.setTag(from);
    if (!m_exec(d))
        return;
    const Tag to = d.tag();
    if (to.colour >= 0 && !(to == from))
        applyTo(from, to);
}

void ManageTagsDialog::applyTo(const Tag &from, const std::optional<Tag> &to)
{
    if (m_pending)
        return;
    m_from = from;
    m_to = to;
    m_done.clear();
    m_failed = false;
    m_error.clear();
    m_balooError.clear();
    setBusy(true);
    m_status->setText(i18n("Working…"));
    const QList<Tag> wanted{from};
    m_pending = 1;
    m_walk.start(m_folder, wanted, TagSearch::Mode::Any, m_engine.value_or(TagSearch::Engine::Walk));
    const QUrl root = QUrl::fromLocalFile(QStringLiteral("/"));
    if (!m_engine && TagSearch::engineFor(root) == TagSearch::Engine::Baloo) {
        ++m_pending;
        m_baloo.start(root, wanted, TagSearch::Mode::Any, TagSearch::Engine::Baloo);
    }
}

void ManageTagsDialog::searchDone(bool ok, const QString &error, bool isWalk)
{
    if (!ok) {
        if (isWalk) {
            m_failed = true;
            m_error = error;
        } else {
            m_balooError = error; // a failed index search alone does not strand the registry
        }
    }
    if (--m_pending > 0)
        return;
    if (!m_failed) { // a failed run leaves the registry alone so the user can retry
        if (!m_to)
            m_registry->remove(m_from);
        else {
            Tag cur = m_from;
            if (m_to->word != cur.word) {
                m_registry->rename(cur, m_to->word);
                cur.word = m_to->word;
            }
            if (m_to->colour != cur.colour)
                m_registry->recolour(cur, m_to->colour);
        }
    }
    QString text = m_failed ? i18n("Some files could not be changed: %1", m_error) : i18np("%1 file changed", "%1 files changed", m_done.size());
    if (!m_balooError.isEmpty())
        text += QLatin1Char(' ') + i18n("Indexed locations could not be searched: %1", m_balooError);
    m_status->setText(text);
    setBusy(false);
    Q_EMIT applied(m_done.size());
}

#include "taglist.h"
#include "tagregistry.h"
#include "tagribbon.h"
#include <KLocalizedString>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QToolButton>
#include <QVBoxLayout>

TagList::TagList(TagRegistry *registry, QWidget *parent)
    : QWidget(parent)
    , m_registry(registry)
    , m_list(new QListWidget(this))
    , m_all(new QToolButton(this))
    , m_any(new QToolButton(this))
{
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection); // plain click = one, Ctrl+click toggles
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setIconSize(QSize(80, TagRibbon::Height));
    for (auto *b : {m_all, m_any}) {
        b->setCheckable(true);
        b->setAutoRaise(true);
    }
    m_all->setText(i18n("all picked tags"));
    m_any->setText(i18n("any picked tag"));
    m_all->setChecked(true);
    auto *group = new QButtonGroup(this);
    group->addButton(m_all);
    group->addButton(m_any);

    auto *modes = new QHBoxLayout;
    modes->addWidget(m_all);
    modes->addWidget(m_any);
    modes->addStretch();
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new QLabel(i18n("Tags (Ctrl+click = more)"), this));
    layout->addWidget(m_list);
    layout->addLayout(modes);

    connect(m_list, &QListWidget::itemSelectionChanged, this, [this] { Q_EMIT filterChanged(picked(), mode()); });
    connect(group, &QButtonGroup::buttonToggled, this, [this](QAbstractButton *, bool on) {
        if (on && !m_list->selectedItems().isEmpty())
            Q_EMIT filterChanged(picked(), mode());
    });
    connect(m_registry, &TagRegistry::changed, this, &TagList::reload);
    reload();
}

void TagList::reload()
{
    const QList<Tag> keep = picked();
    m_list->blockSignals(true);
    m_list->clear();
    for (const Tag &t : m_registry->tags()) {
        auto *item = new QListWidgetItem(TagRibbon::icon(t), QString(), m_list);
        item->setData(Qt::UserRole, t.encode());
        item->setToolTip(t.display());
        item->setSelected(keep.contains(t));
    }
    m_list->blockSignals(false);
    if (picked().size() != keep.size()) // a picked tag was renamed or deleted: its results must not stay shown
        Q_EMIT filterChanged(picked(), mode());
}

QList<Tag> TagList::picked() const
{
    QList<Tag> out;
    for (QListWidgetItem *i : m_list->selectedItems())
        out << Tag::decode(i->data(Qt::UserRole).toString());
    return out;
}

TagSearch::Mode TagList::mode() const
{
    return m_any->isChecked() ? TagSearch::Mode::Any : TagSearch::Mode::All;
}

void TagList::clearPicks(bool silent)
{
    const bool had = !m_list->selectedItems().isEmpty();
    {
        QSignalBlocker block(m_list);
        m_list->clearSelection();
    }
    if (had && !silent)
        Q_EMIT filterChanged({}, mode());
}

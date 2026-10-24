#include "tagpicker.h"
#include <KLocalizedString>
#include <QButtonGroup>
#include <QGridLayout>
#include <QLineEdit>
#include <QPixmap>
#include <QRegularExpressionValidator>
#include <QToolButton>
#include <QVBoxLayout>

TagPicker::TagPicker(QWidget *parent)
    : QWidget(parent)
    , m_word(new QLineEdit(this))
    , m_colours(new QButtonGroup(this))
{
    m_word->setPlaceholderText(i18n("Tag name (optional)"));
    // user.xdg.tags is a comma list and '/' splits colour from word, so neither can be typed
    m_word->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("[^,/\"]*")), m_word));

    auto *grid = new QGridLayout;
    grid->setSpacing(4);
    for (int i = 0; i < TagColours::Count; ++i) {
        QPixmap px(18, 18);
        px.fill(TagColours::colour(i));
        auto *b = new QToolButton(this);
        b->setIcon(QIcon(px));
        b->setIconSize(px.size());
        b->setCheckable(true);
        b->setAutoRaise(false);
        b->setToolTip(TagColours::name(i));
        m_colours->addButton(b, i);
        grid->addWidget(b, i / 8, i % 8); // 6 rows x 8 columns
    }
    connect(m_colours, &QButtonGroup::idClicked, this, [this] { Q_EMIT colourPicked(tag()); });

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(m_word);
    lay->addLayout(grid);
}

Tag TagPicker::tag() const
{
    return Tag{m_colours->checkedId(), m_word->text().trimmed()};
}

void TagPicker::setTag(const Tag &t)
{
    m_word->setText(t.word);
    if (QAbstractButton *b = m_colours->button(t.colour))
        b->setChecked(true);
}

QAbstractButton *TagPicker::colourButton(int i) const
{
    return m_colours->button(i);
}

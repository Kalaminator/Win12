#include "newtagdialog.h"
#include <KLocalizedString>
#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QToolButton>
#include <QVBoxLayout>

NewTagDialog::NewTagDialog(QWidget *parent)
    : QDialog(parent)
    , m_word(new QLineEdit(this))
    , m_colours(new QButtonGroup(this))
{
    setWindowTitle(i18n("New tag"));
    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    box->button(QDialogButtonBox::Ok)->setEnabled(false);
    m_word->setPlaceholderText(i18n("Tag name (optional)"));
    // user.xdg.tags is a comma list and '/' splits colour from word, so neither can be typed
    m_word->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("[^,/]*")), m_word));

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
    connect(m_colours, &QButtonGroup::idClicked, this, [box] { box->button(QDialogButtonBox::Ok)->setEnabled(true); });
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *lay = new QVBoxLayout(this);
    lay->addWidget(new QLabel(i18n("Name"), this));
    lay->addWidget(m_word);
    lay->addWidget(new QLabel(i18n("Colour"), this));
    lay->addLayout(grid);
    lay->addWidget(box);
}

Tag NewTagDialog::tag() const
{
    return Tag{m_colours->checkedId(), m_word->text().trimmed()};
}

void NewTagDialog::setTag(const Tag &t)
{
    m_word->setText(t.word);
    if (QAbstractButton *b = m_colours->button(t.colour)) {
        b->setChecked(true);
        findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->setEnabled(true);
    }
}

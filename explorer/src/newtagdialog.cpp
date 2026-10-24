#include "newtagdialog.h"
#include "tagpicker.h"
#include <KLocalizedString>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QVBoxLayout>

NewTagDialog::NewTagDialog(QWidget *parent)
    : QDialog(parent)
    , m_picker(new TagPicker(this))
{
    setWindowTitle(i18n("New tag"));
    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    box->button(QDialogButtonBox::Ok)->setEnabled(false);
    connect(m_picker, &TagPicker::colourPicked, this, [box] { box->button(QDialogButtonBox::Ok)->setEnabled(true); });
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto *lay = new QVBoxLayout(this);
    lay->addWidget(m_picker);
    lay->addWidget(box);
}

Tag NewTagDialog::tag() const
{
    return m_picker->tag();
}

void NewTagDialog::setTag(const Tag &t)
{
    m_picker->setTag(t);
    if (t.colour >= 0 && m_picker->colourButton(t.colour))
        findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->setEnabled(true);
}

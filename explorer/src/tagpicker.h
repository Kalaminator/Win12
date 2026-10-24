#pragma once
#include "tag.h"
#include <QWidget>

class QAbstractButton;
class QButtonGroup;
class QLineEdit;

// A name field (may be empty) and the 48 colours; a colour click announces the tag. Shared by NewTagDialog and the Tags panel.
class TagPicker : public QWidget
{
    Q_OBJECT
public:
    explicit TagPicker(QWidget *parent = nullptr);
    Tag tag() const;
    void setTag(const Tag &);
    QLineEdit *wordEdit() const { return m_word; }
    QAbstractButton *colourButton(int i) const;

Q_SIGNALS:
    void colourPicked(const Tag &);

private:
    QLineEdit *m_word;
    QButtonGroup *m_colours;
};

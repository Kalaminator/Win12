#pragma once
#include "tag.h"
#include <QDialog>

class TagPicker;

// New tag: a word (may be empty) and one of the 48 colours; OK stays disabled until a colour is picked.
class NewTagDialog : public QDialog
{
    Q_OBJECT
public:
    explicit NewTagDialog(QWidget *parent);
    Tag tag() const;
    void setTag(const Tag &);

private:
    TagPicker *m_picker;
};

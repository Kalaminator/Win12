#pragma once
#include "renameplan.h"
#include <QDialog>

class QComboBox;
class QDialogButtonBox;
class QKeyEvent;
class QLineEdit;
class QListWidget;
class QSpinBox;
class QTreeWidget;
class QWidget;

class RenameManyDialog : public QDialog
{
    Q_OBJECT
public:
    RenameManyDialog(const QList<RenameItem> &items /*view order*/, const QSet<QString> &existingNames, QWidget *parent);
    void setBlocks(const QList<RenameBlock> &);
    QList<RenameBlock> blocks() const; // in list order
    QList<PlannedRename> plan() const; // what Rename will do

protected:
    void keyPressEvent(QKeyEvent *) override;

private:
    void addBlock(const RenameBlock &);
    void showEditor();
    void editorChanged();
    void refresh();
    static QString summary(const RenameBlock &);

    QList<RenameItem> m_items;
    QSet<QString> m_existing;
    QListWidget *m_list;
    QTreeWidget *m_tree;
    QDialogButtonBox *m_buttons;
    QWidget *m_editor;
    QSpinBox *m_digits, *m_start, *m_step;
    QLineEdit *m_text, *m_replace;
    QComboBox *m_case;
    bool m_loading = false;
};

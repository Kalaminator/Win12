#pragma once
#include "renameplan.h"
#include "formularun.h"
#include <QDialog>

class QComboBox;
class QDialogButtonBox;
class QKeyEvent;
class QLineEdit;
class QListWidget;
class QSpinBox;
class QTreeWidget;
class QToolButton;
class QWidget;
class FormulaEdit;
class RuleLibrary;

class RenameManyDialog : public QDialog
{
    Q_OBJECT
public:
    RenameManyDialog(const QList<RenameItem> &items /*view order*/, const QSet<QString> &existingNames, QWidget *parent);
    void setBlocks(const QList<RenameBlock> &);
    QList<RenameBlock> blocks() const; // in list order
    QList<PlannedRename> plan() const; // what Rename will do

    enum class Mode { Blocks, Formula };
    void setMode(Mode);
    Mode mode() const { return m_mode; }
    void setFormula(const QString &);
    QString formula() const;
    void setLibrary(RuleLibrary *l) { m_library = l; } // Save as... -> Kind::Rename
    QUrl base() const; // common ancestor folder of all items (the first item's folder when they share one); FormulaRun's base

Q_SIGNALS:
    void evaluated(); // a formula run finished (ok or not)

protected:
    void keyPressEvent(QKeyEvent *) override;

private:
    void addBlock(const RenameBlock &);
    void showEditor();
    void editorChanged();
    void refresh();
    void formulaChanged(bool valid);
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
    Mode m_mode = Mode::Blocks;
    FormulaEdit *m_fx;
    QWidget *m_blocksRow;
    QToolButton *m_modeBlocks, *m_modeFormula;
    RuleLibrary *m_library = nullptr;
    FormulaRun m_run;
    KFileItemList m_fxItems;
    bool m_running = false; // a run is active or the formula edit still debounces
    bool m_fxValid = false;
    QList<FxValue> m_values; // row order
    QString m_runError;
};

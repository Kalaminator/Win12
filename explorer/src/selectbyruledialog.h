#pragma once
#include "rulematcher.h"
#include "rulesearch.h"
#include <KFileItem>
#include <QDialog>
#include <QList>
#include <QMap>
#include <QUrl>

class KUrlRequester;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;
class QVBoxLayout;

class SelectByRuleDialog : public QDialog
{
    Q_OBJECT
public:
    enum class Action { Select, CopyTo, MoveTo, Rename, Trash, DeleteForever, Zip, CopyPaths, AddTag };
    SelectByRuleDialog(const QUrl &lookIn, Action defaultAction, QWidget *parent);
    ~SelectByRuleDialog() override;

    void setRules(const QList<Rule> &);
    QList<Rule> rules() const;
    void setRecursive(bool);
    void find(); // starts RuleSearch; the Find button and Enter in a value field call this
    Action chosenAction() const; // valid after accept()
    KFileItemList ticked() const; // in result-list order
    bool keepStructure() const;
    QUrl lookIn() const;
    void reject() override;

protected:
    void keyPressEvent(QKeyEvent *) override;

public:

Q_SIGNALS:
    void searchFinished(); // after the last found() was appended

private:
    struct RuleRow {
        QWidget *widget;
        QComboBox *field, *test;
        QLineEdit *value;
        QComboBox *kind;
        QToolButton *remove;
    };
    void addRow(const Rule &);
    void fillTests(RuleRow &);
    void recompile(); // error label + Find enabled
    void updateState(); // count label + action buttons
    void act(Action);

    QUrl m_lookIn;
    KUrlRequester *m_url;
    QCheckBox *m_recursive, *m_keep;
    QVBoxLayout *m_rowsLayout;
    QList<RuleRow> m_rows;
    QLabel *m_error, *m_count;
    QPushButton *m_find;
    QTreeWidget *m_tree;
    QMap<Action, QPushButton *> m_buttons;
    QList<QAction *> m_menuActs;
    RuleSearch m_search;
    Action m_chosen;
};

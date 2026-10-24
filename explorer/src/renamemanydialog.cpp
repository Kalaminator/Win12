#include "renamemanydialog.h"
#include "formulaedit.h"
#include "rulelibrary.h"
#include <KLocalizedString>
#include <KMessageBox>
#include <QInputDialog>
#include <QAbstractButton>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QSpinBox>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

static RenameBlock blockOf(const QListWidgetItem *i)
{
    return i->data(Qt::UserRole).value<RenameBlock>();
}

QString RenameManyDialog::summary(const RenameBlock &b)
{
    switch (b.kind) {
    case RenameBlock::Number:
        return i18n("Number · %1 digits · from %2 · step %3", b.digits, b.start, b.step);
    case RenameBlock::Text:
        return i18n("Text · \"%1\"", b.text);
    case RenameBlock::CurrentName:
        return i18n("Current name");
    case RenameBlock::Date:
        return i18n("Date · %1", b.text);
    case RenameBlock::FindReplace:
        return i18n("Replace · \"%1\" → \"%2\"", b.text, b.replace);
    case RenameBlock::Case:
        return b.caseMode == RenameBlock::Upper ? i18n("Case · UPPER") : b.caseMode == RenameBlock::Lower ? i18n("Case · lower") : i18n("Case · Title");
    }
    return {};
}

RenameManyDialog::RenameManyDialog(const QList<RenameItem> &items, const QSet<QString> &existingNames, QWidget *parent)
    : QDialog(parent)
    , m_items(items)
    , m_existing(existingNames)
{
    qRegisterMetaType<RenameBlock>();
    setWindowTitle(i18n("Rename many"));
    auto *lay = new QVBoxLayout(this);

    auto *modes = new QHBoxLayout;
    auto mkMode = [&](const QString &text, const QString &name) {
        auto *b = new QToolButton;
        b->setObjectName(name);
        b->setText(text);
        b->setCheckable(true);
        b->setAutoExclusive(true);
        modes->addWidget(b);
        return b;
    };
    m_modeBlocks = mkMode(i18n("Blocks"), QStringLiteral("modeBlocks"));
    m_modeFormula = mkMode(i18n("fx Formula"), QStringLiteral("modeFormula"));
    m_modeBlocks->setChecked(true);
    modes->addStretch();
    lay->addLayout(modes);
    m_fx = new FormulaEdit;
    m_fx->hide();
    lay->addWidget(m_fx);
    for (const RenameItem &i : items)
        m_fxItems << (i.item.isNull() ? KFileItem(i.url) : i.item);
    connect(m_modeBlocks, &QToolButton::clicked, this, [this] { setMode(Mode::Blocks); });
    connect(m_modeFormula, &QToolButton::clicked, this, [this] { setMode(Mode::Formula); });
    connect(m_fx, &FormulaEdit::formulaChanged, this, &RenameManyDialog::formulaChanged);
    // every keystroke makes the shown plan stale at once; the debounced formulaChanged starts the new run
    if (auto *le = m_fx->findChild<QLineEdit *>(QStringLiteral("formulaEdit")))
        connect(le, &QLineEdit::textChanged, this, [this] {
            m_run.stop();
            m_values.clear();
            m_runError.clear();
            m_running = true;
            refresh();
        });
    connect(m_fx, &FormulaEdit::saveRequested, this, [this](const QString &text) {
        if (!m_library)
            return;
        saveRuleInteractively(*m_library, SavedRule::Kind::Rename, text, this);
    });
    connect(&m_run, &FormulaRun::results, this, [this](const QList<FxRowResult> &rows) {
        for (const FxRowResult &r : rows)
            m_values << r.value;
    });
    connect(&m_run, &FormulaRun::finished, this, [this](bool ok, const QString &err) {
        m_running = false;
        m_runError = ok ? QString() : err;
        refresh();
        Q_EMIT evaluated();
    });

    m_blocksRow = new QWidget;
    auto *row = new QHBoxLayout(m_blocksRow);
    row->setContentsMargins(0, 0, 0, 0);
    row->addWidget(new QLabel(i18n("New name =")));
    m_list = new QListWidget;
    m_list->setObjectName(QStringLiteral("blockList"));
    m_list->setFlow(QListView::LeftToRight);
    m_list->setDragDropMode(QAbstractItemView::InternalMove);
    m_list->setFixedHeight(48);
    row->addWidget(m_list, 1);
    auto *add = new QToolButton;
    add->setObjectName(QStringLiteral("addBlockButton"));
    add->setText(i18n("+ block"));
    add->setPopupMode(QToolButton::InstantPopup);
    auto *menu = new QMenu(add);
    const std::pair<RenameBlock::Kind, QString> kinds[] = {{RenameBlock::Number, i18n("Number")},
                                                           {RenameBlock::Text, i18n("Text")},
                                                           {RenameBlock::CurrentName, i18n("Current name")},
                                                           {RenameBlock::Date, i18n("Date")},
                                                           {RenameBlock::FindReplace, i18n("Find & replace")},
                                                           {RenameBlock::Case, i18n("Case")}};
    for (const auto &k : kinds) {
        const auto kind = k.first;
        menu->addAction(k.second, this, [this, kind] {
            RenameBlock b;
            b.kind = kind;
            if (kind == RenameBlock::Date)
                b.text = QStringLiteral("yyyy-MM-dd");
            addBlock(b);
            m_list->setCurrentRow(m_list->count() - 1);
            refresh();
        });
    }
    add->setMenu(menu);
    row->addWidget(add);
    auto *del = new QToolButton;
    del->setObjectName(QStringLiteral("removeBlockButton"));
    del->setText(QStringLiteral("✕"));
    connect(del, &QToolButton::clicked, this, [this] {
        delete m_list->takeItem(m_list->currentRow());
        refresh();
    });
    row->addWidget(del);
    lay->addWidget(m_blocksRow);

    // one editor, fields shown per the selected block's kind
    m_editor = new QWidget;
    auto *er = new QHBoxLayout(m_editor);
    er->setContentsMargins(0, 0, 0, 0);
    auto spin = [&](const QString &name, int lo, int hi) {
        auto *s = new QSpinBox;
        s->setObjectName(name);
        s->setRange(lo, hi);
        er->addWidget(s);
        connect(s, &QSpinBox::valueChanged, this, &RenameManyDialog::editorChanged);
        return s;
    };
    m_digits = spin(QStringLiteral("digitsSpin"), 1, 6);
    m_start = spin(QStringLiteral("startSpin"), 0, 999999);
    m_step = spin(QStringLiteral("stepSpin"), 1, 9999);
    m_digits->setPrefix(i18n("digits "));
    m_start->setPrefix(i18n("from "));
    m_step->setPrefix(i18n("step "));
    auto edit = [&](const QString &name) {
        auto *e = new QLineEdit;
        e->setObjectName(name);
        er->addWidget(e, 1);
        connect(e, &QLineEdit::textChanged, this, &RenameManyDialog::editorChanged);
        return e;
    };
    m_text = edit(QStringLiteral("textEdit")); // Text, Date format, or Find
    m_replace = edit(QStringLiteral("replaceEdit"));
    m_replace->setPlaceholderText(i18n("replace with"));
    m_case = new QComboBox;
    m_case->setObjectName(QStringLiteral("caseCombo"));
    m_case->addItems({i18n("UPPER"), i18n("lower"), i18n("Title")});
    er->addWidget(m_case);
    connect(m_case, &QComboBox::currentIndexChanged, this, &RenameManyDialog::editorChanged);
    lay->addWidget(m_editor);
    connect(m_list, &QListWidget::currentRowChanged, this, &RenameManyDialog::showEditor);
    connect(m_list->model(), &QAbstractItemModel::rowsMoved, this, &RenameManyDialog::refresh);

    lay->addWidget(new QLabel(i18n("Order: as shown in the view")));
    m_tree = new QTreeWidget;
    m_tree->setObjectName(QStringLiteral("previewTree"));
    m_tree->setRootIsDecorated(false);
    m_tree->setHeaderLabels({i18n("Now"), i18n("After"), i18n("Why")});
    lay->addWidget(m_tree, 1);

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    m_buttons->setObjectName(QStringLiteral("buttonBox"));
    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    lay->addWidget(m_buttons);

    RenameBlock n, t, c;
    n.kind = RenameBlock::Number;
    t.text = QStringLiteral(" - ");
    c.kind = RenameBlock::CurrentName;
    setBlocks({n, t, c});
}

void RenameManyDialog::addBlock(const RenameBlock &b)
{
    auto *it = new QListWidgetItem(summary(b), m_list);
    it->setData(Qt::UserRole, QVariant::fromValue(b));
}

void RenameManyDialog::setBlocks(const QList<RenameBlock> &bl)
{
    m_list->clear();
    for (const auto &b : bl)
        addBlock(b);
    refresh();
    showEditor();
}

QList<RenameBlock> RenameManyDialog::blocks() const
{
    QList<RenameBlock> l;
    for (int i = 0; i < m_list->count(); ++i)
        l << blockOf(m_list->item(i));
    return l;
}

QList<PlannedRename> RenameManyDialog::plan() const
{
    if (m_mode == Mode::Blocks)
        return RenamePlan::plan(blocks(), m_items, m_existing);
    QList<QString> names;
    const bool ready = !m_running && m_fxValid && m_values.size() == m_items.size();
    for (const FxValue &v : m_values)
        names << (v.isError() ? QString() : v.toText());
    if (!ready) { // nothing to rename until the run is done
        QList<PlannedRename> out;
        for (const RenameItem &it : m_items)
            out.append({it.url, it.name, it.name, PlannedRename::Unchanged, {}});
        return out;
    }
    QList<PlannedRename> out = RenamePlan::planNames(names, m_items, m_existing);
    for (int i = 0; i < out.size(); ++i)
        if (m_values[i].isError()) {
            out[i].status = PlannedRename::Invalid;
            out[i].note = m_values[i].toText();
        }
    return out;
}

QUrl RenameManyDialog::base() const
{
    // ponytail: the common ancestor folder of all items (the first item's folder when they share one), so ROWINFOLDER counts per folder
    if (m_items.isEmpty())
        return QUrl();
    QUrl b = m_items.first().url.adjusted(QUrl::RemoveFilename);
    for (const RenameItem &it : m_items)
        while (!it.url.path().startsWith(b.path()) && b.path().size() > 1)
            b = b.adjusted(QUrl::StripTrailingSlash | QUrl::RemoveFilename);
    return b;
}

QString RenameManyDialog::formula() const
{
    return m_fx->text();
}

void RenameManyDialog::setFormula(const QString &t)
{
    if (m_mode != Mode::Formula)
        setMode(Mode::Formula);
    const bool same = m_fx->text() == t;
    m_fx->setText(t); // a change marks the plan stale and the debounce starts the run
    if (same)
        formulaChanged(m_fx->expr().has_value()); // no change signal will come
    refresh();
}

void RenameManyDialog::setMode(Mode m)
{
    if (m == m_mode)
        return;
    m_mode = m;
    const bool f = m == Mode::Formula;
    m_modeBlocks->setChecked(!f);
    m_modeFormula->setChecked(f);
    m_fx->setVisible(f);
    m_blocksRow->setVisible(!f);
    if (f) {
        m_editor->hide();
        if (!m_fx->text().isEmpty())
            formulaChanged(m_fx->expr().has_value());
    } else {
        m_run.stop();
        m_running = false;
        showEditor();
    }
    refresh();
}

void RenameManyDialog::formulaChanged(bool valid)
{
    m_run.stop();
    m_values.clear();
    m_runError.clear();
    m_fxValid = valid;
    m_running = false;
    const auto expr = valid ? m_fx->expr() : std::nullopt;
    if (expr) {
        m_running = true;
        m_run.startOn(m_fxItems, base(), *expr);
    }
    refresh();
}

void RenameManyDialog::showEditor()
{
    auto *it = m_list->currentItem();
    m_editor->setVisible(it && m_mode == Mode::Blocks);
    if (!it)
        return;
    const RenameBlock b = blockOf(it);
    m_loading = true;
    m_digits->setValue(b.digits);
    m_start->setValue(b.start);
    m_step->setValue(b.step);
    m_text->setText(b.text);
    m_replace->setText(b.replace);
    m_case->setCurrentIndex(b.caseMode);
    m_loading = false;
    const bool num = b.kind == RenameBlock::Number;
    for (auto *w : {m_digits, m_start, m_step})
        w->setVisible(num);
    m_text->setVisible(b.kind == RenameBlock::Text || b.kind == RenameBlock::Date || b.kind == RenameBlock::FindReplace);
    m_text->setPlaceholderText(b.kind == RenameBlock::Date ? i18n("format") : b.kind == RenameBlock::FindReplace ? i18n("find") : i18n("text"));
    m_replace->setVisible(b.kind == RenameBlock::FindReplace);
    m_case->setVisible(b.kind == RenameBlock::Case);
}

void RenameManyDialog::editorChanged()
{
    auto *it = m_list->currentItem();
    if (m_loading || !it)
        return;
    RenameBlock b = blockOf(it);
    b.digits = m_digits->value();
    b.start = m_start->value();
    b.step = m_step->value();
    b.text = m_text->text();
    b.replace = m_replace->text();
    b.caseMode = RenameBlock::CaseMode(m_case->currentIndex());
    it->setData(Qt::UserRole, QVariant::fromValue(b));
    it->setText(summary(b));
    refresh();
}

void RenameManyDialog::refresh()
{
    const auto p = plan();
    m_tree->clear();
    int n = 0;
    for (const auto &r : p) {
        auto *row = new QTreeWidgetItem(m_tree, {r.oldName, m_mode == Mode::Formula && m_running ? QStringLiteral("…") : r.newName, r.note.isEmpty() ? m_runError : r.note});
        QString msg;
        switch (r.status) {
        case PlannedRename::Rename:
            ++n;
            break;
        case PlannedRename::Unchanged:
        case PlannedRename::Invalid:
            row->setForeground(0, Qt::gray);
            row->setForeground(1, Qt::gray);
            break;
        case PlannedRename::ClashExisting:
            msg = i18n("%1 — a file with this name is already in the folder → skipped", r.newName);
            break;
        case PlannedRename::ClashPlanned:
            msg = i18n("%1 — two items would get this name → skipped", r.newName);
            break;
        }
        if (!msg.isEmpty()) {
            row->setText(1, msg);
            row->setForeground(0, Qt::red);
            row->setForeground(1, Qt::red);
        }
    }
    auto *ok = m_buttons->button(QDialogButtonBox::Ok);
    ok->setText(i18n("Rename %1", n));
    ok->setEnabled(n > 0 && !m_running);
}

void RenameManyDialog::keyPressEvent(QKeyEvent *e)
{
    // Enter while typing in a block editor must never start the rename
    if ((e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) && !qobject_cast<QPushButton *>(focusWidget())) {
        e->accept();
        return;
    }
    QDialog::keyPressEvent(e);
}

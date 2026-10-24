#include "selectbyruledialog.h"
#include "formulaedit.h"
#include "fxfile.h"
#include <KIO/Global>
#include <KMessageBox>
#include <QInputDialog>
#include <KLocalizedString>
#include <KUrlRequester>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QKeyEvent>
#include <QPushButton>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

Q_DECLARE_METATYPE(KFileItemList)

static const QList<Field> kFields = {Field::Name, Field::Extension, Field::Size, Field::Modified, Field::Created, Field::Kind, Field::Tag};

SelectByRuleDialog::SelectByRuleDialog(const QUrl &lookIn, Action defaultAction, QWidget *parent)
    : QDialog(parent)
    , m_lookIn(lookIn)
    , m_chosen(defaultAction)
{
    qRegisterMetaType<KFileItemList>();
    setWindowTitle(i18n("Select by rule"));
    resize(640, 520);
    auto *top = new QVBoxLayout(this);

    auto *bar = new QHBoxLayout;
    bar->addWidget(new QLabel(i18n("Look in:")));
    m_url = new KUrlRequester(lookIn, this);
    m_url->setMode(KFile::Directory);
    bar->addWidget(m_url, 1);
    m_recursive = new QCheckBox(i18n("Include subfolders"));
    bar->addWidget(m_recursive);
    top->addLayout(bar);

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
    m_modeRules = mkMode(i18n("Simple rules"), QStringLiteral("modeRules"));
    m_modeFormula = mkMode(i18n("fx Formula"), QStringLiteral("modeFormula"));
    m_modeRules->setChecked(true);
    modes->addStretch();
    top->addLayout(modes);
    m_fx = new FormulaEdit;
    m_fx->hide();
    top->addWidget(m_fx);
    m_rowsLayout = new QVBoxLayout;
    top->addLayout(m_rowsLayout);
    auto *addRow_ = m_addRule = new QToolButton;
    addRow_->setObjectName(QStringLiteral("addRule"));
    addRow_->setText(i18n("+ Add rule"));
    addRow_->setAutoRaise(true);
    m_find = new QPushButton(i18n("Find"));
    m_find->setObjectName(QStringLiteral("findButton"));
    m_find->setAutoDefault(false);
    auto *ar = new QHBoxLayout;
    ar->addWidget(addRow_);
    ar->addStretch();
    ar->addWidget(m_find);
    top->addLayout(ar);
    m_error = new QLabel;
    m_error->setObjectName(QStringLiteral("errorLabel"));
    m_error->setStyleSheet(QStringLiteral("color: red"));
    top->addWidget(m_error);

    m_tree = new QTreeWidget;
    m_tree->setObjectName(QStringLiteral("results"));
    m_tree->setRootIsDecorated(false);
    m_tree->setSortingEnabled(true);
    m_tree->setHeaderLabels({i18n("Name"), i18n("Modified"), i18n("Size")});
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    top->addWidget(m_tree, 1);
    m_count = new QLabel;
    m_count->setObjectName(QStringLiteral("countLabel"));
    top->addWidget(m_count);
    m_keep = new QCheckBox(i18n("Keep subfolder structure"));
    top->addWidget(m_keep);

    auto *btns = new QHBoxLayout;
    auto mk = [&](Action a, const QString &text, const QString &name) {
        auto *b = new QPushButton(text);
        b->setObjectName(name);
        b->setAutoDefault(false);
        b->setDefault(a == defaultAction);
        m_buttons[a] = b;
        connect(b, &QPushButton::clicked, this, [this, a] { act(a); });
        btns->addWidget(b);
    };
    mk(Action::Select, i18n("Select"), QStringLiteral("btnSelect"));
    mk(Action::CopyTo, i18n("Copy to…"), QStringLiteral("btnCopyTo"));
    mk(Action::MoveTo, i18n("Move to…"), QStringLiteral("btnMoveTo"));
    mk(Action::Rename, i18n("Rename…"), QStringLiteral("btnRename"));
    mk(Action::Trash, i18n("Move to Trash"), QStringLiteral("btnTrash"));
    auto *more = new QToolButton;
    more->setText(QStringLiteral("⋯"));
    more->setPopupMode(QToolButton::InstantPopup);
    auto *menu = new QMenu(more);
    auto addAct = [&](Action a, const QString &text, const QString &name) {
        QAction *qa = menu->addAction(text);
        qa->setObjectName(name);
        connect(qa, &QAction::triggered, this, [this, a] { act(a); });
        return qa;
    };
    m_menuActs << addAct(Action::DeleteForever, i18n("Delete forever"), QStringLiteral("actDeleteForever"))
               << addAct(Action::Zip, i18n("Compress to zip…"), QStringLiteral("actZip"))
               << addAct(Action::CopyPaths, i18n("Copy paths"), QStringLiteral("actCopyPaths"))
               << addAct(Action::AddTag, i18n("Add tag…"), QStringLiteral("actAddTag"));
    more->setMenu(menu);
    btns->addWidget(more);
    btns->addStretch();
    top->addLayout(btns);

    connect(addRow_, &QToolButton::clicked, this, [this] {
        addRow({Field::Name, Test::Contains, QString()});
        recompile();
    });
    connect(m_find, &QPushButton::clicked, this, &SelectByRuleDialog::find);
    connect(m_modeRules, &QToolButton::clicked, this, [this] { setMode(Mode::Rules); });
    connect(m_modeFormula, &QToolButton::clicked, this, [this] { setMode(Mode::Formula); });
    connect(m_fx, &FormulaEdit::formulaChanged, this, [this] { recompile(); });
    connect(m_fx, &FormulaEdit::submitted, this, &SelectByRuleDialog::find);
    connect(m_fx, &FormulaEdit::saveRequested, this, [this](const QString &text) {
        if (!m_library)
            return;
        saveRuleInteractively(*m_library, SavedRule::Kind::Select, text, this);
    });
    connect(&m_run, &FormulaRun::results, this, [this](const QList<FxRowResult> &rows) {
        const QSignalBlocker block(m_tree);
        const QString base = QDir::cleanPath(this->lookIn().toLocalFile());
        const bool sorting = m_tree->isSortingEnabled();
        m_tree->setSortingEnabled(false);
        for (const FxRowResult &r : rows) {
            auto *row = new QTreeWidgetItem(m_tree);
            const QString rel = QDir(base).relativeFilePath(r.item.url().toLocalFile());
            row->setText(0, rel);
            row->setIcon(0, QIcon::fromTheme(r.item.iconName()));
            row->setText(1, r.row ? r.row->ext() : QString());
            row->setText(2, r.item.isDir() ? QString() : KIO::convertSize(r.item.size()));
            row->setText(3, QLocale().toString(r.item.time(KFileItem::ModificationTime), QLocale::ShortFormat));
            QStringList tags;
            if (r.row)
                for (const Tag &t : r.row->tags())
                    tags << t.display();
            row->setText(4, tags.join(QStringLiteral(", ")));
            row->setText(5, r.value.display());
            if (r.value.isError())
                row->setForeground(5, Qt::red);
            const bool isTrue = r.value.type == FxValue::Type::Bool && r.value.b;
            row->setCheckState(0, isTrue ? Qt::Checked : Qt::Unchecked);
            row->setData(0, Qt::UserRole, QVariant::fromValue(r.item));
        }
        m_tree->setSortingEnabled(sorting);
        updateState();
    });
    connect(&m_run, &FormulaRun::finished, this, [this](bool ok, const QString &err) {
        if (!ok)
            m_error->setText(err);
        Q_EMIT searchFinished();
    });
    connect(m_recursive, &QCheckBox::toggled, this, [this](bool on) {
        m_keep->setEnabled(on);
        updateState();
    });
    connect(m_tree, &QTreeWidget::itemChanged, this, &SelectByRuleDialog::updateState);
    connect(&m_search, &RuleSearch::found, this, [this](const KFileItemList &items) {
        const QSignalBlocker block(m_tree);
        const QString base = QDir::cleanPath(this->lookIn().toLocalFile());
        for (const KFileItem &it : items) {
            auto *row = new QTreeWidgetItem(m_tree);
            const QString rel = QDir(base).relativeFilePath(it.url().toLocalFile());
            row->setText(0, rel);
            row->setIcon(0, QIcon::fromTheme(it.iconName()));
            row->setCheckState(0, Qt::Checked);
            row->setText(1, QLocale().toString(it.time(KFileItem::ModificationTime), QLocale::ShortFormat));
            row->setText(2, it.isDir() ? QString() : KIO::convertSize(it.size()));
            row->setData(0, Qt::UserRole, QVariant::fromValue(it));
        }
        updateState();
    });
    connect(&m_search, &RuleSearch::finished, this, [this](bool ok, const QString &err) {
        if (!ok)
            m_error->setText(err);
        Q_EMIT searchFinished();
    });

    m_keep->setEnabled(false);
    addRow({Field::Name, Test::StartsWith, QString()});
    recompile();
    updateState();
}

SelectByRuleDialog::~SelectByRuleDialog()
{
    m_search.stop();
    m_run.stop();
}

void SelectByRuleDialog::keyPressEvent(QKeyEvent *e)
{
    // Enter in the look-in or rule fields means "Find", never an action (nothing changes before the list is shown)
    if ((e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) && !qobject_cast<QPushButton *>(focusWidget())
        && !(focusWidget() && m_tree->isAncestorOf(focusWidget()))) {
        find();
        e->accept();
        return;
    }
    QDialog::keyPressEvent(e);
}

void SelectByRuleDialog::reject()
{
    m_search.stop();
    m_run.stop();
    QDialog::reject();
}

void SelectByRuleDialog::fillTests(RuleRow &r)
{
    const Field f = kFields.at(r.field->currentIndex());
    const QSignalBlocker b(r.test);
    r.test->clear();
    for (Test t : testsFor(f))
        r.test->addItem(testText(t), int(t));
    r.kind->setVisible(f == Field::Kind);
    r.value->setVisible(f != Field::Kind);
    r.test->setVisible(f != Field::Kind);
}

void SelectByRuleDialog::addRow(const Rule &rule)
{
    RuleRow r;
    r.widget = new QWidget;
    auto *l = new QHBoxLayout(r.widget);
    l->setContentsMargins(0, 0, 0, 0);
    r.field = new QComboBox;
    for (Field f : kFields)
        r.field->addItem(fieldText(f));
    r.test = new QComboBox;
    r.value = new QLineEdit;
    r.kind = new QComboBox;
    r.kind->addItem(i18n("folder"), QStringLiteral("folder"));
    r.kind->addItem(i18n("file"), QStringLiteral("file"));
    r.remove = new QToolButton;
    r.remove->setObjectName(QStringLiteral("removeRule"));
    r.remove->setText(QStringLiteral("✕"));
    r.remove->setAutoRaise(true);
    for (QWidget *w : {static_cast<QWidget *>(r.field), static_cast<QWidget *>(r.test), static_cast<QWidget *>(r.value), static_cast<QWidget *>(r.kind)})
        l->addWidget(w, w == r.value ? 1 : 0);
    l->addWidget(r.remove);
    m_rowsLayout->addWidget(r.widget);

    r.field->setCurrentIndex(kFields.indexOf(rule.field));
    fillTests(r);
    r.test->setCurrentIndex(qMax(0, r.test->findData(int(rule.test))));
    r.value->setText(rule.value);
    if (rule.field == Field::Kind)
        r.kind->setCurrentIndex(rule.value.compare(QLatin1String("file"), Qt::CaseInsensitive) == 0 ? 1 : 0);
    m_rows << r;

    QWidget *w = r.widget;
    auto rowOf = [this, w]() -> RuleRow * {
        for (RuleRow &x : m_rows)
            if (x.widget == w)
                return &x;
        return nullptr;
    };
    connect(r.field, &QComboBox::currentIndexChanged, this, [this, rowOf] {
        if (RuleRow *x = rowOf())
            fillTests(*x);
        recompile();
    });
    connect(r.test, &QComboBox::currentIndexChanged, this, &SelectByRuleDialog::recompile);
    connect(r.kind, &QComboBox::currentIndexChanged, this, &SelectByRuleDialog::recompile);
    connect(r.value, &QLineEdit::textChanged, this, &SelectByRuleDialog::recompile);
    connect(r.remove, &QToolButton::clicked, this, [this, w] {
        for (int i = 0; i < m_rows.size(); ++i)
            if (m_rows[i].widget == w)
                m_rows.removeAt(i);
        w->setParent(nullptr);
        w->deleteLater();
        recompile();
    });
}

void SelectByRuleDialog::setRules(const QList<Rule> &rules)
{
    for (const RuleRow &r : std::as_const(m_rows)) {
        r.widget->setParent(nullptr);
        r.widget->deleteLater();
    }
    m_rows.clear();
    for (const Rule &r : rules)
        addRow(r);
    if (m_mode == Mode::Formula)
        for (const RuleRow &r : std::as_const(m_rows))
            r.widget->hide();
    recompile();
}

QList<Rule> SelectByRuleDialog::rules() const
{
    QList<Rule> out;
    for (const RuleRow &r : m_rows) {
        const Field f = kFields.at(r.field->currentIndex());
        const int t = r.test->currentData().toInt();
        out.append({f, Test(t), f == Field::Kind ? r.kind->currentData().toString() : r.value->text()});
    }
    return out;
}

void SelectByRuleDialog::setRecursive(bool on)
{
    m_recursive->setChecked(on);
}

void SelectByRuleDialog::setMode(Mode m)
{
    m_mode = m;
    const bool fx = m == Mode::Formula;
    m_modeRules->setChecked(!fx);
    m_modeFormula->setChecked(fx);
    m_fx->setVisible(fx);
    m_addRule->setVisible(!fx);
    for (const RuleRow &r : std::as_const(m_rows))
        r.widget->setVisible(!fx);
    m_search.stop();
    m_run.stop();
    m_tree->clear();
    m_tree->setHeaderLabels(fx ? QStringList{i18n("Name"), i18n("Ext"), i18n("Size"), i18n("Modified"), i18n("Tags"), i18n("fx result")}
                               : QStringList{i18n("Name"), i18n("Modified"), i18n("Size")});
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    recompile();
    updateState();
}

SelectByRuleDialog::Mode SelectByRuleDialog::mode() const
{
    return m_mode;
}

void SelectByRuleDialog::setFormula(const QString &t)
{
    m_fx->setText(t);
    recompile(); // don't wait for the debounce: Find must reflect it now
}

QString SelectByRuleDialog::formula() const
{
    return m_fx->text();
}

void SelectByRuleDialog::setLibrary(RuleLibrary *l)
{
    m_library = l;
}

void SelectByRuleDialog::recompile()
{
    if (m_mode == Mode::Formula) {
        m_find->setEnabled(m_fx->expr().has_value());
        m_error->clear(); // the FormulaEdit shows its own error
        return;
    }
    QString err;
    const bool ok = RuleMatcher::compile(rules(), &err).has_value();
    m_error->setText(ok ? QString() : err);
    m_find->setEnabled(ok);
}

QUrl SelectByRuleDialog::lookIn() const
{
    return m_url->url().isEmpty() ? m_lookIn : m_url->url();
}

void SelectByRuleDialog::find()
{
    if (m_mode == Mode::Formula) {
        m_search.stop();
        m_tree->clear();
        updateState();
        if (auto e = m_fx->expr())
            m_run.start(lookIn(), m_recursive->isChecked(), *e);
        else
            Q_EMIT searchFinished();
        return;
    }
    QString err;
    auto m = RuleMatcher::compile(rules(), &err);
    if (!m) {
        m_search.stop();
        m_tree->clear();
        updateState();
        m_error->setText(err);
        Q_EMIT searchFinished();
        return;
    }
    m_tree->clear();
    updateState();
    m_search.start(lookIn(), m_recursive->isChecked(), *m);
}

KFileItemList SelectByRuleDialog::ticked() const
{
    KFileItemList out;
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *it = m_tree->topLevelItem(i);
        if (it->checkState(0) == Qt::Checked)
            out << it->data(0, Qt::UserRole).value<KFileItem>();
    }
    return out;
}

bool SelectByRuleDialog::keepStructure() const
{
    return m_keep->isEnabled() && m_keep->isChecked();
}

SelectByRuleDialog::Action SelectByRuleDialog::chosenAction() const
{
    return m_chosen;
}

void SelectByRuleDialog::updateState()
{
    const KFileItemList t = ticked();
    qint64 total = 0;
    bool allInLookIn = true;
    const QUrl base = lookIn().adjusted(QUrl::StripTrailingSlash);
    for (const KFileItem &i : t) {
        if (!i.isDir())
            total += i.size();
        if (i.url().adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash) != base)
            allInLookIn = false;
    }
    if (m_mode == Mode::Formula)
        m_count->setText(i18np("%1 row is TRUE", "%1 rows are TRUE", t.size()));
    else
        m_count->setText(i18n("Found: %1 items · %2", t.size(), KIO::convertSize(total)));
    for (QPushButton *b : std::as_const(m_buttons))
        b->setEnabled(!t.isEmpty());
    for (QAction *a : std::as_const(m_menuActs))
        a->setEnabled(!t.isEmpty());
    QPushButton *sel = m_buttons[Action::Select];
    sel->setEnabled(!t.isEmpty() && allInLookIn);
    sel->setToolTip(allInLookIn ? QString() : i18n("Select works on items of the look-in folder; the other actions work on all matches"));
}

void SelectByRuleDialog::act(Action a)
{
    m_chosen = a;
    accept();
}

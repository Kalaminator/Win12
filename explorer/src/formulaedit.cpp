#include "formulaedit.h"
#include <KLocalizedString>
#include <QDialog>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSet>
#include <QTextBrowser>
#include <QTimer>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace
{
// Excel names are not translated; the groups follow the mockup footer.
}

QString fxFunctionGroup(const QString &fn)
{
    static const QSet<QString> text{u"LEFT"_s, u"RIGHT"_s, u"MID"_s, u"LEN"_s, u"FIND"_s, u"SUBSTITUTE"_s, u"UPPER"_s, u"LOWER"_s,
                                    u"PROPER"_s, u"TRIM"_s, u"TEXT"_s, u"CONTAINS"_s, u"STARTSWITH"_s, u"ENDSWITH"_s, u"REGEX"_s};
    static const QSet<QString> logic{u"IF"_s, u"IFS"_s, u"AND"_s, u"OR"_s, u"NOT"_s};
    static const QSet<QString> date{u"TODAY"_s, u"DAYS"_s, u"YEAR"_s, u"MONTH"_s, u"DAY"_s, u"DATE"_s, u"DATEVALUE"_s};
    static const QSet<QString> files{u"HASTAG"_s, u"COUNTTAGS"_s, u"ROW"_s, u"ROWINFOLDER"_s, u"COUNTIN"_s, u"FILES"_s};
    return text.contains(fn) ? i18n("Text") : logic.contains(fn) ? i18n("Logic") : date.contains(fn) ? i18n("Date") : files.contains(fn) ? i18n("Files") : QString();
}

namespace
{
class FxHelpDialog : public QDialog
{
public:
    explicit FxHelpDialog(QWidget *parent)
        : QDialog(parent)
    {
        setWindowTitle(i18n("Functions"));
        resize(520, 460);
        auto *l = new QVBoxLayout(this);
        auto *b = new QTextBrowser;
        b->setObjectName(QStringLiteral("fxHelpText"));
        QString h = u"<p><b>%1</b> "_s.arg(i18n("Columns:")) + FxColumns::names().join(u", "_s).toHtmlEscaped() + u"</p>"_s;
        for (const QString &g : {i18n("Text"), i18n("Logic"), i18n("Date"), i18n("Files")}) {
            h += u"<h4>"_s + g.toHtmlEscaped() + u"</h4>"_s;
            for (const FxFunctionSpec &f : FxFunctions::all())
                if (fxFunctionGroup(f.name) == g)
                    h += u"<p><code>"_s + f.signature.toHtmlEscaped() + u"</code><br>"_s + f.help.toHtmlEscaped() + u"</p>"_s;
        }
        b->setHtml(h);
        l->addWidget(b);
    }
};
}

FormulaEdit::FormulaEdit(QWidget *parent)
    : QWidget(parent)
    , m_timer(new QTimer(this))
{
    auto *top = new QVBoxLayout(this);
    top->setContentsMargins(0, 0, 0, 0);
    auto *row = new QHBoxLayout;
    auto *fx = new QLabel(QStringLiteral("fx"));
    fx->setStyleSheet(QStringLiteral("font-style: italic; font-weight: bold"));
    m_edit = new QLineEdit;
    m_edit->setObjectName(QStringLiteral("formulaEdit"));
    m_edit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_edit->setPlaceholderText(i18n("e.g. AND(EXT=\"jpg\", SIZE>1000000)"));
    auto *help = new QPushButton(i18n("Functions…"));
    help->setObjectName(QStringLiteral("fxHelp"));
    help->setAutoDefault(false);
    auto *save = new QPushButton(i18n("Save as…"));
    save->setObjectName(QStringLiteral("fxSave"));
    save->setAutoDefault(false);
    row->addWidget(fx);
    row->addWidget(m_edit, 1);
    row->addWidget(help);
    row->addWidget(save);
    top->addLayout(row);
    m_error = new QLabel;
    m_error->setObjectName(QStringLiteral("fxError"));
    m_error->setStyleSheet(QStringLiteral("color: red"));
    top->addWidget(m_error);

    m_timer->setSingleShot(true);
    m_timer->setInterval(300);
    connect(m_edit, &QLineEdit::textChanged, m_timer, qOverload<>(&QTimer::start));
    connect(m_timer, &QTimer::timeout, this, [this] { Q_EMIT formulaChanged(validate()); });
    connect(m_edit, &QLineEdit::returnPressed, this, &FormulaEdit::submitted);
    connect(help, &QPushButton::clicked, this, [this] { FxHelpDialog(this).exec(); });
    connect(save, &QPushButton::clicked, this, [this] {
        if (validate())
            Q_EMIT saveRequested(text());
    });
}

void FormulaEdit::setText(const QString &t)
{
    m_edit->setText(t);
}

QString FormulaEdit::text() const
{
    return m_edit->text();
}

bool FormulaEdit::validate() const
{
    return expr().has_value();
}

std::optional<FxExpr> FormulaEdit::expr() const
{
    if (m_edit->text().trimmed().isEmpty()) {
        m_error->clear();
        return std::nullopt;
    }
    FxParseError err;
    auto e = FxParser::parseFormula(m_edit->text(), &err);
    m_error->setText(e ? QString() : err.message);
    return e;
}

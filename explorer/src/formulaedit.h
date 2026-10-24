#pragma once
#include "fxparser.h"
#include <QWidget>
#include <optional>

class QLabel;
class QLineEdit;
class QTimer;

// "fx" + mono line edit + Functions… + Save as…, with a red parse-error label below.
class FormulaEdit : public QWidget
{
    Q_OBJECT
public:
    explicit FormulaEdit(QWidget *parent = nullptr);
    void setText(const QString &);
    QString text() const;
    std::optional<FxExpr> expr() const; // parsed now; nullopt + error label when invalid or empty

Q_SIGNALS:
    void formulaChanged(bool valid); // 300 ms after the last edit
    void saveRequested(const QString &text); // only when valid
    void submitted(); // Enter

private:
    bool validate() const;
    QLineEdit *m_edit;
    QLabel *m_error;
    QTimer *m_timer;
};

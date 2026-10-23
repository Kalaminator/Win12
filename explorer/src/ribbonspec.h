#pragma once
#include <QList>
#include <QString>
#include <QStringList>

namespace Ribbon {
constexpr int currentPhase = 1;
enum class Size { Big, Small };
struct Button {
    QString action;
    QString text;
    QString icon;
    Size size = Size::Big;
    bool menu = false;
};
struct Group {
    QString text;
    QList<Button> buttons;
    int phase = 1;
};
struct Tab {
    QString id;
    QString text;
    QList<Group> groups;
    bool isFileMenu = false;
    int phase = 1;
};
const QList<Tab> &spec();
QStringList actionIds();                        // every Button::action in table order
const Button *button(const QString &actionId); // nullptr when unknown
}

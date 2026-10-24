#pragma once
#include "tag.h"
#include <QList>
#include <QString>
#include <QUrl>

// One planned change of a script's dry run. Nothing here touches files; "Run plan" does that later.
struct PlanStep {
    enum Kind { Move, Copy, Rename, Tag, Untag, Trash };
    Kind kind = Trash;
    QUrl source;
    QUrl destFolder; // Move/Copy
    QString newName; // Rename
    ::Tag tag; // Tag/Untag
    int line = 0;
    bool skipped = false;
    QString note; // why it is skipped: "Name exists", "Same name planned above", "Already has 3 tags", ...
    QString baseFolder; // the script's base folder: actionText() shows destinations inside it relative

    QString actionText() const; // "Move → Invoices/2026", "Copy → …", "Rename → new.txt", "Tag Orange/Invoice", "Untag Invoice", "Trash"
    QString fileText() const; // source file name
};

struct ScriptResult {
    bool ok = true;
    QString error;
    int line = 0;
    QList<PlanStep> plan; // kept on errors: what was built so far
    qint64 statements = 0;
};

#include "sqlineedit.h"

#include <QThread>
#include <QTimer>

using namespace sodium;

SQLineEdit::SQLineEdit(QWidget *parent)
    : SQEditBase<QLineEdit>(parent)
{
    connect(this, &QLineEdit::textChanged, [this](const QString &t) { m_sUserChanges.send(t); });
}

#include <sqlabel.h>

using namespace sodium;

SQLabel::SQLabel(QWidget *parent)
    : SQWidgetBase(parent)
    , m_text(QString())
{
}

void SQLabel::text(const sodium::cell<QString> &text)
{
    m_text = text;
    m_unsubscribe += text.listen(ensureSameThread<QString>(this, &QLabel::setText));
}

const sodium::cell<QString> &SQLabel::text() const
{
    return m_text;
}

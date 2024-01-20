#include <sqlabel.h>

using namespace sodium;

SQLabel::SQLabel(QWidget *parent)
    : SQLabel(QString(), parent)
{}

SQLabel::SQLabel(const sodium::cell<QString> &text, QWidget *parent)
    : SQWidgetBase(parent)
    , m_text(QString())
{
    setText(text);
}

void SQLabel::setText(const sodium::cell<QString> &text)
{
    m_text = text;
    m_unsubscribe.insert_or_assign("text",
                                   text.listen(ensureSameThread<QString>(this, &QLabel::setText)));
}

const sodium::cell<QString> &SQLabel::text() const
{
    return m_text;
}

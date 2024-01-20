#pragma once

#include "sqwidgetbase.h"

#include <QString>
#include <QThread>
#include <QTimer>

#include <sodium/sodium.h>

template<class Base>
class SQEditBase : public SQWidgetBase<Base>
{
public:
    explicit SQEditBase(QWidget *parent = nullptr);
    virtual ~SQEditBase() {}

    void setText(const sodium::stream<QString> &sText, const QString &initialText = {});
    const sodium::cell<QString> &text() const;

protected:
    UserValue<QString> m_text;

private:
    void setTextInternal(const QString &text);
};

// ---------------------- IMPLEMENTATION ----------------------

template<class Base>
SQEditBase<Base>::SQEditBase(QWidget *parent)
    : SQWidgetBase<Base>(parent)
    , m_text({}, this, [this](const QString &v) { setTextInternal(v); })
{
}

template<class Base>
void SQEditBase<Base>::setText(const sodium::stream<QString> &sText, const QString &initialText)
{
    m_text.setValue(sText, initialText);
}

template<class Base>
const sodium::cell<QString> &SQEditBase<Base>::text() const
{
    return m_text.value();
}

template<class Base>
void SQEditBase<Base>::setTextInternal(const QString &text)
{
    // Setting the text changes the cursor position, so don't do it if text wouldn't change,
    // which is for example the case for user changes
    if (text != Base::text())
        Base::setText(text);
}

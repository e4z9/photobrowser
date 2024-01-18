#pragma once

#include <sqeditbase.h>

#include <QLineEdit>

#include <sodium/sodium.h>

class SQLineEdit : public SQEditBase<QLineEdit>
{
public:
    explicit SQLineEdit(QWidget *parent = nullptr);
};

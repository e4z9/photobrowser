#pragma once

#include "tools.h"

#include <QListView>

#include <sodium/sodium.h>

class SQListView : public QListView
{
public:
    SQListView(const sodium::stream<boost::optional<int>> &sCurrentIndex);

    void setModel(QAbstractItemModel *m) override;

    const sodium::cell<boost::optional<int>> &cCurrentIndex() const;

protected:
    void currentChanged(const QModelIndex &current, const QModelIndex &previous) override;

private:
    void checkUpdateCurrent();
    void updateCurrent(const QModelIndex &current);

    sodium::cell_sink<boost::optional<int>> m_currentIndex;
    Unsubscribe m_unsubscribe;
};

#pragma once

#include "sqwidgetbase.h"

#include <QListView>

#include <sodium/sodium.h>

class SQListView : public SQWidgetBase<QListView>
{
public:
    SQListView(const sodium::stream<boost::optional<int>> &sCurrentIndex);

    void setModel(QAbstractItemModel *m) override;

    const sodium::cell<boost::optional<int>> &cCurrentIndex() const;
    const sodium::cell<int> &count() const;

protected:
    void currentChanged(const QModelIndex &current, const QModelIndex &previous) override;

private:
    void updateCountAndCurrent();
    void updateCurrent();

    sodium::cell<boost::optional<int>> m_currentIndex;
    sodium::stream_sink<boost::optional<int>> m_sUserCurrentIndex;
    sodium::cell_sink<int> m_count;
    bool blockChange = false;
};

#include "sqlistview.h"

using namespace sodium;

SQListView::SQListView(const sodium::stream<boost::optional<int>> &sCurrentIndex)
    : m_currentIndex(boost::none)
{
    m_unsubscribe += sCurrentIndex.listen(
        post<boost::optional<int>>(this, [this](boost::optional<int> i) {
            if (!model())
                return;
            if (i)
                setCurrentIndex(model()->index(*i, 0));
            else
                setCurrentIndex(QModelIndex());
        }));
}

void SQListView::setModel(QAbstractItemModel *m)
{
    if (model())
        disconnect(model(), nullptr, this, nullptr);
    QListView::setModel(m);
    if (model()) {
        connect(model(), &QAbstractItemModel::modelReset, this, &SQListView::checkUpdateCurrent);
        connect(model(), &QAbstractItemModel::rowsMoved, this, &SQListView::checkUpdateCurrent);
        connect(model(), &QAbstractItemModel::rowsRemoved, this, &SQListView::checkUpdateCurrent);
        connect(model(), &QAbstractItemModel::rowsInserted, this, &SQListView::checkUpdateCurrent);
    }
}

const sodium::cell<boost::optional<int>> &SQListView::cCurrentIndex() const
{
    return m_currentIndex;
}

void SQListView::currentChanged(const QModelIndex &current, const QModelIndex &previous)
{
    Q_UNUSED(previous)
    updateCurrent(current);
}

void SQListView::checkUpdateCurrent()
{
    const QModelIndex current = currentIndex();
    const boost::optional<int> cIndex = m_currentIndex.sample();
    if (current.isValid() != bool(cIndex)
        || (current.isValid() && cIndex && current.row() != *cIndex)) {
        updateCurrent(current);
    }
}

void SQListView::updateCurrent(const QModelIndex &current)
{
    transaction t;
    if (current.isValid())
        m_currentIndex.send(current.row());
    else
        m_currentIndex.send(boost::none);
}

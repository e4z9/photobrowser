#include "sqlistview.h"

using namespace sodium;

SQListView::SQListView(const sodium::stream<boost::optional<int>> &sCurrentIndex)
    : m_currentIndex(boost::none)
{
    m_unsubscribe += sCurrentIndex.listen(
        ensureSameThread<boost::optional<int>>(this, [this](boost::optional<int> i) {
            if (!model())
                return;
            blockChange = true;
            if (i) {
                const QModelIndex index = model()->index(*i, 0);
                setCurrentIndex(index);
                scrollTo(index);
            } else {
                setCurrentIndex(QModelIndex());
            }
            blockChange = false;
        }));
    m_currentIndex = sCurrentIndex.or_else(m_sUserCurrentIndex).hold(boost::none);
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
    // for whatever reason, if there is no current item and the view gets focus,
    // the current item is set to the first, but the selection is not set, nor visible
    if (!previous.isValid() && current.isValid() && !selectionModel()->isSelected(current))
        selectionModel()->select(current, QItemSelectionModel::Select);
    checkUpdateCurrent();
}

void SQListView::checkUpdateCurrent()
{
    if (blockChange)
        return;
    const QModelIndex current = currentIndex();
    const boost::optional<int> cIndex = m_currentIndex.sample();
    if (current.isValid() != bool(cIndex)
        || (current.isValid() && cIndex && current.row() != *cIndex)) {
        m_sUserCurrentIndex.send(current.isValid() ? boost::make_optional(current.row())
                                                   : boost::none);
    }
}

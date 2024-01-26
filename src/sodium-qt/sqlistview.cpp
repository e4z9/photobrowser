#include "sqlistview.h"

using namespace sodium;

SQListView::SQListView(const sodium::stream<boost::optional<int>> &sCurrentIndex)
    : m_currentIndex(boost::none)
    , m_count(0)
    , m_viewportSize(QSize())
{
    const stream<boost::optional<int>> sBoundedCurrentIndex = sCurrentIndex.filter(
        [this](const auto &optIndex) {
            return !optIndex || (*optIndex < m_count.sample() && *optIndex >= 0);
        });
    m_unsubscribe.insert_or_assign(
        "currentIndex",
        sBoundedCurrentIndex.listen(
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
            })));
    m_currentIndex = sBoundedCurrentIndex.or_else(m_sUserCurrentIndex).hold(boost::none);
}

void SQListView::setModel(QAbstractItemModel *m)
{
    if (model())
        disconnect(model(), nullptr, this, nullptr);
    QListView::setModel(m);
    if (model()) {
        connect(model(), &QAbstractItemModel::modelReset, this, &SQListView::updateCountAndCurrent);
        connect(model(), &QAbstractItemModel::rowsMoved, this, &SQListView::updateCurrent);
        connect(model(), &QAbstractItemModel::rowsRemoved, this, &SQListView::updateCountAndCurrent);
        connect(model(),
                &QAbstractItemModel::rowsInserted,
                this,
                &SQListView::updateCountAndCurrent);
    }
    updateCountAndCurrent();
}

const sodium::cell<QSize> &SQListView::viewportSize() const
{
    return m_viewportSize;
}

const sodium::cell<boost::optional<int>> &SQListView::cCurrentIndex() const
{
    return m_currentIndex;
}

const sodium::cell<int> &SQListView::count() const
{
    return m_count;
}

void SQListView::currentChanged(const QModelIndex &current, const QModelIndex &previous)
{
    Q_UNUSED(previous)
    // for whatever reason, if there is no current item and the view gets focus,
    // the current item is set to the first, but the selection is not set, nor visible
    if (!previous.isValid() && current.isValid() && !selectionModel()->isSelected(current))
        selectionModel()->select(current, QItemSelectionModel::Select);
    updateCurrent();
}

bool SQListView::viewportEvent(QEvent *ev)
{
    if (ev->type() == QEvent::Resize)
        post<QSize>(this,
                    [this](const QSize &size) { m_viewportSize.send(size); })(viewport()->size());
    return SQWidgetBase<QListView>::viewportEvent(ev);
}

void SQListView::updateCountAndCurrent()
{
    transaction t; // put this and updateCurrent into same transaction
    const int count = m_count.sample();
    const int modelCount = model() ? model()->rowCount() : 0;
    if (count != modelCount)
        m_count.send(modelCount);
    updateCurrent();
}

void SQListView::updateCurrent()
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

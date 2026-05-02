#include "tagsview.h"

#include <sqtools.h>

#include <QAbstractItemModel>
#include <QApplication>
#include <QLabel>
#include <QMetaType>
#include <QTreeView>
#include <QVBoxLayout>

static const int kNameColumn = 0;

QDataStream &operator<<(QDataStream &s, const Tags &tags)
{
    QStringList str;
    std::transform(tags.cbegin(), tags.cend(), std::back_inserter(str), [](const Tag &t) {
        return QUrl::toPercentEncoding(t.name) + "," + t.shortcut.toString();
    });
    s << str.join(';');
    return s;
}

QDataStream &operator>>(QDataStream &s, Tags &tags)
{
    QString str;
    s >> str;
    const QStringList strs = str.split(';');
    tags.clear();
    for (const QString &s : strs) {
        const QStringList pairs = s.split(',');
        if (pairs.size() == 2)
            tags.append(Tag{pairs.at(0), QKeySequence::fromString(pairs.at(1))});
    }
    return s;
}

/*
 * - Liste gesammelten Tags
 * - Tag hat einen Namen
 * - Tag kann gerade aktiv oder inaktiv sein
 * - Tag kann einen shortcut haben oder nicht
 *     - eine QAction mit oder ohne shortcut?
 *     
 * Sieht so aus, als würde ein Tag = eine QAction (mit Name=Text, aktiv=checked, shortcut=shortcut)
 * Name & Shortcut sind persistent,
 * aktivität ist temporär.
 * 
 * Interaktion
 * - Anforderung, nach Tag zu suchen (evtl zusätzlich)
 * - Tags aktivieren/inaktivieren
 * - shortcut ändern
 * - gespeicherte Tags bereinigen
 * 
 * Tags kommen von
 * - settings
 * - aktuellem Verzeichnis
 * - angelegt von Benutzer
 */

class TagsModel : public QAbstractItemModel
{
public:
    TagsModel(const sodium::cell<Tags> &tags,
              const sodium::cell<QStringList> &highlightedTags,
              QObject *parent);

public:
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;

private:
    struct ModelTag
    {
        Tag tag;
        bool isHighlighted;
    };

    Unsubscribe m_unsubscribe;
    QList<ModelTag> m_tags;
};

class TagsView : public QWidget
{
public:
    TagsView(const sodium::cell<Tags> &tags, const sodium::cell<QStringList> &highlightedTags);
};

TagsView::TagsView(const sodium::cell<Tags> &tags, const sodium::cell<QStringList> &highlightedTags)
{
    auto model = new TagsModel(tags, highlightedTags, this);
    auto view = new QTreeView;
    view->setModel(model);
    view->setHeaderHidden(true);
    view->setAllColumnsShowFocus(true);
    view->setExpandsOnDoubleClick(false);
    view->setItemsExpandable(false);
    view->setRootIsDecorated(false);
    view->setUniformRowHeights(true);
    view->setTextElideMode(Qt::ElideNone);
    auto layout = new QVBoxLayout;
    // layout->setContentsMargins(0, 0, 0, 0);
    setLayout(layout);
    layout->addWidget(new QLabel(tr("Tags:")));
    layout->addWidget(view);
}

TagsModel::TagsModel(const sodium::cell<Tags> &tags,
                     const sodium::cell<QStringList> &highlightedTags,
                     QObject *parent)
    : QAbstractItemModel(parent)
{
    m_unsubscribe.insert_or_assign("tags", tags.listen([this](const Tags &tags) {
        beginResetModel();
        m_tags.clear();
        std::transform(tags.constBegin(),
                       tags.constEnd(),
                       std::back_insert_iterator(m_tags),
                       [](const Tag &t) { return ModelTag{t, false}; });
        std::sort(m_tags.begin(), m_tags.end(), [](const ModelTag &a, const ModelTag &b) {
            return a.tag.name.localeAwareCompare(b.tag.name) < 0;
        });
        endResetModel();
    }));
    m_unsubscribe.insert_or_assign("highlightedTags",
                                   highlightedTags.listen([this](const QStringList &highlighted) {
                                       for (int i = 0; i < m_tags.size(); ++i) {
                                           ModelTag &tag = m_tags[i];
                                           const bool isHighlighted = highlighted.contains(
                                               tag.tag.name);
                                           if (isHighlighted != tag.isHighlighted) {
                                               tag.isHighlighted = isHighlighted;
                                               const QModelIndex idx = index(i, kNameColumn);
                                               emit dataChanged(idx, idx, {Qt::ForegroundRole});
                                           }
                                       }
                                   }));
}

QModelIndex TagsModel::index(int row, int column, const QModelIndex &parent) const
{
    if (parent.isValid())
        return {};
    return createIndex(row, column);
}

QModelIndex TagsModel::parent(const QModelIndex &) const
{
    return {};
}

int TagsModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_tags.size();
}

int TagsModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return 1;
}

static QColor lighterDarker(const QColor &c, float value)
{
    const QColor cHsv = c.toHsv();
    float v = cHsv.valueF();
    v = v - (v - 0.5f) * value;
    return QColor::fromHsvF(cHsv.hueF(), cHsv.saturationF(), v);
}

QVariant TagsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_tags.size()) {
        return {};
    }
    const ModelTag &tag = m_tags.at(index.row());
    if (role == Qt::DisplayRole && index.column() == kNameColumn)
        return tag.tag.name.split('\n').first();
    if (role == Qt::ForegroundRole) {
        const QColor text = QApplication::palette().color(QPalette::Text);
        const QColor lighterDarkerText = lighterDarker(text, 0.8f);
        return tag.isHighlighted ? text : lighterDarkerText;
    }
    if (role == Qt::BackgroundRole) {
        const QColor base = QApplication::palette().color(QPalette::Base);
        const QColor lighterDarkerBase = lighterDarker(base, 0.2f);
        return tag.isHighlighted ? lighterDarkerBase : base;
    }
    return {};
}

static Tags mergeTags(const Tags &add, const Tags &state)
{
    auto result = state;
    for (const Tag &t : add) {
        const bool isNew = !std::any_of(result.cbegin(), result.cend(), [t](const Tag &s) {
            return s.name == t.name;
        });
        if (isNew)
            result.append(t);
    }
    return result;
}

TagsManager::TagsManager(const sodium::stream<Tags> &sAddTags,
                         const sodium::cell<QStringList> &highlightedTags)
    : m_tags(sAddTags.accum<Tags>(Tags(), mergeTags))
{
    qRegisterMetaType<Tags>();
    m_view = std::make_unique<TagsView>(m_tags, highlightedTags);
}

TagsManager::~TagsManager() = default;

const sodium::cell<Tags> &TagsManager::tags() const
{
    return m_tags;
}

QWidget *TagsManager::view()
{
    return m_view.get();
}

Tags::Tags()
    : QList<Tag>()
{}

Tags Tags::fromSet(const QSet<QString> &set)
{
    Tags ts;
    for (const QString &t : set)
        ts.append({t, {}});
    return ts;
}

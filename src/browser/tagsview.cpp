#include "tagsview.h"

#include <QAbstractItemModel>
#include <QApplication>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMetaType>
#include <QTreeView>
#include <QVBoxLayout>

static const int kNameColumn = 0;
static const int kShortcutColumn = 1;

static const int kTagRole = Qt::UserRole + 1;

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

    const sodium::stream<Tag> &sShortcutEdited() const { return m_sShortcutEdited; }

private:
    void showContextMenu(const QPoint &pos);
    void showShortcutDialog(const Tag &tag);

    QTreeView *m_view;
    sodium::stream_sink<Tag> m_sShortcutEdited;
};

TagsView::TagsView(const sodium::cell<Tags> &tags, const sodium::cell<QStringList> &highlightedTags)
{
    auto model = new TagsModel(tags, highlightedTags, this);
    m_view = new QTreeView;
    m_view->setModel(model);
    m_view->setHeaderHidden(true);
    m_view->setAllColumnsShowFocus(true);
    m_view->setExpandsOnDoubleClick(false);
    m_view->setItemsExpandable(false);
    m_view->setRootIsDecorated(false);
    m_view->setUniformRowHeights(true);
    m_view->setSelectionMode(QAbstractItemView::NoSelection);
    m_view->setTextElideMode(Qt::ElideNone);
    auto layout = new QVBoxLayout;
    setLayout(layout);
    layout->addWidget(new QLabel(tr("Tags:")));
    layout->addWidget(m_view);

    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_view, &QWidget::customContextMenuRequested, this, &TagsView::showContextMenu);
}

void TagsView::showContextMenu(const QPoint &pos)
{
    const QModelIndex index = m_view->indexAt(pos);
    if (!index.isValid())
        return;

    auto menu = new QMenu(m_view);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->addAction(tr("Change Shortcut"), this, [this, pos] {
        const QModelIndex index = m_view->indexAt(pos);
        if (!index.isValid())
            return;
        const auto tag = m_view->model()->data(index, kTagRole).value<Tag>();
        showShortcutDialog(tag);
    });
    menu->move(m_view->viewport()->mapToGlobal(pos));
    menu->show();
}

void TagsView::showShortcutDialog(const Tag &tag)
{
    auto inputDialog = new QInputDialog(this);
    inputDialog->setAttribute(Qt::WA_DeleteOnClose);
    inputDialog->setInputMode(QInputDialog::TextInput);
    inputDialog->setLabelText(tr("Shortcut for \"%1\":").arg(tag.name));
    inputDialog->setWindowTitle(tr("Set Shortcut"));
    inputDialog->setTextValue(tag.shortcut.toString());
    connect(inputDialog, &QDialog::accepted, this, [this, tag, inputDialog] {
        const QString shortcutText = inputDialog->textValue().trimmed();
        const QKeySequence shortcut = QKeySequence::fromString(shortcutText);
        if (shortcut.isEmpty() && !shortcutText.isEmpty())
            return;
        Tag newTag = tag;
        newTag.shortcut = shortcut;
        m_sShortcutEdited.send(newTag);
    });
    inputDialog->show();
}

TagsModel::TagsModel(const sodium::cell<Tags> &tags,
                     const sodium::cell<QStringList> &highlightedTags,
                     QObject *parent)
    : QAbstractItemModel(parent)
{
    m_unsubscribe
        .insert_or_assign("tags", tags.listen(ensureSameThread<Tags>(this, [this](const Tags &tags) {
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
        })));
    m_unsubscribe.insert_or_assign(
        "highlightedTags",
        highlightedTags.listen(
            ensureSameThread<QStringList>(this, [this](const QStringList &highlighted) {
                for (int i = 0; i < m_tags.size(); ++i) {
                    ModelTag &tag = m_tags[i];
                    const bool isHighlighted = highlighted.contains(tag.tag.name);
                    if (isHighlighted != tag.isHighlighted) {
                        tag.isHighlighted = isHighlighted;
                        const QModelIndex idx1 = index(i, 0);
                        const QModelIndex idx2 = index(i, columnCount({}));
                        emit dataChanged(idx1, idx2, {Qt::ForegroundRole});
                    }
                }
            })));
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
    return 2;
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
    if (role == Qt::DisplayRole && index.column() == kShortcutColumn)
        return tag.tag.shortcut.toString(QKeySequence::NativeText);
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
    if (role == kTagRole) {
        return QVariant::fromValue(tag.tag);
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
    : m_tags(Tags())
{
    qRegisterMetaType<Tags>();

    sodium::cell_loop<Tags> tags;
    m_view = std::make_unique<TagsView>(tags, highlightedTags);

    const sodium::stream<Tags> sInputTags = sAddTags.snapshot(tags, &mergeTags);
    const sodium::stream<Tags> sEditedTags = m_view->sShortcutEdited()
                                                 .snapshot(tags, [](const Tag &input, Tags tags) {
                                                     for (Tag &t : tags) {
                                                         if (t.name == input.name) {
                                                             t.shortcut = input.shortcut;
                                                             break;
                                                         }
                                                     }
                                                     return tags;
                                                 });
    m_tags = sInputTags.or_else(sEditedTags).hold(Tags());
    tags.loop(m_tags);

    m_unsubscribe.insert_or_assign(
        "tags", m_tags.listen(ensureSameThread<Tags>(qApp, [this](const Tags &tags) {
            // update QActions
            QHash<QString, QAction *> updated;
            for (const Tag &t : tags) {
                const auto it = m_actions.constFind(t.name);
                if (it != m_actions.constEnd()) {
                    QAction *action = it.value();
                    action->setShortcut(t.shortcut);
                    updated.insert(t.name, action);
                } else {
                    auto action = new QAction(t.name, qApp);
                    m_view->addAction(action);
                    action->setShortcutContext(Qt::ApplicationShortcut);
                    action->setShortcut(t.shortcut);
                    QObject::connect(action,
                                     &QAction::triggered,
                                     m_view.get(),
                                     [this, name = t.name] { m_sToggleTag.send(name); });
                    updated.insert(t.name, action);
                }
            }
            // clean up
            for (const auto [k, action] : std::as_const(m_actions).asKeyValueRange()) {
                if (!updated.contains(k))
                    delete action;
            }
            // assign
            m_actions = updated;
        })));
}

TagsManager::~TagsManager() = default;

const sodium::cell<Tags> &TagsManager::tags() const
{
    return m_tags;
}

const sodium::stream<QString> &TagsManager::sToggleTag() const
{
    return m_sToggleTag;
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

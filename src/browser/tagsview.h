#pragma once

#include <sqtools.h>

#include <QAction>
#include <QHash>
#include <QKeySequence>
#include <QWidget>

#include <sodium/sodium.h>

#include <memory.h>

struct Tag
{
    QString name;
    QKeySequence shortcut;
};

Q_DECLARE_METATYPE(Tag)

class Tags : public QList<Tag>
{
public:
    Tags();

    static Tags fromSet(const QSet<QString> &set);
};

QDataStream &operator<<(QDataStream &s, const Tags &tag);
QDataStream &operator>>(QDataStream &s, Tags &tag);

Q_DECLARE_METATYPE(Tags)

class TagsView;

class TagsManager
{
public:
    TagsManager(const sodium::stream<Tags> &sAddTags,
                const sodium::cell<QStringList> &highlightedTags);
    ~TagsManager();

    const sodium::cell<Tags> &tags() const;

    const sodium::stream<QString> &sToggleTag() const;

    QWidget *view();

private:
    Unsubscribe m_unsubscribe;
    QHash<QString, QAction *> m_actions;

    sodium::cell<Tags> m_tags;
    sodium::stream_sink<QString> m_sToggleTagAction;
    sodium::stream<QString> m_sToggleTag;
    std::unique_ptr<TagsView> m_view;
};

#pragma once

#include <QKeySequence>
#include <QWidget>

#include <sodium/sodium.h>

#include <memory.h>

struct Tag
{
    QString name;
    QKeySequence shortcut;
};

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

    QWidget *view();

private:
    sodium::cell<Tags> m_tags;
    std::unique_ptr<TagsView> m_view;
};

#pragma once

#include "mediadirectorymodel.h"

#include <sqaction.h>
#include <sqtools.h>

#include <QMainWindow>
#include <QSettings>
#include <QTimer>

#include <sodium/sodium.h>

QT_BEGIN_NAMESPACE
class QCheckBox;
QT_END_NAMESPACE

class DirectoryTree;
class FullscreenSplitter;

namespace Utils { class ProgressIndicator; }

class Settings
{
public:
    template<typename T>
    const sodium::stream<T> add(const QByteArray &key, const sodium::cell<T> &value);
    template<typename T>
    const sodium::stream<T> addInt(const QByteArray &key, const sodium::cell<T> &value);
    const sodium::stream<QVariant> add(const QByteArray &key, const sodium::cell<QVariant> &value);

    void restore(QSettings *s);
    void save(QSettings *s);

private:
    struct Setting
    {
        Setting(const QByteArray &key, const sodium::cell<QVariant> &value);
        QByteArray key;
        sodium::cell<QVariant> value;
        sodium::stream_sink<QVariant> sRestore;
    };
    std::vector<Setting> m_settings;
};

class BrowserWindow : public QMainWindow
{
    Q_OBJECT

public:
    BrowserWindow(QWidget *parent = nullptr);

    void restore(QSettings *settings);
    void save(QSettings *settings);

    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void adaptProgressIndicator();

    Settings m_settings;
    sodium::stream_sink<bool> m_sFullscreen;
    FullscreenSplitter *m_splitter = nullptr;
    DirectoryTree *m_fileTree = nullptr;
    Utils::ProgressIndicator *m_progressIndicator = nullptr;
    QTimer m_progressTimer;
    std::unique_ptr<MediaDirectoryModel> m_model;
    Unsubscribe m_unsubscribe;
};

template<typename T>
const sodium::stream<T> Settings::add(const QByteArray &key, const sodium::cell<T> &value)
{
    return add(key, value.map([](const T &t) -> QVariant { return qVariantFromValue(t); }))
        .map([](const QVariant &v) { return v.value<T>(); });
}

template<typename T>
const sodium::stream<T> Settings::addInt(const QByteArray &key, const sodium::cell<T> &value)
{
    return add(key, value.map([](const T &t) -> QVariant { return int(t); }))
        .map([](const QVariant &v) { return T(v.toInt()); });
}

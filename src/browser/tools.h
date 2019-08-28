#pragma once

#include <QObject>
#include <QThread>
#include <QTimer>

#include <functional>
#include <vector>

template<typename A>
std::function<void(A)> ensureSameThread(QObject *guard, const std::function<void(A)> &action)
{
    return [guard, action](const A &a) -> void {
        if (guard->thread() == QThread::currentThread())
            action(a);
        QTimer::singleShot(0, guard, [action, a] { action(a); });
    };
}

class Unsubscribe
{
public:
    Unsubscribe &operator+=(const std::function<void()> &&unsub);
    ~Unsubscribe();

private:
    std::vector<std::function<void()>> m_unsubs;
};

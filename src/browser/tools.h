#pragma once

#include <QCoreApplication>
#include <QObject>
#include <QTimer>

#include <functional>

template<typename A>
std::function<void(A)> ensureMainThread(QObject *guard, const std::function<void(A)> &action)
{
    return [guard, action](const A &a) -> void {
        if (guard->thread() == QCoreApplication::instance()->thread())
            action(a);
        QTimer::singleShot(0, guard, [action, a] { action(a); });
    };
}

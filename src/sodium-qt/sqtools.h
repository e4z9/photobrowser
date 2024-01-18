#pragma once

#include <QObject>
#include <QThread>

#include <sodium/sodium.h>

#include <functional>

template<typename A>
std::function<void(A)> ensureSameThread(QObject *guard, const std::function<void(A)> &action)
{
    return [guard, action](const A &a) -> void {
        QMetaObject::invokeMethod(guard, [action, a] { action(a); });
    };
}

template<typename A, typename B, typename C>
std::function<void(A)> ensureSameThread(B *guard, void (C::*f)(A))
{
    return ensureSameThread<A>(guard, std::bind(f, guard, std::placeholders::_1));
}

template<typename A, typename B, typename C>
std::function<void(A)> ensureSameThread(B *guard, void (C::*f)(const A &))
{
    return ensureSameThread<A>(guard, std::bind(f, guard, std::placeholders::_1));
}

template<typename A>
std::function<void(A)> post(QObject *guard, const std::function<void(A)> &action)
{
    return [guard, action](const A &a) -> void {
        QMetaObject::invokeMethod(
            guard, [action, a] { action(a); }, Qt::QueuedConnection);
    };
}

void post(QObject *guard, const std::function<void()> &action);

template<typename A, typename B, typename C>
std::function<void(A)> post(B *guard, void (C::*f)(A))
{
    return post<A>(guard, std::bind(f, guard, std::placeholders::_1));
}

template<typename A, typename B, typename C>
std::function<void(A)> post(B *guard, void (C::*f)(const A &))
{
    return post<A>(guard, std::bind(f, guard, std::placeholders::_1));
}

class UnsubscribeFunction
{
public:
    UnsubscribeFunction(const std::function<void()> &&unsub);
    ~UnsubscribeFunction();

    UnsubscribeFunction(UnsubscribeFunction &&other);
    UnsubscribeFunction &operator=(UnsubscribeFunction &&other);

    UnsubscribeFunction(const UnsubscribeFunction &) = delete;
    UnsubscribeFunction &operator=(UnsubscribeFunction &) = delete;

private:
    std::function<void()> m_unsub;
};

using Unsubscribe = std::unordered_map<std::string, UnsubscribeFunction>;

template<typename T>
class SQWidgetWrapper : public T
{
public:
    SQWidgetWrapper();
    SQWidgetWrapper(const sodium::cell<bool> &visible);

private:
    Unsubscribe m_unsubscribe;
};

template<typename T>
SQWidgetWrapper<T>::SQWidgetWrapper()
    : SQWidgetWrapper(true)
{}

template<typename T>
SQWidgetWrapper<T>::SQWidgetWrapper(const sodium::cell<bool> &visible)
{
    m_unsubscribe.insert_or_assign("visible",
                                   visible.listen(ensureSameThread<bool>(this, [this](bool v) {
                                       T::setVisible(v);
                                   })));
}

template<typename A>
sodium::stream<A> calm(const sodium::stream<A> &s, const sodium::lazy<boost::optional<A>> &optInit)
{
    return sodium::filter_optional(
        s.collect_lazy(optInit,
                       [](const A &a, const boost::optional<A> &optLastA)
                           -> std::tuple<boost::optional<A>, boost::optional<A>> {
                           if (optLastA && *optLastA == a)
                               return {{}, optLastA};
                           return {{a}, {a}};
                       }));
}

template<typename A>
sodium::stream<A> calm(const sodium::stream<A> &s)
{
    return calm(s, {});
}

template<typename A>
sodium::cell<A> calm(const sodium::cell<A> &a)
{
    const auto init = a.sample_lazy();
    const auto optInit = init.map([](const A &a) -> boost::optional<A> { return a; });
    return calm(a.updates(), optInit).hold_lazy(init);
}

#define DEFINE_BOOL_TYPE(type) \
    class type \
    { \
    public: \
        type(bool b) \
            : value(b) \
        {} \
        operator bool() const { return value; } \
        bool toBool() { return value; } \
        static type fromBool(bool b) { return b; } \
\
    private: \
        bool value; \
    };

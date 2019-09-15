#pragma once

#include <QObject>
#include <QThread>

#include <sodium/sodium.h>

#include <functional>
#include <vector>

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

class Unsubscribe
{
public:
    Unsubscribe &operator+=(const std::function<void()> &&unsub);
    ~Unsubscribe();

private:
    std::vector<std::function<void()>> m_unsubs;
};

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

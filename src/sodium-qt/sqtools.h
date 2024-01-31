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

template<typename Value>
class UserValue
{
public:
    UserValue(const Value &initialValue, QObject *guard, const std::function<void(Value)> &slot);

    void setUserValue(const Value &value);
    void setValue(const sodium::stream<Value> &value, Value initialValue);
    const sodium::cell<Value> &value() const;

private:
    QObject *m_guard;
    sodium::stream_sink<Value> m_sUserValue;
    sodium::cell<Value> m_value;
    const std::function<void(Value)> m_slot;
    std::unique_ptr<UnsubscribeFunction> m_unsubscribe;
};

template<typename Value>
UserValue<Value>::UserValue(const Value &initialValue,
                            QObject *guard,
                            const std::function<void(Value)> &slot)
    : m_guard(guard)
    , m_value(initialValue)
    , m_slot(slot)
{
    setValue({}, initialValue);
}

template<typename Value>
void UserValue<Value>::setUserValue(const Value &value)
{
    m_sUserValue.send(value);
}

template<typename Value>
void UserValue<Value>::setValue(const sodium::stream<Value> &value, Value initialValue)
{
    m_value = calm(m_sUserValue.or_else(value).hold(initialValue));
    // use m_isChecked.sample in the potentially async listener,
    // in case a user change is posted in between
    m_unsubscribe.reset(new UnsubscribeFunction(
        m_value.listen(post<Value>(m_guard, [this](Value) { m_slot(m_value.sample()); }))));
}

template<typename Value>
const sodium::cell<Value> &UserValue<Value>::value() const
{
    return m_value;
}

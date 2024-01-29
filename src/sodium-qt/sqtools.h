#pragma once

#include <QObject>
#include <QThread>

#include <sodium/sodium.h>

#include <functional>

/*!
 * \brief ensureSameThread makes sure that the given action is executed in the same thread as the
 *        guard, by invoking the action on the event loop if necessary. Note that ensureSameThread
 *        itself does not execute anything, it returns a function that does that.
 * \param guard Defines the thread that the action is executed on. If the action is posted on the
 *              event thread, and the guard is deleted in the meantime, the action is not executed.
 * \param action The function to execute. It takes a parameter of type A and does not return
 *               anything.
 * \return A function object that takes a parameter of type A and executes the action in the same
 *         thread as guard.
 */
template<typename A>
std::function<void(A)> ensureSameThread(QObject *guard, const std::function<void(A)> &action)
{
    return [guard, action](const A &a) -> void {
        QMetaObject::invokeMethod(guard, [action, a] { action(a); });
    };
}

/*!
 * \brief ensureSameThread makes sure that the given member function of guard is executed in the
 *        same thread as the guard, by invoking the action on the event loop if necessary. Note
 *        that ensureSameThread itself does not execute anything, it returns a function that does
 *        that.
 * \param guard Defines the thread that the action is executed on. If the action is posted on the
 *              event thread, and the guard is deleted in the meantime, the action is not executed.
 * \param action The member function to execute. It takes a parameter of type A and does not return
 *               anything.
 * \return A function object that takes a parameter of type A and executes the action in the same
 *         thread as guard.
 */
template<typename A, typename B, typename C>
std::function<void(A)> ensureSameThread(B *guard, void (C::*f)(A))
{
    return ensureSameThread<A>(guard, std::bind(f, guard, std::placeholders::_1));
}

/*!
 * \brief ensureSameThread makes sure that the given member function of guard is executed in the
 *        same thread as the guard, by invoking the action on the event loop if necessary. Note
 *        that ensureSameThread itself does not execute anything, it returns a function that does
 *        that.
 * \param guard Defines the thread that the action is executed on. If the action is posted on the
 *              event thread, and the guard is deleted in the meantime, the action is not executed.
 * \param action The member function to execute. It takes a parameter of type A and does not return
 *               anything.
 * \return A function object that takes a parameter of type A and executes the action in the same
 *         thread as guard.
 */
template<typename A, typename B, typename C>
std::function<void(A)> ensureSameThread(B *guard, void (C::*f)(const A &))
{
    return ensureSameThread<A>(guard, std::bind(f, guard, std::placeholders::_1));
}

/*!
 * \brief post returns a function that executes the given action delayed via the event loop.
 *        The function takes a parameter of type A that is passed on to the action.
 * \param guard Defines the thread that the action is executed on. If the action is posted on the
 *              event thread, and the guard is deleted in the meantime, the action is not executed.
 * \param action The function to execute. It takes a parameter of type A and does not return
 *               anything.
 * \return A function object that takes a parameter of type A and executes the action delayed via
 *         the event loop in the same thread as guard.
 */
template<typename A>
std::function<void(A)> post(QObject *guard, const std::function<void(A)> &action)
{
    return [guard, action](const A &a) -> void {
        QMetaObject::invokeMethod(
            guard, [action, a] { action(a); }, Qt::QueuedConnection);
    };
}

/*!
 * \brief This overload of post executes the given action delayed via the event loop.
 * \param guard Defines the thread that the action is executed on. If the action is posted on the
 *              event thread, and the guard is deleted in the meantime, the action is not executed.
 * \param action The function to execute.
 */
void post(QObject *guard, const std::function<void()> &action);

template<typename A, typename B, typename C>
std::function<void(A)> post(B *guard, void (C::*f)(A))
{
    return post<A>(guard, std::bind(f, guard, std::placeholders::_1));
}

/*!
 * \brief post returns a function that executes the given member function delayed via the event
 *        loop. The function takes a parameter of type A that is passed on to the action.
 * \param guard Defines the thread that the action is executed on. If the action is posted on the
 *              event thread, and the guard is deleted in the meantime, the action is not executed.
 * \param action The member function to execute. It takes a parameter of type A and does not return
 *               anything.
 * \return A function object that takes a parameter of type A and executes the action delayed via
 *         the event loop in the same thread as guard.
 */
template<typename A, typename B, typename C>
std::function<void(A)> post(B *guard, void (C::*f)(const A &))
{
    return post<A>(guard, std::bind(f, guard, std::placeholders::_1));
}

/*!
 * \brief The UnsubscribeFunction class keeps a reference to a function,
 *        and executes it in the destructor. It cannot be copied, but moved.
 *        This is useful as a container for functions returned by
 *        sodium::listen.
 */
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

/*!
 * An Unsubscribe map is used to collect various UnsubscribeFunctions. When using
 * insert_or_assign, any previously saved UnsubscribeFunction for the same key is
 * deleted and therefore executed.
 */
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

/*!
 * \brief calm suppresses updates if the sent value is the same as the last one.
 *        This requires the type to provide operator==.
 * \param s The stream to calm.
 * \return A stream with updates of same values suppressed.
 */
template<typename A>
sodium::stream<A> calm(const sodium::stream<A> &s)
{
    return calm(s, {});
}

/*!
 * \brief calm suppresses updates if the sent value is the same as the last one.
 *        This requires the type to provide operator==.
 * \param s The cell to calm.
 * \return A cell with updates of same values suppressed.
 */
template<typename A>
sodium::cell<A> calm(const sodium::cell<A> &a)
{
    const auto init = a.sample_lazy();
    const auto optInit = init.map([](const A &a) -> boost::optional<A> { return a; });
    return calm(a.updates(), optInit).hold_lazy(init);
}

/*!
 * \brief The UserValue class implements a value that can both change by user interaction
 *        and programmatically.
 *
 *        Think of an input field: Its value might depend on other fields (clearing a text field
 *        with a "Clear" button, so its value depends on the "trigger" stream of the button)
 *        and also changes when the user types in it.
 *        
 *        The UserValue class models this with an input stream for programmatic changes, and
 *        and output cell for the current value which holds the latest value from either
 *        the programmatic or the user-input stream.
 *        
 *        It also handles the nitty-gritty details of when to send updates from the UI and when to
 *        trigger UI updates exactly, without falling into the "may not send in a listener" death
 *        trap or otherwise shooting yourself in the foot during the interaction with the non-FRP
 *        world of Qt.
 *        
 *        The slot passed to the constructor should directly update the corresponding property
 *        in the UI element.
 *        Call setUserValue() directly from the slot that you connected to the UI property's
 *        changed signal.
 */
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

/*!
 * \brief Creates a new UserValue.
 * \param initialValue The initial value for the output cell.
 * \param guard        Object to be used as the context of the slot. Used for posting (see post).
 * \param slot         Slot to call for updating the UI.
 */
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

/*!
 * \brief Notifies the UserValue class that the property has been changed by the user.
 *        Directly call this method from the slot that you connected to the UI property's
 *        changed signal.
 * \param value
 */
template<typename Value>
void UserValue<Value>::setUserValue(const Value &value)
{
    m_sUserValue.send(value);
}

/*!
 * \brief Sets the stream of programmatic changes that the value should respect.
 * \param value        The stream of values that is merged with the user changes.
 * \param initialValue The starting value of the UserValue.
 */
template<typename Value>
void UserValue<Value>::setValue(const sodium::stream<Value> &value, Value initialValue)
{
    m_value = calm(m_sUserValue.or_else(value).hold(initialValue));
    // use m_isChecked.sample in the potentially async listener,
    // in case a user change is posted in between
    m_unsubscribe.reset(new UnsubscribeFunction(
        m_value.listen(post<Value>(m_guard, [this](Value) { m_slot(m_value.sample()); }))));
}

/*!
 * \brief The cell that represents the current value of the UserValue.
 * \return The cell that represents the current value of the UserValue.
 */
template<typename Value>
const sodium::cell<Value> &UserValue<Value>::value() const
{
    return m_value;
}

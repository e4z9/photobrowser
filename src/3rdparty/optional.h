#pragma once

#if __cplusplus < 201703L

#include "optional/optional.hpp"

namespace std {

using std::experimental::optional;
using std::experimental::nullopt;
using std::experimental::nullopt_t;
using std::experimental::in_place;

template <class T>
constexpr optional<typename std::decay<T>::type> make_optional(T&& v)
{
  return optional<typename std::decay<T>::type>(std::experimental::constexpr_forward<T>(v));
}

template <class X>
constexpr optional<X&> make_optional(std::reference_wrapper<X> v)
{
  return optional<X&>(v.get());
}

} // namespace std

#else

#include <optional>

#endif

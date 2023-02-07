#pragma once

namespace std {

template<class T>
struct remove_reference { typedef T type; };

template<class T>
struct remove_reference<T&> { typedef T type; };

template<class T>
struct remove_reference<T&&> { typedef T type; };

template<typename T>
typename remove_reference<T>::type&& move(T&& arg) {
    return static_cast<typename remove_reference<T>::type&&>(arg);
}

template<typename T>
struct is_lvalue_reference { static constexpr bool value = false; };
template<typename T>
struct is_lvalue_reference<T&> { static constexpr bool value = true; };

template<typename T>
constexpr T&& forward(typename remove_reference<T>::type& arg) noexcept {
    return static_cast<T&&>(arg);
}

template<typename T>
constexpr T&& forward(typename remove_reference<T>::type&& arg) noexcept {
    static_assert(!is_lvalue_reference<T>::value, "invalid rvalue to lvalue conversion");
    return static_cast<T&&>(arg);
}

struct true_type { static constexpr bool value = true; };
struct false_type { static constexpr bool value = false; };

template<class T, class U>
struct is_same : std::false_type {};

template<class T> struct is_same<T, T> : std::true_type {};

template<typename T, typename U>
static constexpr bool is_same_v = is_same<T, U>::value;

}
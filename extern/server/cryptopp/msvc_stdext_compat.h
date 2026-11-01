#pragma once
#include <stddef.h>
namespace stdext {
    template <class T>
    constexpr T make_checked_array_iterator(T ptr, size_t) noexcept { return ptr; }
    template <class T>
    constexpr T make_unchecked_array_iterator(T ptr) noexcept { return ptr; }
}

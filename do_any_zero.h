#ifndef DO_ANY_TYPE_ZERO_H
#define DO_ANY_TYPE_ZERO_H

#include "Header.h"

template <typename T>
constexpr void do_zero(T& val)
{
    if constexpr (requires { val.clear(); })  // constexpr for compile-time inlining
        val.clear();
    else
        val = T{};
}

#endif // DO_ANY_TYPE_ZERO_H
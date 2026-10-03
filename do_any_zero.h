#ifndef DO_ANY_TYPE_ZERO_H
#define DO_ANY_TYPE_ZERO_H

#include "Header.h"

template <typename T>
inline std::enable_if_t<std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T>, void>
do_zero(T* val)                              { val = 0;       }

inline void do_zero(const char* val)         { val = nullptr; }
inline void do_zero(const std::string& val)  { val.empty();   }
inline void do_zero(std::string_view val)    { val.empty();   }


#endif // DO_ANY_TYPE_ZERO_H
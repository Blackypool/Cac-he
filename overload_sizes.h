#ifndef OVERLOAD_SIZES_FUNC_H
#define OVERLOAD_SIZES_FUNC_H

#include "M_cacher.h"

// POD = Plain Old Data -- trivially copyable (можно копи через memcpy) + standard layout (одинаковое расположение in mem для всех компиляторов)
// условие + type of ret || SFINAE = Substitution Failure Is Not An Error -> another перегрузка
template <typename T>
inline std::enable_if_t<std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T>, size_t>
check_byte_size(T val)                                  { return sizeof(val);       }

inline size_t check_byte_size(const char* val)          { return std::strlen(val);  }       // search strlen in std space
inline size_t check_byte_size(const std::string& val)   { return val.size();        }       // владеет буфером // & is cheeper
inline size_t check_byte_size(std::string_view val)     { return val.size();        }       // не владеет строкой


#endif
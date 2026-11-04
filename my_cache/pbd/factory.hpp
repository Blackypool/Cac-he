#pragma once

#include "../level_spec.hpp"
#include "cache.hpp"

#define MAX_LEVELS_NUM 5

#include <unordered_map>
#include <functional>
#include <utility>

namespace MyCache::pbd {

template <
    typename Key, 
    typename Value,
    typename Loader,
    typename Hash = std::hash<Key>,
    typename ... Specs
>
auto make_cache(Loader load_func, Hash hash_func = std::hash<Key>{}, Specs ... spec)
{
    static_assert(
        sizeof...(Specs) > 0,
        "make_cache requires at least one level"
    );

    static_assert(
        sizeof...(Specs) <= MAX_LEVELS_NUM,
        "Max number of levels is 5"
    );

    using CacheType = Cache <
        Key, 
        Value, 
        Loader, 
        typename Specs::template Level<
            Key,
            Value, 
            Hash
        > ...
    >;

    return CacheType{
        load_func,
        std::move(spec).template build<Key, Value, Hash>(hash_func)
        ...
    };
}

} // MyCache::pbd

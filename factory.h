#ifndef FACTORY_HH
#define FACTORY_HH

#define MAX_LEVELS_NUM 5

#include <unordered_map>

namespace MyCache {

template <
    template <class, class, class> class Policy,
    typename Options
>
struct LevelSpec 
{
    Options options;

    template <
        typename Key, 
        typename Value, 
        typename Hash
    > using Level = Policy<Key, Value, Hash>;

    template <
        typename Key, 
        typename Value, 
        typename Hash
    >
    Level<Key, Value, Hash> build(Hash hash_func) &&
    {
        return Level<Key, Value, Hash> {
            std::move(options),
            hash_func
        };
    }
};

template <
    template <class, class, class> class Policy,
    typename Options
>
auto level(Options options)
{
    return LevelSpec<Policy, Options>{std::move(options)};
}

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

}

#endif // FACTORY_HH
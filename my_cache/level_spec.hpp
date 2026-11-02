#ifndef LEVEL_SPEC_HH
#define LEVEL_SPEC_HH

#include <utility>

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

} // MyCache

#endif // LEVEL_SPEC_HH

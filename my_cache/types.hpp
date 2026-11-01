#ifndef TYPES_HH
#define TYPES_HH

#include <optional>

namespace MyCache {

template <typename Key, typename Value>
struct CacheEntry
{
    Key key;
    Value value;
};

template <typename Entry, typename Meta>
struct CacheTransfer
{
    Entry entry;
    Meta meta;
};

struct EmptyMeta {};

template <typename Evicted, typename Value>
struct AddResult
{
    std::optional<Evicted> evicted;
    Value* inserted;
};

} // MyCache

#endif // TYPES_HH
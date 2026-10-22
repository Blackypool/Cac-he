#ifndef TYPES_HH
#define TYPES_HH

namespace MyCache {

template <typename Key, typename Value, typename Meta>
struct CacheTransfer
{
    Key key;
    Value value;
    Meta meta;
};

struct EmptyMeta {};

template <typename Transfer, typename Value>
struct AddResult
{
    std::optional<Transfer> evicted;
    Value* inserted;
};

}

#endif // TYPES_HH
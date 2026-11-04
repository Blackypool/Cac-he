#pragma once

#include "../types.hpp"

#include <ostream>
#include <optional>

namespace MyCache::runtime {

template <typename Key, typename Value>
class ICacheLevel
{
    public:

        using Entry =  CacheEntry<Key, Value>;
        using Transfer = Entry;
        using Evicted  = Transfer;

        virtual AddResult<Transfer, Value> add(Entry entry) = 0;
        virtual const Value* get(const Key& key) = 0;
        virtual std::optional<Transfer> extract(const Key& key) = 0;
        virtual bool contains(const Key& key) const = 0;
        virtual void remove(const Key& key) = 0;
        virtual void dump(std::ostream& out) = 0;

        virtual ~ICacheLevel() = default;
};

} // MyCache::runtime 

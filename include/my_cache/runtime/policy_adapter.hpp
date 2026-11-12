#pragma once

#include "../types.hpp"
#include "cache_level.hpp"

#include <optional>
#include <ostream>
#include <utility>

namespace MyCache::runtime {

template <
    typename Key, 
    typename Value, 
    typename Policy
>
class PolicyAdapter final : 
    public ICacheLevel<Key, Value> 
{
    public:

        using Entry =  CacheEntry<Key, Value>;
        using Transfer = Entry;
        using Evicted  = Transfer;

    private:

        Policy policy_;

    public:

        PolicyAdapter(Policy policy)
        :   policy_(std::move(policy))
        {}

        AddResult<Evicted, Value> add(Entry entry) override
        {
            auto result = policy_.add(std::move(entry));

            std::optional<Evicted> evicted;

            if (result.evicted)
            {
                evicted = std::move(result.evicted->entry);
            }

            return AddResult<Evicted, Value> {
                std::move(evicted),
                result.inserted
            };
        }

        std::optional<Entry> extract(const Key& key) override
        {
            auto result = policy_.extract(key);

            if (!result)
            {
                return std::nullopt;
            }

            return std::move(result->entry);
        }

        const Value* get(const Key& key) override
        {
            return policy_.get(key);
        }

        bool contains(const Key& key) const override
        {
            return policy_.contains(key);
        }

        void remove(const Key& key) override
        {
            return policy_.remove(key);
        }

        void dump(std::ostream& out) override
        {
            return policy_.dump(out);
        }

        ~PolicyAdapter() override = default;
};

} // MyCache::runtime

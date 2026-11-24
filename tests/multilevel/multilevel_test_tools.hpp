#pragma once

#include <functional>
#include <memory>
#include <ostream>
#include <utility>

#include "my_cache/types.hpp"

namespace Tests {

// Copies of this test wrapper share one policy instance.
// Pass it to the cache by value to keep an observing copy in the test.
template<typename Policy>
class ObservedLevel
{
    private:

        std::shared_ptr<Policy> policy_;

    public:

        using Entry = typename Policy::Entry;
        using Transfer = typename Policy::Transfer;

        explicit ObservedLevel(Policy policy)
            : policy_(std::make_shared<Policy>(std::move(policy)))
        {}

        auto add(Entry entry)
        {
            return policy_->add(std::move(entry));
        }

        template<typename Meta>
        auto add(MyCache::CacheTransfer<Entry, Meta> transfer)
        {
            return policy_->add(std::move(transfer));
        }

        template<typename Key>
        const auto* get(const Key& key)
        {
            return policy_->get(key);
        }

        // Use contains() for observation: get() changes policy state.
        template<typename Key>
        bool contains(const Key& key) const
        {
            return std::as_const(*policy_).contains(key);
        }

        template<typename Key>
        auto extract(const Key& key)
        {
            return policy_->extract(key);
        }

        template<typename Key>
        void remove(const Key& key)
        {
            policy_->remove(key);
        }

        void dump(std::ostream& out) const
        {
            std::as_const(*policy_).dump(out);
        }
};

template<
    typename Key,
    typename Value,
    typename Spec,
    typename Hash = std::hash<Key>
>
auto make_observed_level(Spec spec, Hash hash = Hash{})
{
    using Policy = typename Spec::template Level<Key, Value, Hash>;

    return ObservedLevel<Policy>{
        std::move(spec).template build<Key, Value, Hash>(std::move(hash))
    };
}

} // namespace Tests


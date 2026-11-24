#pragma once

#include "../level_spec.hpp"
#include "../errors.hpp"
#include "cache.hpp"
#include "cache_level.hpp"
#include "policy_adapter.hpp"

#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace MyCache::runtime {

template <
    typename Key,
    typename Value,
    typename Spec,
    typename Hash = std::hash<Key>
>
auto make_level(Spec spec, Hash hash_func = Hash{})
{
    auto policy = std::move(spec).template build<Key, Value, Hash>(hash_func);

    using Level  = ICacheLevel<Key, Value>;
    using Policy = Spec::template Level<Key, Value, Hash>;

    return std::unique_ptr<Level> (
        std::make_unique<PolicyAdapter<Key, Value, Policy>>(std::move(policy))
    );
}

template <
    typename Key,
    typename Value,
    typename Hash = std::hash<Key>
>
class CacheBuilder
{
    public:

        using Level  = ICacheLevel<Key, Value>;
        using LevelPtr = std::unique_ptr<Level>;
        using Loader = std::function<Value(const Key&)>;

    private:

        std::vector<LevelPtr> levels_;
        Loader loader_;
        Hash hash_func_;
        bool built_ = false;

        void require_not_built() const
        {
            if (built_)
            {
                throw invalid_operation{"CacheBuilder has already been built"};
            }
        }

    public:

        CacheBuilder(Loader loader, Hash hash_func = Hash{})
        :   loader_(std::move(loader)),
            hash_func_(std::move(hash_func))
        {
            if (!loader_)
            {
                throw configuration_error{"Cache loader must not be empty"};
            }
        }

        template<typename Spec>
        void add_level(Spec spec)
        {
            require_not_built();

            levels_.push_back(
                make_level<Key, Value, Spec, Hash>(
                    std::move(spec),
                    hash_func_
                )
            );
        }
        
        void add_level(LevelPtr level)
        {
            require_not_built();
            
            if (!level)
            {
                throw configuration_error{"Cache level must not be null"};
            }
            levels_.push_back(std::move(level));
        }

        Cache<Key, Value> build() &&
        {
            require_not_built();
            if (levels_.empty())
            {
                throw configuration_error{"Cache must contain at least one level"};
            }

            built_ = true;
            return Cache<Key, Value> {
                std::move(loader_),
                std::move(levels_)
            };
        };
};

} // MyCache::runtime

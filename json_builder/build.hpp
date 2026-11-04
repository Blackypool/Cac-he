#pragma once

#include "../my_cache/runtime.hpp"
#include "../my_cache/policies/lfu_policy.hpp"
#include "../my_cache/policies/lru_policy.hpp"
#include "config.hpp"

#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace JsonConfig {

template <
    typename Key,
    typename Value,
    typename Loader
>
auto make_cache_from_json(
    const std::filesystem::path& path,
    Loader loader
)
{
    static_assert(
        std::is_invocable_r_v<Value, Loader&, const Key&>,
        "Loader must be callable as Value(const Key&)"
    );

    json root = read_json_file(path);
    const json& levels = require_levels(root);

    MyCache::runtime::CacheBuilder<Key, Value> builder{
        std::move(loader)
    };

    for (std::size_t index = 0; index < levels.size(); ++index)
    {
        const json& level = levels.at(index);
        const std::string policy_name =
            require_policy_name(level, index);
        const json& options = require_options(level, index);
        const std::size_t capacity = require_capacity(options, index);

        if (policy_name == "lru")
        {
            builder.add_level(
                MyCache::policy::LRU(capacity)
            );
        }
        else if (policy_name == "lfu")
        {
            builder.add_level(
                MyCache::policy::LFU(capacity)
            );
        }
        else
        {
            throw std::invalid_argument(
                "Cache level " + std::to_string(index) +
                ": unknown cache policy '" + policy_name + "'"
            );
        }
    }

    return std::move(builder).build();
}

} // namespace JsonConfig

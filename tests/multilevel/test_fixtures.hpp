#pragma once

#include <gtest/gtest.h>

#include <memory>
#include <utility>
#include <vector>

#include "my_cache/pbd.hpp"
#include "my_cache/runtime.hpp"

namespace Tests {

struct PBDTestMake
{
    template<typename Loader, typename ... Policies>
    static auto make_cache(Loader load, Policies ... policies)
    {
        return MyCache::pbd::Cache<int, int, Loader, MetaMode::drop, Policies ...>(
            std::move(load), std::move(policies) ...
        );
    }
};

struct RunTimeTestMake
{
    template<typename Loader, typename ... Policies>
    static auto make_cache(Loader load, Policies ... policies)
    {   
        using Level = MyCache::runtime::ICacheLevel<int, int>;
        std::vector<std::unique_ptr<Level>> levels;

        levels.reserve(sizeof...(Policies));
        (
            levels.push_back(
                std::make_unique<
                    MyCache::runtime::PolicyAdapter<int, int, Policies>
                >(std::move(policies))
            ),
            ...
        );

        return MyCache::runtime::Cache<int, int>(
            std::move(load), std::move(levels)
        );
    }
};

template <typename CacheType>
struct CacheFixture : public testing::Test
{
    template<typename Loader, typename ... Policies>
    auto make_cache(Loader load, Policies ... policies)
    {
        return CacheType::make_cache(load, policies ...);
    }
};

} // namespace Tests

#include <gtest/gtest.h>

#include <cstddef>
#include <initializer_list>
#include <string>
#include <type_traits>

#include "my_cache/policies/lru_policy.hpp"
#include "multilevel_test_tools.hpp"
#include "test_fixtures.hpp"

namespace {

using namespace Tests;
using namespace MyCache;

using Cache = testing::Types<
    PBDTestMake,
    RunTimeTestMake
>;

struct CacheNames
{
    template<typename Backend>
    static std::string GetName(int)
    {
        if constexpr (std::is_same_v<Backend, PBDTestMake>)
            return "PBD";
        else
            return "Runtime";
    }
};

TYPED_TEST_SUITE(CacheFixture, Cache, CacheNames);

auto make_lru(std::size_t capacity = 1)
{
    return make_observed_level<int, int>(policy::LRU(capacity));
}

// Observe residency without calling get(), which would change recency.
template<typename Level>
void expect_level(
    const Level& level,
    std::initializer_list<int> present,
    std::initializer_list<int> absent
)
{
    for (int key : present)
        EXPECT_TRUE(level.contains(key)) << "Expected resident key=" << key;

    for (int key : absent)
        EXPECT_FALSE(level.contains(key)) << "Unexpected resident key=" << key;
}

TYPED_TEST(CacheFixture, StartsEmpty)
{
    auto l1 = make_lru();
    auto l2 = make_lru();
    auto l3 = make_lru();
    int loads = 0;
    auto loader = [&loads](const int&) { ++loads; return -1; };
    auto cache = this->make_cache(loader, l1, l2, l3);

    EXPECT_FALSE(cache.contains(1));
    EXPECT_FALSE(cache.contains(2));
    expect_level(l1, {}, {1, 2});
    expect_level(l2, {}, {1, 2});
    expect_level(l3, {}, {1, 2});
    EXPECT_EQ(loads, 0);
}

TYPED_TEST(CacheFixture, MissLoadsOnceAndRepeatedGetHitsL1)
{
    auto l1 = make_lru();
    auto l2 = make_lru();
    auto l3 = make_lru();
    int loads = 0;
    auto loader = [&loads](const int& key)
    {
        EXPECT_EQ(key, 7);
        ++loads;
        return 777;
    };
    auto cache = this->make_cache(loader, l1, l2, l3);

    const int* value = cache.get(7);
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(*value, 777);
    EXPECT_EQ(loads, 1);
    EXPECT_TRUE(cache.contains(7));
    expect_level(l1, {7}, {});
    expect_level(l2, {}, {7});
    expect_level(l3, {}, {7});

    const int* repeated = cache.get(7);
    ASSERT_NE(repeated, nullptr);
    EXPECT_EQ(*repeated, 777);
    EXPECT_EQ(loads, 1);
}

TYPED_TEST(CacheFixture, ExplicitAddAndL1HitDoNotCallLoader)
{
    auto l1 = make_lru();
    auto l2 = make_lru();
    auto l3 = make_lru();
    int loads = 0;
    auto loader = [&loads](const int&) { ++loads; return -1; };
    auto cache = this->make_cache(loader, l1, l2, l3);

    cache.add(1, 111);
    const int* value = cache.get(1);
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(*value, 111);
    expect_level(l1, {1}, {});
    expect_level(l2, {}, {1});
    expect_level(l3, {}, {1});
    EXPECT_EQ(loads, 0);
}

TYPED_TEST(CacheFixture, AddCascadesThroughAllLevels)
{
    auto l1 = make_lru();
    auto l2 = make_lru();
    auto l3 = make_lru();
    int loads = 0;
    auto loader = [&loads](const int&) { ++loads; return -1; };
    auto cache = this->make_cache(loader, l1, l2, l3);

    cache.add(1, 111);
    cache.add(2, 222);
    cache.add(3, 333);

    expect_level(l1, {3}, {1, 2});
    expect_level(l2, {2}, {1, 3});
    expect_level(l3, {1}, {2, 3});
    EXPECT_TRUE(cache.contains(1));
    EXPECT_TRUE(cache.contains(2));
    EXPECT_TRUE(cache.contains(3));

    // Each LRU holds only one key, so these reads cannot change its order.
    const int* value_3 = l1.get(3);
    const int* value_2 = l2.get(2);
    const int* value_1 = l3.get(1);
    ASSERT_NE(value_3, nullptr);
    ASSERT_NE(value_2, nullptr);
    ASSERT_NE(value_1, nullptr);
    EXPECT_EQ(*value_3, 333);
    EXPECT_EQ(*value_2, 222);
    EXPECT_EQ(*value_1, 111);
    EXPECT_EQ(loads, 0);
}

TYPED_TEST(CacheFixture, L2HitPromotesWithoutLoadingOrDuplicating)
{
    auto l1 = make_lru();
    auto l2 = make_lru();
    auto l3 = make_lru();
    int loads = 0;
    auto loader = [&loads](const int&) { ++loads; return -1; };
    auto cache = this->make_cache(loader, l1, l2, l3);

    cache.add(1, 111);
    cache.add(2, 222);
    cache.add(3, 333);

    const int* value = cache.get(2);
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(*value, 222);
    expect_level(l1, {2}, {1, 3});
    expect_level(l2, {3}, {1, 2});
    expect_level(l3, {1}, {2, 3});
    EXPECT_EQ(loads, 0);
}

TYPED_TEST(CacheFixture, L3HitPromotesAndCascadesWithoutDuplicates)
{
    auto l1 = make_lru();
    auto l2 = make_lru();
    auto l3 = make_lru();
    int loads = 0;
    auto loader = [&loads](const int&) { ++loads; return -1; };
    auto cache = this->make_cache(loader, l1, l2, l3);

    cache.add(1, 111);
    cache.add(2, 222);
    cache.add(3, 333);

    const int* value = cache.get(1);
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(*value, 111);
    expect_level(l1, {1}, {2, 3});
    expect_level(l2, {3}, {1, 2});
    expect_level(l3, {2}, {1, 3});

    cache.add(4, 444);
    expect_level(l1, {4}, {1, 2, 3});
    expect_level(l2, {1}, {2, 3, 4});
    expect_level(l3, {3}, {1, 2, 4});
    EXPECT_FALSE(cache.contains(2));
    EXPECT_EQ(loads, 0);
}

TYPED_TEST(CacheFixture, LastLevelOverflowRemovesEntryAndNextGetReloads)
{
    auto l1 = make_lru();
    auto l2 = make_lru();
    auto l3 = make_lru();
    int loads = 0;
    auto loader = [&loads](const int& key)
    {
        EXPECT_EQ(key, 1);
        ++loads;
        return 999;
    };
    auto cache = this->make_cache(loader, l1, l2, l3);

    cache.add(1, 111);
    cache.add(2, 222);
    cache.add(3, 333);
    cache.add(4, 444);

    EXPECT_FALSE(cache.contains(1));
    expect_level(l1, {4}, {1, 2, 3});
    expect_level(l2, {3}, {1, 2, 4});
    expect_level(l3, {2}, {1, 3, 4});
    EXPECT_EQ(loads, 0);

    const int* value = cache.get(1);
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(*value, 999);
    EXPECT_EQ(loads, 1);
    expect_level(l1, {1}, {2, 3, 4});
    expect_level(l2, {4}, {1, 2, 3});
    expect_level(l3, {3}, {1, 2, 4});
    EXPECT_FALSE(cache.contains(2));
}

TYPED_TEST(CacheFixture, DuplicateAddIsIgnoredOnEveryLevel)
{
    for (int key : {1, 2, 3})
    {
        SCOPED_TRACE(testing::Message{} << "duplicate key=" << key);
        auto l1 = make_lru();
        auto l2 = make_lru();
        auto l3 = make_lru();
        int loads = 0;
        auto loader = [&loads](const int&) { ++loads; return -1; };
        auto cache = this->make_cache(loader, l1, l2, l3);

        cache.add(1, 111);
        cache.add(2, 222);
        cache.add(3, 333);
        cache.add(key, 999);

        expect_level(l1, {3}, {1, 2});
        expect_level(l2, {2}, {1, 3});
        expect_level(l3, {1}, {2, 3});

        const int* value = cache.get(key);
        ASSERT_NE(value, nullptr);
        EXPECT_EQ(*value, key * 111);

        cache.remove(key);
        EXPECT_FALSE(cache.contains(key));
        EXPECT_FALSE(l1.contains(key));
        EXPECT_FALSE(l2.contains(key));
        EXPECT_FALSE(l3.contains(key));
        EXPECT_EQ(loads, 0);
    }
}

TYPED_TEST(CacheFixture, RemoveWorksOnEveryLevelAndNextGetReloads)
{
    for (int key : {1, 2, 3})
    {
        SCOPED_TRACE(testing::Message{} << "removed key=" << key);
        auto l1 = make_lru();
        auto l2 = make_lru();
        auto l3 = make_lru();
        int loads = 0;
        auto loader = [&loads, key](const int& requested)
        {
            EXPECT_EQ(requested, key);
            ++loads;
            return 999;
        };
        auto cache = this->make_cache(loader, l1, l2, l3);

        cache.add(1, 111);
        cache.add(2, 222);
        cache.add(3, 333);
        cache.remove(key);

        EXPECT_FALSE(cache.contains(key));
        EXPECT_FALSE(l1.contains(key));
        EXPECT_FALSE(l2.contains(key));
        EXPECT_FALSE(l3.contains(key));
        EXPECT_EQ(l1.contains(3), key != 3);
        EXPECT_EQ(l2.contains(2), key != 2);
        EXPECT_EQ(l3.contains(1), key != 1);
        EXPECT_EQ(loads, 0);

        const int* value = cache.get(key);
        ASSERT_NE(value, nullptr);
        EXPECT_EQ(*value, 999);
        EXPECT_EQ(loads, 1);
        EXPECT_TRUE(l1.contains(key));
        EXPECT_FALSE(l2.contains(key));
        EXPECT_FALSE(l3.contains(key));
        EXPECT_TRUE(cache.contains(1));
        EXPECT_TRUE(cache.contains(2));
        EXPECT_TRUE(cache.contains(3));
    }
}

TYPED_TEST(CacheFixture, CascadeStopsAtVacantMiddleLevel)
{
    auto l1 = make_lru();
    auto l2 = make_lru();
    auto l3 = make_lru();
    int loads = 0;
    auto loader = [&loads](const int&) { ++loads; return -1; };
    auto cache = this->make_cache(loader, l1, l2, l3);

    cache.add(1, 111);
    cache.add(2, 222);
    cache.add(3, 333);
    cache.remove(2);
    cache.add(4, 444);

    expect_level(l1, {4}, {1, 2, 3});
    expect_level(l2, {3}, {1, 2, 4});
    expect_level(l3, {1}, {2, 3, 4});
    EXPECT_FALSE(cache.contains(2));
    EXPECT_EQ(loads, 0);
}

TYPED_TEST(CacheFixture, ContainsAndMissingRemoveDoNotChangeOrder)
{
    auto l1 = make_lru(2);
    auto l2 = make_lru(2);
    auto l3 = make_lru(2);
    int loads = 0;
    auto loader = [&loads](const int&) { ++loads; return -1; };
    auto cache = this->make_cache(loader, l1, l2, l3);

    for (int key = 1; key <= 6; ++key)
        cache.add(key, key * 111);

    // Touch the oldest key of each level through contains(), not get().
    EXPECT_TRUE(cache.contains(5));
    EXPECT_TRUE(cache.contains(3));
    EXPECT_TRUE(cache.contains(1));
    EXPECT_FALSE(cache.contains(999));
    cache.remove(999);

    expect_level(l1, {5, 6}, {1, 2, 3, 4, 999});
    expect_level(l2, {3, 4}, {1, 2, 5, 6, 999});
    expect_level(l3, {1, 2}, {3, 4, 5, 6, 999});

    cache.add(7, 777);
    expect_level(l1, {6, 7}, {1, 2, 3, 4, 5});
    expect_level(l2, {4, 5}, {1, 2, 3, 6, 7});
    expect_level(l3, {2, 3}, {1, 4, 5, 6, 7});
    EXPECT_FALSE(cache.contains(1));
    EXPECT_EQ(loads, 0);
}

TYPED_TEST(CacheFixture, SingleLevelSupportsMissHitEvictionAndReload)
{
    auto l1 = make_lru();
    int loads = 0;
    auto loader = [&loads](const int& key)
    {
        EXPECT_EQ(key, 1);
        return 100 + ++loads;
    };
    auto cache = this->make_cache(loader, l1);

    const int* first = cache.get(1);
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(*first, 101);
    EXPECT_EQ(loads, 1);

    const int* repeated = cache.get(1);
    ASSERT_NE(repeated, nullptr);
    EXPECT_EQ(*repeated, 101);
    EXPECT_EQ(loads, 1);

    cache.add(2, 222);
    expect_level(l1, {2}, {1});
    EXPECT_FALSE(cache.contains(1));
    EXPECT_EQ(loads, 1);

    const int* reloaded = cache.get(1);
    ASSERT_NE(reloaded, nullptr);
    EXPECT_EQ(*reloaded, 102);
    EXPECT_EQ(loads, 2);
    expect_level(l1, {1}, {2});
}


} // namespace

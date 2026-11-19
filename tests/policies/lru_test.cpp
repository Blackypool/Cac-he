#include <gtest/gtest.h>
#include <functional>

#include "my_cache/types.hpp"
#include "my_cache/policies/lru_policy.hpp"
#include "my_cache/errors.hpp"

namespace {

using Hash = std::hash<int>;
using namespace MyCache;

TEST(LruPolicy, HitChangesEvictionVictim)
{
    auto level = policy::LRU(2).build<int, int, Hash>(Hash{});

    level.add(CacheEntry<int, int>{1, 10});
    level.add(CacheEntry<int, int>{2, 20});

    const int* value = level.get(1);
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(*value, 10);

    auto result = level.add(CacheEntry<int, int>{3, 30});

    ASSERT_TRUE(result.evicted.has_value());
    EXPECT_EQ(result.evicted->entry.key, 2);
    EXPECT_EQ(result.evicted->entry.value, 20);
    
    ASSERT_NE  (result.inserted, nullptr);
    EXPECT_EQ(*(result.inserted), 30);

    EXPECT_TRUE (level.contains(1));
    EXPECT_FALSE(level.contains(2));
    EXPECT_TRUE (level.contains(3));
}

TEST(LruPolicy, OverflowEvictsLeastRecentlyUsed)
{
    auto level = policy::LRU(2).build<int, int, Hash>(Hash{});

    level.add(CacheEntry<int, int>{1, 10});
    level.add(CacheEntry<int, int>{2, 20});
    auto result = level.add(CacheEntry<int, int>{3, 30});

    ASSERT_TRUE(result.evicted.has_value());
    EXPECT_EQ(result.evicted->entry.key, 1);
    EXPECT_EQ(result.evicted->entry.value, 10);
    
    ASSERT_NE  (result.inserted, nullptr);
    EXPECT_EQ(*(result.inserted), 30);

    EXPECT_FALSE(level.contains(1));
    EXPECT_TRUE (level.contains(2));
    EXPECT_TRUE (level.contains(3));
}

TEST(LruPolicy, RejectsZeroCapacity)
{
    EXPECT_THROW(
        (policy::LRU(0).build<int, int, Hash>(Hash{})),
        configuration_error
    );

    EXPECT_NO_THROW(
        (policy::LRU(5).build<int, int, Hash>(Hash{}))
    );  
}

} // namespace

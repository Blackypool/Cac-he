#include <gtest/gtest.h>
#include <functional>

#include "my_cache/types.hpp"
#include "my_cache/policies/lfu_policy.hpp"
#include "my_cache/policies/lru_policy.hpp"
#include "my_cache/policies/lirs_policy.hpp"
#include "my_cache/policies/arc_policy.hpp"
#include "my_cache/policies/two_q_policy.hpp"
#include "my_cache/errors.hpp"
#include "test_fixtures.hpp"

namespace {

inline constexpr std::size_t common_level_capacity = 64;

using namespace Test;
using namespace MyCache;

using Policies = testing::Types<
    PolicyBuild<policy::LFU>,
    PolicyBuild<policy::LRU>,
    PolicyBuild<overload_cast<std::size_t>(&policy::ARC)>,
    PolicyBuild<overload_cast<std::size_t>(&policy::TwoQ)>,
    PolicyBuild<overload_cast<std::size_t>(&policy::LIRS)>
>;

TYPED_TEST_SUITE(PolicyFixture, Policies);

TYPED_TEST(PolicyFixture, RemoveMakesKeyAbsent)
{
    auto level = this->make_level(common_level_capacity);

    level.add(CacheEntry<int, int>{1, 10});
    level.add(CacheEntry<int, int>{2, 20});

    level.remove(1);
    EXPECT_FALSE(level.contains(1));
    EXPECT_TRUE (level.contains(2));

    level.remove(2);
    EXPECT_FALSE(level.contains(1));
    EXPECT_FALSE(level.contains(2));
}

TYPED_TEST(PolicyFixture, ExtractReturnsEntryAndRemovesIt)
{
    auto level = this->make_level(common_level_capacity);

    level.add(CacheEntry<int, int>{1, 10});
    level.add(CacheEntry<int, int>{2, 20});
    
    auto result1 = level.extract(1);

    ASSERT_TRUE(result1.has_value());
    EXPECT_EQ(result1->entry.key, 1);
    EXPECT_EQ(result1->entry.value, 10);
    EXPECT_FALSE(level.contains(1));
    EXPECT_TRUE(level.contains(2));

    auto result2 = level.extract(3);
    EXPECT_FALSE(result2.has_value());
}

TYPED_TEST(PolicyFixture, InsertWithoutOverflowDoesNotEvict)
{
    auto level = this->make_level(common_level_capacity);

    for (int i = 1; i <= 3; ++i)
    {
        SCOPED_TRACE(i);

        auto result = level.add(CacheEntry<int, int>{i, i * 10});

        EXPECT_FALSE(result.evicted.has_value());
        ASSERT_NE(result.inserted, nullptr);
        EXPECT_EQ(*result.inserted, i * 10);
        EXPECT_TRUE(level.contains(i));
    }

    EXPECT_TRUE(level.contains(1));
    EXPECT_TRUE(level.contains(2));
    EXPECT_TRUE(level.contains(3));
}

TYPED_TEST(PolicyFixture, RejectsZeroCapacity)
{
    EXPECT_THROW(
        this->make_level(0),
        configuration_error
    );

    EXPECT_NO_THROW(this->make_level(common_level_capacity));  
}

} // namespace


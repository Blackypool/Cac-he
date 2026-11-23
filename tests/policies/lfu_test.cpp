#include <gtest/gtest.h>
#include <functional>

#include "my_cache/types.hpp"
#include "my_cache/policies/lfu_policy.hpp"
#include "my_cache/errors.hpp"
#include "policy_test_tools.hpp"

namespace {

using Hash = std::hash<int>;
using namespace MyCache;
using Test::ExpectedAccess;
using Test::run_access_trace;
using enum ExpectedAccess;

auto make_lfu(std::size_t capacity)
{
    return policy::LFU(capacity)
        .build<int, int, Hash>(Hash{});
}

TEST(LfuPolicy, HitChangesEvictionVictim)
{
    auto level = make_lfu(3);

    ASSERT_NO_FATAL_FAILURE(
        run_access_trace(
            level, 
            { 
                {1, miss}, {2, miss}, {3, miss},
                {1, hit }, {2, hit }, {1, hit},
                {4, miss, 3}
            }
        );
    );

    EXPECT_TRUE (level.contains(1));
    EXPECT_TRUE (level.contains(2));
    EXPECT_FALSE(level.contains(3));
    EXPECT_TRUE (level.contains(4));
}

TEST(LfuPolicy, SameFreqEviction)
{
    auto level = make_lfu(3);

    ASSERT_NO_FATAL_FAILURE(
        run_access_trace(
            level, 
            { 
                {1, miss}, {2, miss}, {3, miss},
                {1, hit }, {1, hit },
                {4, miss, 2}
            }
        );
    );

    EXPECT_TRUE (level.contains(1));
    EXPECT_FALSE(level.contains(2));
    EXPECT_TRUE (level.contains(3));
    EXPECT_TRUE (level.contains(4));
}

TEST(LfuPolicy, OneCapacityEviction)
{
    auto level = make_lfu(1);

    ASSERT_NO_FATAL_FAILURE(
        run_access_trace(
            level, 
            { 
                {1, miss}, 
                {2, miss, 1}, 
                {3, miss, 2}
            }
        );
    );

    EXPECT_FALSE (level.contains(1));
    EXPECT_FALSE(level.contains(2));
    EXPECT_TRUE (level.contains(3));
}

TEST(LfuPolicy, ExtractIncreaseFreq)
{
    auto level = make_lfu(2);

    ASSERT_NO_FATAL_FAILURE(
        run_access_trace(
            level, 
            { 
                {1, miss}, {2, miss},
                {1, hit }, {1, hit }, {2, hit}
            }
        );
    );

    auto extract_1 = level.extract(1);
    ASSERT_TRUE(extract_1.has_value());
    EXPECT_EQ(extract_1->entry.value, 10);
    EXPECT_EQ(extract_1->entry.key, 1);
    EXPECT_EQ(extract_1->meta.freq, 4);
    EXPECT_FALSE (level.contains(1));

    auto extract_2 = level.extract(2);
    ASSERT_TRUE(extract_2.has_value());
    EXPECT_EQ(extract_2->entry.value, 20);
    EXPECT_EQ(extract_2->entry.key, 2);
    EXPECT_EQ(extract_2->meta.freq, 3);
    EXPECT_FALSE (level.contains(1));
    EXPECT_FALSE (level.contains(2));
}


TEST(LfuPolicy, SameFreqEvictionAfterHits)
{
    auto level = make_lfu(2);

    ASSERT_NO_FATAL_FAILURE(
        run_access_trace(
            level,
            {
                {1, miss}, {2, miss},
                {2, hit }, {1, hit },
                {3, miss, 2}
            }
        );
    );

    EXPECT_TRUE (level.contains(1));
    EXPECT_FALSE(level.contains(2));
    EXPECT_TRUE (level.contains(3));
}

TEST(LfuPolicy, ContainsDoesNotIncreaseFreq)
{
    auto level = make_lfu(2);

    ASSERT_NO_FATAL_FAILURE(
        run_access_trace(
            level,
            {
                {1, miss}, {2, miss}
            }
        );
    );

    EXPECT_TRUE(level.contains(1));
    EXPECT_TRUE(level.contains(1));

    ASSERT_NO_FATAL_FAILURE(
        run_access_trace(
            level,
            {
                {3, miss, 1}
            }
        );
    );

    EXPECT_FALSE(level.contains(1));
    EXPECT_TRUE (level.contains(2));
    EXPECT_TRUE (level.contains(3));
}

TEST(LfuPolicy, CompatibleMetaControlsEvictionAndPreservesFreq)
{
    auto level = make_lfu(2);

    auto result_1 = level.add(
        CacheTransfer<CacheEntry<int, int>, policy::LFUMeta>{{1, 10}, {5}}
    );
    ASSERT_FALSE(result_1.evicted.has_value());
    ASSERT_NE(result_1.inserted, nullptr);
    EXPECT_EQ(*result_1.inserted, 10);

    auto result_2 = level.add(
        CacheTransfer<CacheEntry<int, int>, policy::LFUMeta>{{2, 20}, {3}}
    );
    ASSERT_FALSE(result_2.evicted.has_value());
    ASSERT_NE(result_2.inserted, nullptr);
    EXPECT_EQ(*result_2.inserted, 20);

    auto result_3 = level.add(CacheEntry<int, int>{3, 30});
    ASSERT_TRUE(result_3.evicted.has_value());
    EXPECT_EQ(result_3.evicted->entry.key, 2);
    EXPECT_EQ(result_3.evicted->entry.value, 20);
    EXPECT_EQ(result_3.evicted->meta.freq, 3);
    ASSERT_NE(result_3.inserted, nullptr);
    EXPECT_EQ(*result_3.inserted, 30);

    EXPECT_TRUE (level.contains(1));
    EXPECT_FALSE(level.contains(2));
    EXPECT_TRUE (level.contains(3));
}

TEST(LfuPolicy, IncompatibleMetaStartsWithFreqOne)
{
    auto level = make_lfu(2);

    auto result = level.add(
        CacheTransfer<CacheEntry<int, int>, EmptyMeta>{{1, 10}, {}}
    );
    ASSERT_FALSE(result.evicted.has_value());
    ASSERT_NE(result.inserted, nullptr);
    EXPECT_EQ(*result.inserted, 10);

    auto extract_1 = level.extract(1);
    ASSERT_TRUE(extract_1.has_value());
    EXPECT_EQ(extract_1->entry.key, 1);
    EXPECT_EQ(extract_1->entry.value, 10);
    EXPECT_EQ(extract_1->meta.freq, 2);
    EXPECT_FALSE(level.contains(1));
}

TEST(LfuPolicy, RemoveMinimumFreqUpdatesEvictionVictim)
{
    auto level = make_lfu(3);

    ASSERT_NO_FATAL_FAILURE(
        run_access_trace(
            level,
            {
                {1, miss}, {2, miss}, {3, miss},
                {2, hit }, {2, hit },
                {3, hit }, {3, hit }, {3, hit}, {3, hit}
            }
        );
    );

    level.remove(1);
    EXPECT_FALSE(level.contains(1));

    auto result = level.add(
        CacheTransfer<CacheEntry<int, int>, policy::LFUMeta>{{4, 40}, {7}}
    );
    ASSERT_FALSE(result.evicted.has_value());
    ASSERT_NE(result.inserted, nullptr);
    EXPECT_EQ(*result.inserted, 40);

    ASSERT_NO_FATAL_FAILURE(
        run_access_trace(
            level,
            {
                {5, miss, 2}
            }
        );
    );

    EXPECT_FALSE(level.contains(1));
    EXPECT_FALSE(level.contains(2));
    EXPECT_TRUE (level.contains(3));
    EXPECT_TRUE (level.contains(4));
    EXPECT_TRUE (level.contains(5));
}

TEST(LfuPolicy, ExtractMinimumFreqUpdatesEvictionVictim)
{
    auto level = make_lfu(3);

    ASSERT_NO_FATAL_FAILURE(
        run_access_trace(
            level,
            {
                {1, miss}, {2, miss}, {3, miss},
                {2, hit }, {2, hit },
                {3, hit }, {3, hit }, {3, hit}, {3, hit}
            }
        );
    );

    auto extract_1 = level.extract(1);
    ASSERT_TRUE(extract_1.has_value());
    EXPECT_EQ(extract_1->entry.key, 1);
    EXPECT_EQ(extract_1->entry.value, 10);
    EXPECT_EQ(extract_1->meta.freq, 2);
    EXPECT_FALSE(level.contains(1));

    auto result = level.add(
        CacheTransfer<CacheEntry<int, int>, policy::LFUMeta>{{4, 40}, {7}}
    );
    ASSERT_FALSE(result.evicted.has_value());
    ASSERT_NE(result.inserted, nullptr);
    EXPECT_EQ(*result.inserted, 40);

    ASSERT_NO_FATAL_FAILURE(
        run_access_trace(
            level,
            {
                {5, miss, 2}
            }
        );
    );

    EXPECT_FALSE(level.contains(1));
    EXPECT_FALSE(level.contains(2));
    EXPECT_TRUE (level.contains(3));
    EXPECT_TRUE (level.contains(4));
    EXPECT_TRUE (level.contains(5));
}

TEST(LfuPolicy, RemoveAllThenRefillResetsState)
{
    auto level = make_lfu(2);

    ASSERT_NO_FATAL_FAILURE(
        run_access_trace(
            level,
            {
                {1, miss}, {2, miss},
                {1, hit }, {1, hit }, {2, hit}
            }
        );
    );

    level.remove(1);
    level.remove(2);
    EXPECT_FALSE(level.contains(1));
    EXPECT_FALSE(level.contains(2));

    ASSERT_NO_FATAL_FAILURE(
        run_access_trace(
            level,
            {
                {1, miss}, {2, miss},
                {3, miss, 1}
            }
        );
    );

    EXPECT_FALSE(level.contains(1));
    EXPECT_TRUE (level.contains(2));
    EXPECT_TRUE (level.contains(3));
}

} // namespace

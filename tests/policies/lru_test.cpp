#include <gtest/gtest.h>
#include <functional>

#include "my_cache/types.hpp"
#include "my_cache/policies/lru_policy.hpp"
#include "my_cache/errors.hpp"
#include "policy_test_tools.hpp"

namespace {

using Hash = std::hash<int>;
using namespace MyCache;
using Test::ExpectedAccess;
using Test::run_access_trace;
using enum ExpectedAccess;

auto make_lru(std::size_t capacity)
{
    return policy::LRU(capacity)
        .build<int, int, Hash>(Hash{});
}

TEST(LruPolicy, HitChangesEvictionVictim)
{
    auto level = make_lru(2);

    ASSERT_NO_FATAL_FAILURE(
        run_access_trace(
            level,
            {
                {1, miss}, {2, miss}, {1, hit},
                {3, miss, 2}
            }
        );
    );

    EXPECT_TRUE (level.contains(1));
    EXPECT_FALSE(level.contains(2));
    EXPECT_TRUE (level.contains(3));
}

TEST(LruPolicy, OverflowEvictsLeastRecentlyUsed)
{
    auto level = make_lru(2);

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

TEST(LruPolicy, OneCapacityEviction)
{
    auto level = make_lru(1);

    ASSERT_NO_FATAL_FAILURE(
        run_access_trace(
            level,
            {
                {1, miss}, {1, hit},
                {2, miss, 1}, {2, hit},
                {3, miss, 2}, {3, hit}
            }
        );
    );

    EXPECT_FALSE(level.contains(1));
    EXPECT_FALSE(level.contains(2));
    EXPECT_TRUE (level.contains(3));
}

TEST(LruPolicy, ContainsDoesNotChangeRecency)
{
    auto level = make_lru(2);

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

TEST(LruPolicy, RemoveThenInsertPreservesOrder)
{
    auto level = make_lru(3);

    ASSERT_NO_FATAL_FAILURE(
        run_access_trace(
            level,
            {
                {1, miss}, {2, miss}, {3, miss}
            }
        );
    );

    level.remove(2);
    EXPECT_FALSE(level.contains(2));

    ASSERT_NO_FATAL_FAILURE(
        run_access_trace(
            level,
            {
                {4, miss}, {5, miss, 1}
            }
        );
    );

    EXPECT_FALSE(level.contains(1));
    EXPECT_FALSE(level.contains(2));
    EXPECT_TRUE (level.contains(3));
    EXPECT_TRUE (level.contains(4));
    EXPECT_TRUE (level.contains(5));
}

TEST(LruPolicy, ExtractThenInsertPreservesOrder)
{
    auto level = make_lru(3);

    ASSERT_NO_FATAL_FAILURE(
        run_access_trace(
            level,
            {
                {1, miss}, {2, miss}, {3, miss}
            }
        );
    );

    auto extract_2 = level.extract(2);
    ASSERT_TRUE(extract_2.has_value());
    EXPECT_EQ(extract_2->entry.key, 2);
    EXPECT_EQ(extract_2->entry.value, 20);
    EXPECT_FALSE(level.contains(2));

    ASSERT_NO_FATAL_FAILURE(
        run_access_trace(
            level,
            {
                {4, miss}, {5, miss, 1}
            }
        );
    );

    EXPECT_FALSE(level.contains(1));
    EXPECT_FALSE(level.contains(2));
    EXPECT_TRUE (level.contains(3));
    EXPECT_TRUE (level.contains(4));
    EXPECT_TRUE (level.contains(5));
}

} // namespace

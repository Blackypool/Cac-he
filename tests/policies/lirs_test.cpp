#include <cstddef>
#include <functional>
#include <optional>

#include <gtest/gtest.h>

#include "my_cache/errors.hpp"
#include "my_cache/policies/lirs_policy.hpp"
#include "my_cache/types.hpp"
#include "policy_test_tools.hpp"

namespace {

using namespace MyCache;
using Test::ExpectedAccess;
using Test::run_access_trace;
using enum ExpectedAccess;
using Hash = std::hash<int>;

auto make_lirs(std::size_t capacity, std::size_t hir, std::size_t history)
{
    return policy::LIRS(capacity, hir, history)
        .build<int, int, Hash>(Hash{});
}

TEST(LirsPolicy, RejectsInvalidHIRCapacity)
{
    EXPECT_THROW(make_lirs(3, 0, 3), configuration_error);
    EXPECT_THROW(make_lirs(3, 4, 3), configuration_error);
    EXPECT_THROW(make_lirs(3, 3, 3), configuration_error);
    EXPECT_NO_THROW(make_lirs(3, 1, 0));
}

TEST(LirsPolicy, RejectsCapacityBelowTwo)
{
    for (std::size_t capacity : {0u, 1u})
    {
        SCOPED_TRACE(capacity);
        EXPECT_THROW(
            (policy::LIRS(capacity).build<int, int, Hash>(Hash{})),
            configuration_error
        );
        EXPECT_THROW(make_lirs(capacity, 1, 3), configuration_error);
    }
}

TEST(LirsPolicy, MinimumCapacitySupportsHitsAndEviction)
{
    auto level = policy::LIRS(2).build<int, int, Hash>(Hash{});
    run_access_trace(level, {
        {1, miss}, {2, miss}, {1, hit},
        {3, miss, 2}, {3, hit}, {4, miss, 1}
    });
}

TEST(LirsPolicy, ColdScanEvictsHIRAndPreservesLIR)
{
    auto level = make_lirs(3, 1, 3);
    // 1 and 2 fill LIR; cold entries compete for the single HIR slot.
    run_access_trace(level, {
        {1, miss}, {2, miss}, {3, miss},
        {4, miss, 3}, {5, miss, 4}, {6, miss, 5},
        {1, hit}, {2, hit}, {6, hit}
    });
}

TEST(LirsPolicy, ResidentHIRHitPromotesAndDemotesBottomLIR)
{
    auto level = make_lirs(3, 1, 3);
    // Hitting HIR 3 promotes it and demotes bottom LIR 1.
    run_access_trace(level, {
        {1, miss}, {2, miss}, {3, miss}, {3, hit},
        {4, miss, 1}, {2, hit}, {3, hit}
    });
}

TEST(LirsPolicy, LIRHitChangesWhichLIREntryIsDemoted)
{
    auto level = make_lirs(3, 1, 3);
    // Touching 1 makes 2 the bottom LIR before promoting 3.
    run_access_trace(level, {
        {1, miss}, {2, miss}, {3, miss}, {1, hit}, {3, hit},
        {4, miss, 2}, {1, hit}, {3, hit}
    });
}

TEST(LirsPolicy, GhostReloadIsMissAndPromotesToLIR)
{
    auto level = make_lirs(3, 1, 3);
    // 3 becomes a ghost. Reloading it promotes 3 and demotes 1;
    // existing HIR 4 is evicted first, then demoted 1 on the next miss.
    run_access_trace(level, {
        {1, miss}, {2, miss}, {3, miss}, {4, miss, 3},
        {3, miss, 4}, {5, miss, 1}, {2, hit}, {3, hit}
    });
}

TEST(LirsPolicy, HIROutsideStackNeedsTwoHitsToPromote)
{
    auto level = make_lirs(3, 1, 3);
    // Demoted 1 has been pruned from S. Its first hit only restores it
    // to S; its second hit promotes it and demotes 2.
    run_access_trace(level, {
        {1, miss}, {2, miss}, {3, miss}, {4, miss, 3},
        {3, miss, 4}, {1, hit}, {1, hit}, {5, miss, 2}
    });
}

TEST(LirsPolicy, SingleHitOutsideStackKeepsEntryHIR)
{
    auto level = make_lirs(3, 1, 3);
    run_access_trace(level, {
        {1, miss}, {2, miss}, {3, miss}, {4, miss, 3},
        {3, miss, 4}, {1, hit}, {5, miss, 1}
    });
}

TEST(LirsPolicy, DisabledHistoryPreventsGhostPromotion)
{
    auto level = make_lirs(3, 1, 0);
    // Without history, reloaded 3 stays HIR and is evicted by 5.
    run_access_trace(level, {
        {1, miss}, {2, miss}, {3, miss}, {4, miss, 3},
        {3, miss, 4}, {5, miss, 3}, {1, hit}, {2, hit}
    });
}

TEST(LirsPolicy, RemovingGhostPreventsItsPromotionOnReload)
{
    auto level = make_lirs(3, 1, 3);
    ASSERT_NO_FATAL_FAILURE(run_access_trace(level, {
        {1, miss}, {2, miss}, {3, miss}, {4, miss, 3}
    }));

    EXPECT_FALSE(level.contains(3));
    EXPECT_EQ(level.get(3), nullptr);
    level.remove(3);

    run_access_trace(level, {
        {3, miss, 4}, {5, miss, 3}, {1, hit}, {2, hit}
    });
}

TEST(LirsPolicy, ExtractLIRReturnsMetadataAndRefillsLIRPart)
{
    auto level = make_lirs(3, 1, 3);
    ASSERT_NO_FATAL_FAILURE(run_access_trace(level, {
        {1, miss}, {2, miss}, {3, miss}
    }));

    auto extracted = level.extract(1);
    ASSERT_TRUE(extracted.has_value());
    EXPECT_EQ(extracted->entry.key, 1);
    EXPECT_EQ(extracted->entry.value, 10);
    EXPECT_TRUE(extracted->meta.is_lir);
    EXPECT_FALSE(level.contains(1));

    // HIR 3 fills the LIR vacancy, so the next cold victim is 4.
    run_access_trace(level, {{4, miss}, {5, miss, 4}, {3, hit}});
}

TEST(LirsPolicy, ExtractHIRAndEvictionReportHIRMetadata)
{
    auto level = make_lirs(3, 1, 3);
    ASSERT_NO_FATAL_FAILURE(run_access_trace(level, {
        {1, miss}, {2, miss}, {3, miss}
    }));

    auto extracted = level.extract(3);
    ASSERT_TRUE(extracted.has_value());
    EXPECT_EQ(extracted->entry.key, 3);
    EXPECT_EQ(extracted->entry.value, 30);
    EXPECT_FALSE(extracted->meta.is_lir);
    EXPECT_FALSE(level.contains(3));

    level.add(CacheEntry<int, int>{4, 40});
    auto result = level.add(CacheEntry<int, int>{5, 50});
    ASSERT_TRUE(result.evicted.has_value());
    EXPECT_EQ(result.evicted->entry.key, 4);
    EXPECT_EQ(result.evicted->entry.value, 40);
    EXPECT_FALSE(result.evicted->meta.is_lir);
}

} // namespace

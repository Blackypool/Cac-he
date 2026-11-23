#pragma once

#include <cstddef>
#include <initializer_list>
#include <limits>
#include <optional>

#include <gtest/gtest.h>

#include "my_cache/types.hpp"

namespace Test {

enum class ExpectedAccess
{
    hit,
    miss
};

struct AccessStep
{
    int key;
    ExpectedAccess expected_access;
    std::optional<int> expected_evicted_key = std::nullopt;
};

// Test helper for levels with int keys/values. One step calls get(key),
// then add({key, key * 10}) only on a miss. Expectations are supplied by
// the test, independently of the policy implementation.
// ASSERT_* failures return from this helper. Use ASSERT_NO_FATAL_FAILURE
// at the call site if subsequent test code must not run after such a failure.
template<typename Level>
void run_access_trace(
    Level& level,
    std::initializer_list<AccessStep> trace
)
{
    std::size_t index = 0;

    for (const auto& step : trace)
    {
        SCOPED_TRACE(
            testing::Message{} << "step=" << index << ", key=" << step.key
        );
        ++index;

        // Keep the synthetic loader's multiplication well-defined.
        ASSERT_GE(step.key, std::numeric_limits<int>::min() / 10);
        ASSERT_LE(step.key, std::numeric_limits<int>::max() / 10);
        const int expected_value = step.key * 10;
        const bool expected_hit = step.expected_access == ExpectedAccess::hit;
        ASSERT_FALSE(expected_hit && step.expected_evicted_key.has_value())
            << "Invalid trace: a hit does not call add and cannot evict";

        const int* value = nullptr;
        ASSERT_NO_THROW(value = level.get(step.key));
        ASSERT_EQ(value != nullptr, expected_hit)
            << "Unexpected hit/miss result";

        if (value != nullptr)
        {
            EXPECT_EQ(*value, expected_value);
            EXPECT_TRUE(level.contains(step.key));
            continue;
        }

        using Result = decltype(level.add(MyCache::CacheEntry<int, int>{}));
        Result result{};
        ASSERT_NO_THROW(
            result = level.add(
                MyCache::CacheEntry<int, int>{step.key, expected_value}
            )
        );
        ASSERT_NE(result.inserted, nullptr);
        EXPECT_EQ(*result.inserted, expected_value);
        EXPECT_TRUE(level.contains(step.key));

        std::optional<int> actual_evicted_key;
        if (result.evicted)
        {
            actual_evicted_key = result.evicted->entry.key;
        }
        ASSERT_EQ(actual_evicted_key, step.expected_evicted_key)
            << "Unexpected eviction after add";

        if (result.evicted)
        {
            EXPECT_EQ(
                static_cast<long long>(result.evicted->entry.value),
                static_cast<long long>(*step.expected_evicted_key) * 10
            );
            EXPECT_FALSE(level.contains(*step.expected_evicted_key))
                << "Evicted key is still resident";
        }
    }
}

} // namespace Test

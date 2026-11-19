#include <gtest/gtest.h>
#include <functional>

#include "my_cache/types.hpp"
#include "my_cache/policies/lfu_policy.hpp"
#include "my_cache/errors.hpp"

namespace {

using Hash = std::hash<int>;
using namespace MyCache;

TEST(LfuPolicy, RejectsZeroCapacity)
{
    EXPECT_THROW(
        (policy::LFU(0).build<int, int, Hash>(Hash{})),
        configuration_error
    );

    EXPECT_NO_THROW(
        (policy::LFU(5).build<int, int, Hash>(Hash{}))
    );  
}

} // namespace
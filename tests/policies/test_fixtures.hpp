#pragma once

namespace Test {

using namespace MyCache;

template <typename Arg, typename Return>
constexpr auto overload_cast(Return (*func)(Arg))
{
    return func;
}

template<auto Policy>
struct PolicyBuild
{
    static constexpr auto make = Policy;
};

template <typename Policy>
class PolicyFixture : public testing::Test
{
    protected:

        using Key   = int;
        using Value = int;
        using Hash  = std::hash<Key>;
        using Entry = CacheEntry<Key, Value>;

    auto make_level(size_t capacity)
    {
        return Policy::make(capacity)
            .template build<Key, Value, Hash>(Hash{});
    }
};

} // namespace Test
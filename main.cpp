#include "my_cache/pbd.hpp"
// #include "my_cache/runtime.hpp"
#include "my_cache/policies/arc_policy.hpp"
#include "my_cache/policies/two_q_policy.hpp"


#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <random>


using Key = std::string;
using Value = int;
using Hash = std::hash<Key>;

struct Loader
{
    std::size_t* calls = nullptr;

    Value operator()(const std::string& key) const
    {
        if (calls != nullptr)
        {
            ++(*calls);
        }
        else
        {
            std::cout << "[loader] loading: "
                      << key
                      << '\n';
        }

        return static_cast<int>(key.size() * 100);
    }
};


std::string generate_random_string()
{
    static thread_local std::random_device rd;
    static thread_local std::mt19937 rng(rd());
    
    std::uniform_int_distribution<int> length_dist(3, 10);
    
    const std::string chars = 
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789";
        
    std::uniform_int_distribution<int> char_dist(0, chars.size() - 1);

    int length = length_dist(rng);
    
    std::string result;
    result.reserve(length);
    
    for (int i = 0; i < length; ++i)
    {
        result += chars[char_dist(rng)];
    }
    
    return result;
}

int main (void)
{
    auto cache = MyCache::pbd::make_cache<Key, Value> (
        Loader{},
        std::hash<Key>{},
        MyCache::policy::LRU(10),
        MyCache::policy::LRU(20),
        MyCache::policy::LRU(30)
    );

    for (int i = 0; i < 100; i++)
    {
        cache.add(generate_random_string(), i);
    }

    cache.dump(std::cout);

    // typename MyCache::runtime::CacheBuilder<Key, Value>
    // builder(Loader{});

    // builder.add_level(MyCache::policy::ARC(10));
    // builder.add_level(MyCache::policy::TwoQ(20));
    // builder.add_level(MyCache::policy::ARC(30));

    // auto cache = std::move(builder).build();

    // for (int i = 0; i < 100; i++)
    // {
    //     cache.add(generate_random_string(), i);
    // }

    // cache.dump(std::cout);

    return 0;
}

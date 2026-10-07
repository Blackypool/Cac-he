#include "M_cache.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>


struct Item
{
    std::string key;
    int value;
};


struct ItemKey
{
    std::string operator()(const Item& item) const
    {
        return item.key;
    }
};


struct Loader
{
    std::size_t* calls = nullptr;

    Item operator()(const std::string& key) const
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

        return Item{
            key,
            static_cast<int>(key.size() * 100)
        };
    }
};


using Value = Item;
using Key = std::string;
using Hash = std::hash<Key>;
using Extractor = ItemKey;
using Finder = Loader;


using Level =
    MyCache::LFUCacheLevel<
        Value,
        Key,
        Extractor,
        Hash
    >;


using CacheType =
    MyCache::Cache<
        Level,   // L1
        Level,   // L2
        Level,   // L3
        Value,
        Key,
        Extractor,
        Hash,
        Finder
    >;


#ifdef DUMP

void print_item(
    std::ostream& out,
    const Item& item,
    const MyCache::LFUMeta& meta
)
{
    out << "{"
        << item.key
        << ":"
        << item.value
        << ", freq="
        << meta.freq
        << "}";
}

#endif


void run_demo()
{
    CacheType cache(
        10,     // L1
        20,     // L2
        30,     // L3
        Extractor{},
        Hash{},
        Finder{}
    );


    auto add = [&](std::string key, int value)
    {
        std::cout << "[add] "
                  << key
                  << " = "
                  << value
                  << '\n';

        cache.add(Item{
            std::move(key),
            value
        });
    };


    auto get = [&](const std::string& key)
    {
        std::cout << "[get] "
                  << key
                  << '\n';

        if (Item* item = cache.get(key))
        {
            std::cout << "  result: "
                      << item->key
                      << " = "
                      << item->value
                      << '\n';
        }
        else
        {
            std::cout << "  result: not found\n";
        }
    };


    std::cout << "\n=== Initial inserts ===\n";

    add("alpha", 10);
    add("beta", 20);
    add("gamma", 30);
    add("delta", 40);
    add("epsilon", 50);


#ifdef DUMP
    cache.dump(std::cout, print_item);
#endif


    std::cout << "\n=== Repeated access ===\n";

    get("alpha");
    get("alpha");
    get("alpha");

    get("beta");
    get("gamma");


#ifdef DUMP
    cache.dump(std::cout, print_item);
#endif


    std::cout << "\n=== Duplicate insert ===\n";

    add("alpha", 999);


    std::cout << "\n=== Insert many elements ===\n";

    for (int i = 0; i < 45; ++i)
    {
        add(
            "key_" + std::to_string(i),
            i
        );
    }


#ifdef DUMP
    cache.dump(std::cout, print_item);
#endif


    std::cout << "\n=== Access old elements ===\n";

    get("alpha");
    get("key_10");
    get("key_20");
    get("key_44");


    std::cout << "\n=== Access missing element ===\n";

    get("unknown");


#ifdef DUMP
    cache.dump(std::cout, print_item);
#endif
}


#ifdef BENCH


template <typename Prepare, typename Operation>
void run_case(
    std::string_view name,
    std::size_t iterations,
    Prepare prepare,
    Operation operation
)
{
    std::size_t loader_calls = 0;

    CacheType cache(
        10,
        20,
        30,
        Extractor{},
        Hash{},
        Loader{&loader_calls}
    );


    // Подготовка выполняется до измерения.
    prepare(cache);

    loader_calls = 0;

    std::uint64_t state = 123456789;
    std::int64_t checksum = 0;


    const auto start =
        std::chrono::steady_clock::now();


    for (std::size_t i = 0; i < iterations; ++i)
    {
        operation(
            cache,
            i,
            state,
            checksum
        );
    }


    const auto finish =
        std::chrono::steady_clock::now();


    const auto elapsed =
        std::chrono::duration_cast<
            std::chrono::nanoseconds
        >(finish - start);


    const double seconds =
        static_cast<double>(elapsed.count())
        / 1'000'000'000.0;


    const double ns_per_operation =
        static_cast<double>(elapsed.count())
        / static_cast<double>(iterations);


    const double operations_per_second =
        static_cast<double>(iterations)
        / seconds;


    std::cout << '\n'
              << name
              << '\n'
              << "  operations: "
              << iterations
              << '\n'
              << "  time:       "
              << elapsed.count() / 1'000'000.0
              << " ms\n"
              << "  ns/op:      "
              << std::fixed
              << std::setprecision(2)
              << ns_per_operation
              << '\n'
              << "  ops/sec:    "
              << std::fixed
              << std::setprecision(0)
              << operations_per_second
              << '\n'
              << "  loader:     "
              << loader_calls
              << '\n'
              << "  checksum:   "
              << checksum
              << '\n';
}


void run_benchmark()
{
    constexpr std::size_t iterations = 500'000;
    constexpr std::size_t cold_iterations = 50'000;


    std::vector<std::string> hot_keys;
    std::vector<std::string> all_keys;
    std::vector<std::string> cold_keys;


    for (int i = 0; i < 10; ++i)
    {
        hot_keys.push_back(
            "hot_" + std::to_string(i)
        );
    }


    for (int i = 0; i < 500; ++i)
    {
        all_keys.push_back(
            "key_" + std::to_string(i)
        );
    }


    for (std::size_t i = 0; i < cold_iterations; ++i)
    {
        cold_keys.push_back(
            "cold_" + std::to_string(i)
        );
    }


    auto fill_hot = [&](CacheType& cache)
    {
        for (std::size_t i = 0; i < hot_keys.size(); ++i)
        {
            cache.add(Item{
                hot_keys[i],
                static_cast<int>(i)
            });
        }
    };


    auto fill_all = [&](CacheType& cache)
    {
        for (std::size_t i = 0; i < all_keys.size(); ++i)
        {
            cache.add(Item{
                all_keys[i],
                static_cast<int>(i)
            });
        }
    };


    run_case(
        "L1 hot hits",
        iterations,
        fill_hot,
        [&](CacheType& cache,
            std::size_t i,
            std::uint64_t&,
            std::int64_t& checksum)
        {
            const std::string& key =
                hot_keys[i % hot_keys.size()];


            if (Item* item = cache.get(key))
            {
                checksum += item->value;
            }
        }
    );


    run_case(
        "Three-level random workload",
        iterations,
        fill_all,
        [&](CacheType& cache,
            std::size_t,
            std::uint64_t& state,
            std::int64_t& checksum)
        {
            state =
                state * 6364136223846793005ULL + 1;


            const std::size_t index =
                (state >> 32) % all_keys.size();


            if (Item* item = cache.get(all_keys[index]))
            {
                checksum += item->value;
            }
        }
    );


    run_case(
        "Cold misses",
        cold_iterations,
        [](CacheType&)
        {
            // Кэш остаётся пустым.
        },
        [&](CacheType& cache,
            std::size_t i,
            std::uint64_t&,
            std::int64_t& checksum)
        {
            if (Item* item = cache.get(cold_keys[i]))
            {
                checksum += item->value;
            }
        }
    );
}


#endif


int main()
{
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);


#ifdef BENCH
    run_benchmark();
#else
    run_demo();
#endif


    return 0;
}
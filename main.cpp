#include "cache.hpp"
#include "lfu_policy.hpp"

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


std::string generate_random_string() {
    // Делаем генератор статическим, чтобы он не пересоздавался при каждом вызове функции
    static thread_local std::random_device rd;
    static thread_local std::mt19937 rng(rd());
    
    // Задаем распределение для длины строки (от 3 до 10)
    std::uniform_int_distribution<int> length_dist(3, 10);
    
    const std::string chars = 
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789";
        
    // Задаем распределение для случайного символа
    std::uniform_int_distribution<int> char_dist(0, chars.size() - 1);
    
    // Получаем случайную длину
    int length = length_dist(rng);
    
    std::string result;
    result.reserve(length); // Резервируем память для избежания лишних аллокаций
    
    // Генерируем строку
    for (int i = 0; i < length; ++i) {
        result += chars[char_dist(rng)];
    }
    
    return result;
}

using namespace MyCache;

int main (void)
{

    auto cache = make_cache<Key, Value> (
        Loader{},
        std::hash<Key>{},
        LFU(10),
        LFU(20),
        LFU(30)
    );

    for (int i = 0; i < 100; i++)
    {
        cache.add(generate_random_string(), i);
    }

    cache.dump(std::cout);

    return 0;
}
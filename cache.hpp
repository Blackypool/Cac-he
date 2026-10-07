#ifndef CACHE_HH
#define CACHE_HH

#include <utility>
#include <optional>
#include <list>
#include <unordered_map>
#include <concepts>
#include <limits>

#include <algorithm>
#include <ostream>
#include <vector>

namespace MyCache {

template <typename Value, typename Meta>
struct CacheTransfer
{
    Value value;
    Meta meta;
};

struct EmptyMeta {};

// ====================================================== main cache class ====================================================
template <
    class Level1, 
    class Level2, 
    class Level3, 
    typename Value, 
    typename Key, 
    typename Extractor, 
    typename Hash,
    typename Finder
>
class Cache  
{
    private:

        Extractor get_key_;
        Finder  find_data_;

        Level1 L1_;
        Level2 L2_;
        Level3 L3_;

        template <typename AnyMeta>
        Value* add_circle(CacheTransfer<Value, AnyMeta> insert)
        {
            auto l1_info = L1_.add(std::move(insert));
            auto go_l2 = std::move(l1_info.evicted);

            if (go_l2.has_value())
            {
                auto l2_info = L2_.add(std::move(go_l2.value()));
                auto go_l3 =std::move(l2_info.evicted);

                if (go_l3.has_value())
                {
                    L3_.add(std::move(go_l3.value()));
                }
            }  
            
            return l1_info.inserted;
        }

        Value* add_circle(Value insert)
        {
            return add_circle (
                CacheTransfer<Value, EmptyMeta> {
                    std::move(insert), {}
                }
            );
        }

    public:

        Cache(size_t size_L1, size_t size_L2, size_t size_L3,
              Extractor get_key, Hash hash_func, Finder find_data) 
            : get_key_(get_key),
              find_data_(find_data),
              L1_ (size_L1, get_key, hash_func),
              L2_ (size_L2, get_key, hash_func),
              L3_ (size_L3, get_key, hash_func)
        {}

        void add(Value value)
        {
            Key key = get_key_(value);

            if (L1_.find(key) ||
                L2_.find(key) ||
                L3_.find(key)) 
            {
                return;
            }

            add_circle(value);
        }

        Value* get(Key key)
        {
            if (Value* val = L1_.get(key)) 
            {
                return val;
            }
            if (auto val = L2_.extract(key))
            {
                return add_circle(std::move(*val));
            }
            if (auto val = L3_.extract(key))
            { 
                return add_circle(std::move(*val));
            }

            Value val = find_data_(key);
            return add_circle(std::move(val));
        }
};


}

#endif // CACHE_HH
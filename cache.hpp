#ifndef CACHE_HH
#define CACHE_HH

#include <utility>
#include <optional>
#include <list>
#include <unordered_map>

#include <algorithm>
#include <ostream>
#include <vector>
#include <tuple>

#include "types.h"

namespace MyCache {

// ====================================================== main cache class ====================================================
template <
    typename Key, 
    typename Value, 
    typename Loader,
    typename ... Policies
>
class Cache  
{
    private:

        Loader load_;
        std::tuple<Policies ...> levels_;

        template <std::size_t I, typename Transfer>
        const Value* push_down(Transfer insert)
        {
            auto& current = std::get<I>(levels_);

            auto res = current.add(std::move(insert));
    
            const Value* inserted = res.inserted;

            if constexpr (I + 1 < sizeof...(Policies))
            {
                if (res.evicted.has_value())
                {
                    push_down<I + 1>(std::move(*res.evicted));
                }
            }

            return inserted;
        }

        const Value* add_new(Key key, Value value)
        {
            return push_down<0>(
                CacheTransfer<Key, Value, EmptyMeta> {
                    std::move(key),
                    std::move(value), 
                    {}
                }
            );
        }

        bool contains(const Key& key) const
        {
            return std::apply (
                [&](const auto& ... levels)
                {
                    return (levels.contains(key) || ...);
                },
                levels_
            );
        }

        template <std::size_t I>
        const Value* get_from_low_levels(const Key& key)
        {
            if constexpr (I < sizeof...(Policies))
            {
                auto& level = std::get<I>(levels_);

                if (auto transfer = level.extract(key))
                {
                    return push_down<0>(std::move(*transfer));
                }

                return get_from_low_levels<I + 1>(key);
            }

            else
            {
                Value value = load_(key);
                return add_new (
                    std::move(key),
                    std::move(value)
                );
            }
        }
        
        template <std::size_t I>
        void level_dump(std::ostream& out) const
        {
            if constexpr (I < sizeof ... (Policies))
            {
                auto& level = std::get<I>(levels_);

                out<< "\n\n========= " << "L"<< I + 1 << " dump" << " =========\n\n";
                level.dump(out);
                out<< "\n===========================\n\n";

                level_dump<I + 1>(out);
            }
        }

    public:

        Cache(Loader load, Policies ... policies) 
            :   load_(load),
                levels_(std::move(policies) ...)
        {}

        void add(Key key, Value value)
        {
            if (contains(key))
            {
                return;
            }
            
            add_new(
                std::move(key),
                std::move(value)
            );
        }

        const Value* get(Key key)
        {
            auto& L1 = std::get<0>(levels_);

            if (const Value* val = L1.get(key)) 
            {
                return val;
            }

            return get_from_low_levels<1>(key);
        }

        void dump(std::ostream& out) const
        {
            out << "\n========== CACHE ==========\n";

            level_dump<0>(out);

            out << "\n===========================\n";
        }
};


} // close MyCache

#include "factory.h"

#endif // CACHE_HH
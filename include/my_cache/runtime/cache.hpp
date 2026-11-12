#pragma once

#include "../types.hpp"
#include "../errors.hpp"
#include "cache_level.hpp"

#include <cstddef>
#include <functional>
#include <memory>
#include <ostream>
#include <utility>
#include <vector>

namespace MyCache::runtime {

template <typename Key, typename Value>
class Cache
{
    public:

        using Loader = std::function<Value(const Key&)>;
        using Level  = ICacheLevel<Key, Value>;
        using LevelPtr = std::unique_ptr<Level>;
        using Entry = CacheEntry<Key, Value>;

    private:

        std::vector<LevelPtr> levels_;
        Loader loader_;

        const Value* push_down(Entry current)
        {
            const Value* inserted = nullptr;

            for (std::size_t i = 0; i < levels_.size(); ++i)
            {
                auto result = levels_[i]->add(std::move(current));

                if (i == 0)
                {
                    inserted = result.inserted;
                }

                if (!result.evicted)
                {
                    return inserted;
                }

                current = std::move(*result.evicted);
            }

            return inserted;
        }

        const Value* add_new(Key key, Value value)
        {
            return push_down(
                Entry{
                    std::move(key),
                    std::move(value)
                }
            );
        }
    
    public:
        
        Cache(Loader loader, std::vector<LevelPtr> levels)
        :   levels_(std::move(levels)),
            loader_(std::move(loader))
        {
            if (!loader_)
            {
                throw configuration_error{"Cache loader must not be empty"};
            }
            if (levels_.empty())
            {
                throw configuration_error{"Cache must contain at least one level"};
            }
            for (const auto& level : levels_)
            {
                if (!level)
                {
                    throw configuration_error{"Cache level must not be null"};
                }
            }
        }

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

        const Value* get(const Key& key)
        {
            if (const Value* value = levels_[0]->get(key))
            {
                return value;
            }

            for (std::size_t i = 1; i < levels_.size(); ++i)
            {
                auto entry = levels_[i]->extract(key);

                if (entry)
                {
                    return push_down(
                        std::move(*entry)
                    );
                }
            }

            Value value = loader_(key);

            return add_new(
                key,
                std::move(value)
            );
        }

        bool contains(const Key& key) const
        {
            for (const auto& level : levels_)
            {
                if (level->contains(key))
                    return true;
            }

            return false;
        }

        void remove(const Key& key)
        {
            for (auto& level : levels_)
            {
                if (level->contains(key))
                {
                    level->remove(key);
                    return;
                }
            }
        }

        void dump(std::ostream& out) const
        {
            out << "\n========== CACHE ==========\n";

            for (std::size_t i = 0; i < levels_.size(); i++)
            {   
                out<< "\n\n========= " << "L"<< i + 1 << " dump" << " =========\n\n";
                levels_[i]->dump(out);
            }

            out << "\n===========================\n";
        }

        ~Cache() = default;
};

} // MyCache::runtime

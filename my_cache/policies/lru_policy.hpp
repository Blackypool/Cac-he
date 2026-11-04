#pragma once

#include "../level_spec.hpp"
#include "../types.hpp"
#include "../errors.hpp"

#include <cstddef>
#include <utility>
#include <optional>
#include <list>
#include <unordered_map>
#include <concepts>
#include <limits>

#include <algorithm>
#include <ostream>
#include <stdexcept>
#include <vector>

namespace MyCache::policy {

struct LRUTag {};
struct LRUMeta
{
    using compatibility = LRUTag; 
};

struct OptionsLRU
{
    std::size_t capacity;
};

// ====================================================== LRU cachelevel class ===================================================
template <
    typename Key, 
    typename Value, 
    typename Hash
>
class LRUCacheLevel
{
    public:
        
        using Entry = CacheEntry<Key, Value>;
        using Transfer = CacheTransfer<Entry, LRUMeta>;

    private:

        std::size_t capacity_;

        struct NodeLRU
        {
            Key key;
            Value value;
        };

        std::list <NodeLRU> hot_list_;
        std::unordered_map <Key, typename std::list<NodeLRU>::iterator, Hash> h_table_;  // Ht -- vector of lists with {it, hash}

    //_________________________________________________________________________________________________________________________________________//
    public:

        LRUCacheLevel(OptionsLRU options, Hash hash_func)
        :   capacity_(options.capacity),
            h_table_(0, hash_func)
        {
            if (capacity_ == 0)
            {
                throw configuration_error(
                    "LRU cache capacity must be positive"
                );
            }
            h_table_.reserve(capacity_);
        } 

        template <typename AnyMeta>
        AddResult<Transfer, Value> add(CacheTransfer<Entry, AnyMeta> transfer)
        {     
            std::optional<Transfer> evicted = std::nullopt;
   
            if (hot_list_.size() >= capacity_)
            {
                auto it_last = std::prev(hot_list_.end());  // check it of last in list

                h_table_.erase(it_last->key);  // delete last in Ht
                evicted = Transfer {
                    Entry {
                        std::move(it_last->key),
                        std::move(it_last->value)
                    },
                    {}
                };

                hot_list_.pop_back();          // delete in lidt
            }

            hot_list_.push_front( 
                NodeLRU {
                    std::move(transfer.entry.key), 
                    std::move(transfer.entry.value)
                }
            );

            auto inserted_it = hot_list_.begin();
            h_table_.emplace(inserted_it->key, inserted_it);  // add new it in Ht
            
            return {std::move(evicted), &inserted_it->value};
        }

        AddResult<Transfer, Value> add(Entry entry)
        {   
            return add(
                Transfer {
                    std::move(entry),
                    {/* empty meta */}
                }
            );
        }

        const Value* get(const Key& key)
        {
            auto it = h_table_.find(key);
            if (it == h_table_.end())  // cache miss
                return nullptr;

            hot_list_.splice(
                hot_list_.begin(), 
                hot_list_, it->second
            );
            return &((it->second)->value);
        }

        bool contains(const Key& key) const
        {
            auto it = h_table_.find(key);
            if (it == h_table_.end())  // cache miss
                return false;

            return true;
        }

        void remove(const Key& key)
        {
            auto it = h_table_.find(key);
            if (it == h_table_.end())  // removed before
                return;

            hot_list_.erase(it->second);
            h_table_.erase(it);
        }

        std::optional<Transfer> extract(const Key& key)
        {
            auto it = h_table_.find(key);
            if (it == h_table_.end())  // removed before
                return std::nullopt;

            Transfer result{
                Entry {
                    std::move(it->second->key),
                    std::move(it->second->value)
                },
                LRUMeta{}
            };

            hot_list_.erase(it->second);
            h_table_.erase(it);

            return result;
        }

        void dump(std::ostream& out) const
        {
            out << "  [ LRU Policy State ]\n"
                << "    Size     : " << hot_list_.size()
                << '/' << capacity_ << '\n';

            if (hot_list_.empty())
            {
                out << "    (Level is empty)\n";
                return;
            }

            out << "\n  [ LRU Queue (MRU -> LRU) ]\n"
                << "  --------------------------------\n";

            int index = 1;
            for (const auto& node : hot_list_) 
            {
                out << "      " << index++ << ". [" << node.key << "]\n";
            }
            
            out << "  --------------------------------\n";
        }

};

inline LevelSpec<LRUCacheLevel, OptionsLRU> LRU(std::size_t capacity)
{
    return level<LRUCacheLevel, OptionsLRU>(OptionsLRU(capacity));
}

} // MyCache::policy

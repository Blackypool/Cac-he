#ifndef LRU_POLICY_HH
#define LRU_POLICY_HH

#include <utility>
#include <optional>
#include <list>
#include <unordered_map>
#include <concepts>
#include <limits>

#include <algorithm>
#include <ostream>
#include <vector>

#include "types.h"

namespace MyCache {

struct LRUTag {};
struct LRUMeta
{
    using compatibility = LRUTag; 
};

// ====================================================== LRU cachelevel class ===================================================
template <
    typename Value, 
    typename Key, 
    typename Extractor, 
    typename Hash
>
class LRUCacheLevel
{
    private:

        size_t size_;
        Extractor get_key_;

        struct Node_LRU_
        {
            Key key;
            Value value;
        };

        std::list <Node_LRU_> hot_list_;
        std::unordered_map <Key, typename std::list<Node_LRU_>::iterator, Hash> h_table_;  // Ht -- vector of lists with {it, hash}

    //_________________________________________________________________________________________________________________________________________//
    public:

        struct AddResult 
        {
            std::optional <CacheTransfer<Value, LRUMeta>> evicted;  // выкинутый <value + meta>
            Value* inserted;
        };

        template <typename AnyMeta>
        AddResult add(CacheTransfer<Value, AnyMeta> elem)
        {     
            /////////////////INIT///////////////////
            Key key = get_key_(elem.value);
            std::optional<CacheTransfer<Value, LRUMeta>> evicted = std::nullopt;
            ////////////////////////////////////////


            //////////////CHECK_SIZE////////////////    
            if (hot_list_.size() >= size_)
            {
                typename std::list<Node_LRU_>::iterator it_last = std::prev(hot_list_.end());  // check it of last in list
                evicted = CacheTransfer<Value, LRUMeta>{std::move(it_last->value), {}};
                
                h_table_.erase(it_last->key);  // delete last in Ht  
                hot_list_.pop_back();          // delete in lidt
            }
            ////////////////////////////////////////


            //////////////////ADD///////////////////
            hot_list_.push_front(Node_LRU_{key, std::move(elem.value)});  // add new value in list

            auto inserted_it = hot_list_.begin();
            h_table_.emplace(key, inserted_it);  // add new it in Ht
            ////////////////////////////////////////
            
            return {std::move(evicted), &inserted_it->value};
        }

        AddResult add(Value value)
        {   
            return add<LRUMeta>({std::move(value), {}});
        }

        const Value* get (const Key& key)
        {
            auto it = h_table_.find(key);
            if (it == h_table_.end())  // cache miss
                return nullptr;

            hot_list_.splice(hot_list_.begin(), hot_list_, it->second);  // move to head of list
            return &((it->second)->value);
        }

        bool find (const Key& key)
        {
            auto it = h_table_.find(key);
            if (it == h_table_.end())  // cache miss
                return false;

            return true;
        }

        void remove (const Key& key)
        {
            auto it = h_table_.find(key);
            if (it == h_table_.end())  // removed before
                return;

            hot_list_.erase(it->second);
            h_table_.erase(it);
        }

        std::optional<CacheTransfer<Value, LRUMeta>> extract(const Key& key)
        {
            auto it = h_table_.find(key);
            if (it == h_table_.end())  // removed before
                return std::nullopt;

            CacheTransfer result{std::move(it->second->value), LRUMeta{}};
            hot_list_.erase(it->second);
            h_table_.erase(it);

            return result;
        }

    //_________________________________________________________________________________________________________________________________________//

        LRUCacheLevel (size_t size_of_cache, Extractor key, Hash hash_func) : size_(size_of_cache), get_key_(key)
        {
            if (size_ < 1)
                throw std::invalid_argument("need size of cache > 0");
        } 
        ~LRUCacheLevel() = default;
};


}

#endif // LRU_POLICY_HH
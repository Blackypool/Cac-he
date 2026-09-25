#ifndef CACHE_HH
#define CACHE_HH

#include "Header.h"
#include <utility>
#include <optional>
#include <unordered_map>

// =================================================== LRU cachelevel class ================================================
template <typename Value, typename Key, typename Extractor, typename Hash>
class LFUCacheLevel
{   
    private:
    
        struct ListNode
        {
            size_t freq;
            Value value;
        };

        using UMD = std::list<ListNode>;
        using UMP = std::list<ListNode>::iterator;
        
        size_t cur_size_ = 0;
        size_t min_freq_ = 0;
        size_t max_size_;
        Extractor get_key_;

        std::unordered_map <Key, UMP> table_p_;
        std::unordered_map <size_t, UMD> table_d_;

        Value replace_elem()
        {
            UMD& list = table_d_[min_freq_];
            //  ==== check =====
            auto it_d = --list.end();

            Key key = get_key_(it_d->value);
            Value go_up = std::move(it_d->value);

            table_p_.erase(key);
            table_d_[min_freq_].erase(it_d);

            return go_up;
        }

    public:

        LFUCacheLevel (size_t size, Extractor key, Hash hash_func)  
            : max_size_(size),
              get_key_ (key) ,
              table_p_ (size),
              table_d_ (size) 
        {}

        std::optional<Value> add(Value value)
        {   
            ListNode new_elem = {1, std::move(value)};
            auto& list = table_d_[1];

            if (cur_size_ >= max_size_)
            {
                Value go_up = replace_elem();
                list.push_front(new_elem);

                UMP it = list.begin();
                Key key = get_key_(it->value);

                table_p_.emplace(key, it);
                return go_up;
            }
            
            list.push_front(new_elem);
            UMP it  = list.begin();
            Key key = get_key_(it->value);

            table_p_.emplace(it);
            
            min_freq_ = 1;
            cur_size_++;
            return std::nullopt;
        }

        bool find(const Key& key)
        {
            return table_p_.find(key) != table_p_.end();
        }

        std::optional extractor(const Key& key)
        {
            auto it_p = table_p_.find(key);
            if (it_p != table_p_.end())
            {
                return std::nullopt;
            }

            auto it_d = it_p->second;
            Value extract = std::move(it_d->value);

            table_p_.erase(it_p);
            table_d_[it_d->freq].erase(it_d);
            --cur_size_;

            return extract;
        }

        Value* get(const Key& key)
        {
            auto it_p = table_p_.find(key);
            if (!it_p == table_p_.end()) 
            {
                return nullptr;
            } 
            
            UMP it_d = it_p->second;
            
            auto& list_from = table_d_[it_d->freq];
            auto& list_to   = table_d_[++it_d->freq]

            list_to[freq].splice(
                list_to.begin(),
                list_from,
                it_d
            );

            return &(it_d->value);
        }

        void remove(const Key& key)
        {
            auto it_p = table_p_.find(key);
            if (it_p == table_p_.end()) return;

            UMP it_d = it_p->value;
            
            table_p_.erase(it_p);
            table_d_.erase(it_d);
            
            cur_size_--;
        }
};


// ===================================================== main cache class ===================================================
template <
    typename CacheLevel, 
    typename Value, 
    typename Key, 
    typename Extractor, 
    typename Hash,
    typename Finder
> class Cache  
{
    private:

        Extractor get_key_;
        Finder  find_data_;

        CacheLevel<Value, Key, Extractor, Hash> L1_;
        CacheLevel<Value, Key, Extractor, Hash> L2_;
        CacheLevel<Value, Key, Extractor, Hash> L3_;

        void add_circle (Value value)
        {
            auto go_L2 = L1_.add(std::move(value));
            if (go_L2.has_value())
            {
                auto go_L3 = L2_.add(std::move(go_L2.value()));

                if (go_L3.has_value())
                {
                    L3_.add(std::move(go_L3.value()));
                    return;
                }
            }      
        }

    public:

        Cache(size_t size_L1, size_t size_L2, size_t size_L3,
              Extractor get_key, Hash hash_func, ) 
            : get_key_(get_key),
              L1_ (size_L1, key, hash_func),
              L2_ (size_L1, key, hash_func),
              L3_ (size_L1, key, hash_func)
        {}

        void add(Value value)
        {
            Key key = get_key_(value);

            if (L1_.find(key) ||
                L2_.find(key) ||
                L3_.find(key)  ) 
            {
                return;
            }

            add_circle (value);
        }

        Value* get(Key key)
        {
            if (Value* val = L1_.get(key)) 
            {
                return val;
            }
            if (auto val = L2_.extract(key))
            {
                add_circle(std::move(*val));
                return L1.get(key);
            }
            if (auto val = L3_.extract(key))
            {
                add_circle(std::move(*val));
                return L1_.get(key);
            }

            Value val = find_value_(key);
            add_circle(std::move(val));

            return L1_.get(key);
        }
};

#endif // CACHE_HH
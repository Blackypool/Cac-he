#ifndef CACHE_HH
#define CACHE_HH

#include "Header.h"
#include <utility>
#include <optional>
#include <unordered_map>

// ===================================================== hash table class ====================================================
// template <typename Value, typename Key, typename Extractor, typename Hash>
// class HashTable 
// {
//     private:

//         struct HashEntry
//         {
//             Value value;
//             size_t hash;
//         };

//         std::vector<std::list<HashEntry>> table_;
//         Extractor get_key_;
//         Hash hash_func_;

//     public:
        
//         using Iterator = typename std::list<Value>::iterator;

//         HashTable (size_t size, Extractor get_key, Hash hash_func) 
//                 : table_(size), 
//                   get_key_(get_key),
//                   hash_func_(hash_func)
//         {}

//         Iterator add(Value value) 
//         {
//             Key key = get_key_(value);
//             size_t hash = hash_func_(key);
//             size_t index = hash % table_.size();
            
//             table_[index].push_front({std::move(value), hash});
//             size_++;
//             return table_[index].begin();
//         }

//         std::optional<Iterator> find(const Key& key) 
//         {
//             size_t hash = hash_func_(key);
//             size_t index = hash % table_.size();
//             auto&  list = table_[index].

//             for (auto it = list.begin(); it != list.end(); ++it) 
//             {
//                 if (it->hash == hash && get_key_(it->value) == key) 
//                 {
//                     return it;  
//                 }
//             }

//             return std::nullopt;
//         }

//         Value* get(const Key& key) 
//         {
//             auto opt_it = find(key);
            
//             if (opt_it.has_value())
//                 return &(opt_it.value()->value);  

//             return nullptr;
//         }

//         void erase_by_it (Iterator it) 
//         {
//             Key key = get_key_(*it);
//             size_t hash = hash_func_(key);
//             size_t index = hash % table_.size();
            
//             table_[index].erase(it);
//         }

//         void remove (const Key& key) 
//         {
//             auto opt_it = find(key);
//             if (opt_it.has_value())
//             {
//                 erase_by_it(opt_it.value());
//             }
//         }

//         size_t size () 
//         {
//             return size_;
//         }
// };

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
template <typename CacheLevel, typename Value, typename Key, typename Extractor, typename Hash>
class Cache  
{
    private:

        Extractor get_key_;

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
              Extractor get_key, Hash hash_func) 
            : get_key_(get_key),
              L1_ (size_L1, key, hash_func),
              L2_ (size_L1, key, hash_func),
              L3_ (size_L1, key, hash_func)
        {}

        void add (Value value)
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

        value* get (Key key)
        {
            Value* value = L1_.get(key);
            if (value) 
            {
                return value;
            }
            else
            {
                value = L2_.get(key);
                if (value)
                {
                    value new_value = std::move(*value);
                    add_circle (*value);
                    L2_.remove (key);
                }
                else
                {
                    value = L3_.get(key);
                    add_circle (value);
                    L3_.remove (key);
                }
            }

            return value;
        }
};

#endif // CACHE_HH
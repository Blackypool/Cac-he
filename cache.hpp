#ifndef CACHE_HH
#define CACHE_HH

#include <cstddef>
#include <vector>
#include <list>

template <typename Value, typename Key, typename Extractor>
class HashTable {
    private:

        std::vector<std::list<Value>> table_;
        Extractor get_key_;

        size_t hash_function (const Key& key) const;

    public:
        
        HashTable (size_t size, Extractor get) 
                : table_(size), get_key_(get) {}

        void add (const Value& value) {
            Key key = get_key_(value);
            size_t index = hash_function(key) % table_.size();
            table_[index].push_back (value);
        }

        const Value* get (const Key& key) const {
            size_t index = hash_function (key) % table_.size();

            for (const auto& elem : table_[index]) {
                if (get_key_(elem) == key)
                    return &elem;
            }

            return nullptr;
        }

        void remove (const Key& key) {
            size_t index  = hash_function (key) % table_.size();
            auto& list = table_[index];

            for (auto it = list.begin(); it != list.end(); ++it) {
                if (key == get_key_(*it)) {
                    list.erase(it);
                    return;
                }
            }
        }
};

template <typename Value, typename Key, typename Extractor>
class CacheLevel {
    private:

        size_t size_;
        HashTable<Key, Value, Extractor> table_;
    
    public:
        CacheLevel  (size_t size, Extractor key) 
            : size_(size), table_(size, key) {}

        put (Value Value);
        get (Key Key);
        Remove (Key key);    
};

template <typename Value, typename Key, typename Extractor>
class Cache  {
    private:

        CacheLevel<Key, Value, Extractor> L1_;
        CacheLevel<Key, Value, Extractor> L2_;
        CacheLevel<Key, Value, Extractor> L3_;

    public:
        Cache  (size_t size_L1, size_t size_L2, size_t size_L3);
        ~Cache ();
};

#endif // CACHE_HH
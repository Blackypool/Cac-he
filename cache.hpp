#ifndef CACHE_HH
#define CACHE_HH

#include <cstddef>
#include <vector>
#include <list>

template <typename Value, typename Key, typename Extractor>
class HashTable 
{
    private:

        size_t size_;
        std::vector<std::list<Value>> table_;
        Extractor get_key_;

        int hash_function (Key key) const;

    public:
        
        HashTable  (size_t size) : size_(size);
        ~HashTable ();

        void add (const Value& value) {
            Key key = get_key_(value);
            int index = hash_function(key) % size_;
            table_[index].push_back (value);
        }

        const Value* get (const Key& key) const {
            int index = hash_function (key) % size_;
            for (const auto& elem : table_[index]) {
                Key elem_key = get_key_(elem);
                if (get_key_(elem) == key)
                    return &elem
            }

            return nullptr;
        }

        void remove (const Key& key) {
            int index  = hash_function (key) % size_;
            auto& list = table_[index];

            for (auto it = list.begin(); it != list.end(); it++) {
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

        size_t size_ = 0;
        HashTable<Key, Value> table;
    
    public:
        CacheLevel  (size_t size) : size_(size);
        ~CacheLevel ();

        put (Value Value);
        get (Key Key);
        Remove (Key key);
        
};

template <typename Value, typename Key, typename Extractor>
class Cache  {
    private:

        CacheLevel<Key, Value> L1_;
        CacheLevel<Key, Value> L2_;
        CacheLevel<Key, Value> L3_;

    public:
        Cache  (size_t size_L1, size_t size_L2, size_t size_L3);
        ~Cache ();
};

#endif // CACHE_HH
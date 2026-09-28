#ifndef CACHE_HH
#define CACHE_HH

#include "Header.h"

template <typename Value, typename Key, typename Extractor>
class CacheLevel 
{
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
class Cache  
{
    private:

        CacheLevel<Value, Key, Extractor> L1_;
        CacheLevel<Value, Key, Extractor> L2_;
        CacheLevel<Value, Key, Extractor> L3_;

    public:
        Cache  (size_t size_L1, size_t size_L2, size_t size_L3);
        ~Cache ();
};

#endif // CACHE_HH
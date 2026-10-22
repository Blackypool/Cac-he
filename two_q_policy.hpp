#ifndef TWO_Q_POLICY_HH
#define TWO_Q_POLICY_HH

#include "lru_policy.hpp"

#include <utility>
#include <optional>
#include <list>
#include <unordered_map>
#include <concepts>
#include <limits>
#include <stdexcept>

#include <algorithm>
#include <ostream>
#include <vector>

#include "types.h"

namespace MyCache {

struct TwoQTag {};

struct TwoQMeta
{
    using compatibility = TwoQTag; 
};

// ======================================================= 2Q cachelevel class ==================================================
template <
    typename Value, 
    typename Key, 
    typename Extractor, 
    typename Hash
>
class TwoQCacheLevel
{
    private:
        inline static std::optional<Key> last_key_for_up_{};

        Extractor get_key_;
    
    //_________________________________________________________________________________________________________________________________________//
    
        size_t size_A1_;     
        size_t size_Am_;     
        size_t size_A1out_;  


        // A1 -- FIFO -- new objects
        struct Node_2Q_A1_
        {
            Key key;
            Value value;
        };
        std::list <Node_2Q_A1_> A1_list_;
        std::unordered_map <Key, typename std::list<Node_2Q_A1_>::iterator, Hash> A1_Htable_;


        // A1_out -- w\ meta-data  // -- keys of вытесненных из A1
        struct Node_2Q_A1out_
        {
            Key key;
        };
        std::list <Node_2Q_A1out_> A1out_list_;
        std::unordered_map <Key, typename std::list<Node_2Q_A1out_>::iterator, Hash> A1out_Htable_;


        // Am -- LRU -- >= 2 запросов
        LRUCacheLevel <Value, Key, Extractor, Hash> Am_LRU_;

    //_________________________________________________________________________________________________________________________________________//
        
        std::optional<Value> add_to_A1 (Node_2Q_A1_ n_value)
        {
            std::optional<Value> value_of_last = std::nullopt;

            //////////////CHECK_SIZE////////////////    
            if (A1_list_.size() >= size_A1_)
            {
                typename std::list<Node_2Q_A1_>::iterator it_last = std::prev(A1_list_.end());  // check it of last in list
                value_of_last = std::move(it_last->value);                           // move владение of last

                remove_out_A1 (it_last->key);
            }
            ////////////////////////////////////////
    

            //////////////////ADD///////////////////            
            A1_list_.push_front(std::move(n_value));                     // add new value in list
            A1_Htable_.emplace(A1_list_.front().key, A1_list_.begin());  // add new it in Ht
            ////////////////////////////////////////
            
            return value_of_last;
        }

        void remove_out_A1 (Key key)
        {
            //////////////////IT////////////////////
            auto it = A1_Htable_.find(key);  // it in Ht
            ////////////////////////////////////////


            ////////////////DEL_in_A1////////////////
            A1_list_.erase(it->second);             // it->second = value of node in Ht that equal "it" in list
            A1_Htable_.erase(it);
            ////////////////////////////////////////


            ////////////////ADD_in_out//////////////
            Node_2Q_A1out_ move_last = {.key = key};
            add_to_A1_out (move_last);
            ////////////////////////////////////////
        }

        void add_to_A1_out (Node_2Q_A1out_ n_key)
        {
            //////////////CHECK_SIZE////////////////    
            if (A1out_list_.size() >= size_A1out_)
            {
                Key last_key = A1out_list_.back().key;
                auto last_it = A1out_Htable_.find(last_key);

                if (last_it == A1out_Htable_.end())
                    throw std::out_of_range("A1out hmap mismatch\n");
                remove_out_A1_out(last_it);
            }
            ////////////////////////////////////////

    
            //////////////////ADD///////////////////            
            A1out_list_.push_front(n_key);                          // add new value in list
            A1out_Htable_.emplace(n_key.key, A1out_list_.begin());  // add new it in Ht
            ////////////////////////////////////////
        }

        void remove_out_A1_out (typename std::unordered_map<Key, typename std::list<Node_2Q_A1out_>::iterator, Hash>::iterator& it)
        {
            A1out_list_.erase(it->second);
            A1out_Htable_.erase(it);
        }

    //_________________________________________________________________________________________________________________________________________//
    public:

        struct AddResult 
        {
            std::optional <CacheTransfer<Value, TwoQMeta>> evicted;  // выкинутый <value + meta>
            Value* inserted;
        };

        template <typename AnyMeta>
        AddResult add(CacheTransfer<Value, AnyMeta> elem)
        {
            ////////////////INIT////////////////////
            Key key = get_key_(elem.value);
            ////////////////////////////////////////


            ////////////////UPPER///////////////////
            if (last_key_for_up_ == key)  // => go to Am
            {
                // if в L1 выпал из А1 и попал в Aout и попадает в L2 A1 -> при поиске находим в Aout L1 сохраняем флаг, находим в L2 и переносим в Am L1
                auto last_ret_add = Am_LRU_.add(std::move(elem.value));  // add in Am
                std::optional<CacheTransfer<Value, TwoQMeta>> evicted = std::nullopt;

                if (last_ret_add.evicted != std::nullopt)
                {
                    last_key_for_up_ = get_key_(last_ret_add.evicted->value);
                    evicted.emplace(CacheTransfer<Value, TwoQMeta> {std::move(last_ret_add.evicted->value), TwoQMeta{}} );
                }
                else
                    last_key_for_up_.reset();

                return {std::move(evicted), last_ret_add.inserted};
            }
            ////////////////////////////////////////


            //////////////////A1///////////////////            
            Node_2Q_A1_ n_value = {key, std::move(elem.value)};      
            std::optional<Value> last_val = add_to_A1 (std::move(n_value));

            std::optional<CacheTransfer<Value, TwoQMeta>> evicted = std::nullopt;
            if (last_val != std::nullopt)
                evicted.emplace(CacheTransfer<Value, TwoQMeta> {std::move(*last_val), TwoQMeta{}});

            return {std::move(evicted), &A1_list_.front().value};
            ////////////////////////////////////////
        }

        AddResult add(Value value)
        {   
            return add<TwoQMeta>({std::move(value), {}});
        }

        const Value* get(const Key& key)
        {
            //////////////////Am////////////////////
            const Value* value = Am_LRU_.get(key);
            if (value != nullptr)
                return value;
            ////////////////////////////////////////


            //////////////////A1////////////////////
            auto it_A1 = A1_Htable_.find(key);
            if (it_A1 != A1_Htable_.end())
                return &(it_A1->second->value);     // A1 is FIFO => not need push to head
            ////////////////////////////////////////


            ////////////////A1_out//////////////////
            auto it_A1out = A1out_Htable_.find(key);
            if (it_A1out != A1out_Htable_.end())
            {
                last_key_for_up_ = key;
                remove_out_A1_out (it_A1out);

                return nullptr;
            }
            ////////////////////////////////////////

            // cache-miss
            return nullptr;
        }

        bool find (const Key& key)
        {
            //////////////////A1////////////////////
            auto it_A1 = A1_Htable_.find(key);
            if (it_A1 != A1_Htable_.end())
                return true;
            ////////////////////////////////////////


            ////////////////GHOST///////////////////
            auto it_A1_out = A1out_Htable_.find(key);
            if (it_A1_out != A1out_Htable_.end())
            {
                last_key_for_up_ = key;
                remove_out_A1_out (it_A1_out);

                return false;
            }
            ////////////////////////////////////////


            //////////////////Am////////////////////
            return Am_LRU_.find(key);
            ////////////////////////////////////////
        }

        void remove (const Key& key)
        {
            /////////////////A1/////////////////////
            auto it_A1 = A1_Htable_.find(key);
            if (it_A1 != A1_Htable_.end())
            {
                A1_list_.erase(it_A1->second);
                A1_Htable_.erase(it_A1);

                return;
            }
            ////////////////////////////////////////


            ////////////////A1_out//////////////////
            auto it_A1out = A1out_Htable_.find(key);
            if (it_A1out != A1out_Htable_.end())
                return remove_out_A1_out (it_A1out);
            ////////////////////////////////////////


            //////////////////Am////////////////////
            Am_LRU_.remove(key);
            ////////////////////////////////////////
        }

        std::optional<CacheTransfer<Value, TwoQMeta>> extract(const Key& key)
        {
            /////////////////A1/////////////////////
            auto it_A1 = A1_Htable_.find(key);
            if (it_A1 != A1_Htable_.end())
            {
                CacheTransfer<Value, TwoQMeta> result{std::move(it_A1->second->value), TwoQMeta{}};

                A1_list_.erase(it_A1->second);
                A1_Htable_.erase(it_A1);

                return result;
            }
            ////////////////////////////////////////


            ////////////////A1_out//////////////////
            auto it_A1out = A1out_Htable_.find(key);
            if (it_A1out != A1out_Htable_.end())
            {
                last_key_for_up_ = key;

                remove_out_A1_out (it_A1out);
                return std::nullopt;
            }
            ////////////////////////////////////////


            //////////////////Am////////////////////
            auto inner = Am_LRU_.extract(key);
            if (inner == std::nullopt)
                return std::nullopt;

            last_key_for_up_ = key;     // because up from Am need add to Am
            return CacheTransfer<Value, TwoQMeta>{std::move(inner->value), TwoQMeta{}};
            ////////////////////////////////////////
        }

    //_________________________________________________________________________________________________________________________________________//

        TwoQCacheLevel(size_t size_of_A1_cache, size_t size_of_Am_cache, size_t size_of_A1out_cache, Extractor key, Hash hash_func) :
            get_key_(key),    
        
            size_A1_(size_of_A1_cache),  
            size_Am_(size_of_Am_cache),    
            size_A1out_(size_of_A1out_cache),  

            Am_LRU_(size_Am_, get_key_, hash_func)
        {
            if ( (size_of_A1_cache < 1) || (size_of_Am_cache < 1) || (size_of_A1out_cache < 1))
                throw std::invalid_argument("need size of cache 2Q > 0");
        }

        // A1 = 0.25  Am = 0.75  A1out = 1.00
        TwoQCacheLevel(size_t capacity, Extractor key, Hash hash_func) :
            get_key_(key),    
        
            size_A1_(capacity / 4),  
            size_Am_(3 * capacity / 4),    
            size_A1out_(capacity),  

            Am_LRU_(size_Am_, get_key_, hash_func)
        {
            if ( (capacity < 4))
                throw std::invalid_argument("need size of cache 2Q > 0");
        }
        ~TwoQCacheLevel() = default;
};


}

#endif // TWO_Q_POLICY_HH
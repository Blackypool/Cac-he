#ifndef TWO_Q_POLICY_HH
#define TWO_Q_POLICY_HH

#include <utility>
#include <optional>
#include <list>
#include <unordered_map>
#include <concepts>
#include <limits>

#include <algorithm>
#include <ostream>
#include <vector>

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
        static Key last_key_for_up_{};

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
        LRUCache::LRUCacheLevel <Value, Key, Extractor, Hash> Am_LRU_;

    //_________________________________________________________________________________________________________________________________________//
        
        std::optional<Value> add_to_A1 (Node_2Q_A1_& n_value)
        {
            std::optional<Value> value_of_last = std::nullopt;

            //////////////CHECK_SIZE////////////////    
            if (A1_list_.size() == size_A1_)
            {
                typename std::list<Node_2Q_A1_>::iterator it_last = std::prev(A1_list_.end());  // check it of last in list
                value_of_last = std::move(it_last->value);                           // move владение of last

                remove_out_A1 (it_last->key);
            }
            ////////////////////////////////////////
    

            //////////////////ADD///////////////////            
            A1_list_.push_front(n_value);                       // add new value in list
            A1_Htable_.emplace(n_value.key, A1_list_.begin());  // add new it in Ht
            ////////////////////////////////////////
            
            return value_of_last;
        }

        void remove_out_A1 (Key& key)
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

        void add_to_A1_out (Node_2Q_A1out_& n_key)
        {
            //////////////CHECK_SIZE////////////////    
            if (A1out_list_.size() == size_A1out_)
            {
                typename std::list<Node_2Q_A1out_>::iterator it_last = std::prev(A1out_list_.end());  // check it of last in list
                remove_out_A1_out (it_last->key);
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
            Value value = elem.value;
            Key key = get_key_(value);
            ////////////////////////////////////////


            ////////////////UPPER///////////////////
            if (last_key_for_up_ == key)  // => go to Am
            {
                // if в L1 выпал из А1 и попал в Aout и попадает в L2 A1 -> при поиске находим в Aout L1 сохраняем флаг, находим в L2 и переносим в Am L1
                AddResult last_ret_add = Am_LRU_.add(value);  // add in Am

                if (last_ret_add.evicted == std::nullopt)
                    do_zero(&last_key_for_up_);
                else
                    last_key_for_up_ = get_key_(last_ret_add.evicted.value);

                return last_ret_add;
            }
            ////////////////////////////////////////


            //////////////////A1///////////////////            
            Node_2Q_A1_ n_value = {.key = key, .value = value};      
            std::optional<Value> last_val = add_to_A1 (n_value);

            AddResult ret_add = {};
            ret_add.inserted = &value;

            if (last_val == std::nullopt)
                ret_add.evicted = std::nullopt;
            else
                ret_add.evicted.value = last_val;

            return ret_add;
            ////////////////////////////////////////
        }

        AddResult add(Value value)
        {   
            return add<TwoQMeta>({std::move(value)}, {});
        }

        Value* get(const Key& key)
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
            CacheTransfer<Value, TwoQMeta> need_ret = {};

            /////////////////A1/////////////////////
            auto it_A1 = A1_Htable_.find(key);
            if (it_A1 != A1_Htable_.end())
            {
                need_ret.value = std::move(it_A1->second->value);  // move владение of need

                A1_list_.erase(it_A1->second);
                A1_Htable_.erase(it_A1);

                return need_ret;
            }
            ////////////////////////////////////////


            ////////////////A1_out//////////////////
            auto it_A1out = A1out_Htable_.find(key);
            if (it_A1out != A1out_Htable_.end())
            {
                last_key_for_up_ = key;

                need_ret.value = std::move(it_A1out->second->value);  // move владение of need
                remove_out_A1_out (it_A1out);

                return need_ret;
            }
            ////////////////////////////////////////


            //////////////////Am////////////////////
            last_key_for_up_ = key;     // because up from Am need add to Am
            return Am_LRU_.extract(key);
            ////////////////////////////////////////
        }

    //_________________________________________________________________________________________________________________________________________//

        TwoQCacheLevel(Extractor key, size_t size_of_A1_cache, size_t size_of_Am_cache, size_t size_of_A1out_cache) :
            get_key_(key),    
        
            size_A1_(size_of_A1_cache),  
            size_Am_(size_of_Am_cache),    
            size_A1out_(size_of_A1out_cache),  

            Am_LRU_(size_Am_, get_key_)
        {}
        ~TwoQCacheLevel() = default;
};


}

#endif // TWO_Q_POLICY_HH
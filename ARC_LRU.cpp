#include "Header.h"

//________________________________________________Questions_Tasks__________________________________________________________________________//
// 
//_________________________________________________________________________________________________________________________________________//

int main()
{


    return 0;
}

//_________________________________________________________________________________________________________________________________________//
template <typename Value, typename Meta>
struct CacheTransfer
{
    Value value;
    Meta meta;
};
//_________________________________________________________________________________________________________________________________________//


//_________________________________________________________________________________________________________________________________________//
namespace ARCCache
{
    //_________________________________________________________________________________________________________________________________________//
    struct ARCTag {};

    struct ARCMeta
    {
        using compatibility = ARCTag; 
    };
    //_________________________________________________________________________________________________________________________________________//

template <typename Value, typename Key, typename Extractor, typename Hash>
class ARCCacheLevel
{
    private:

    //_________________________________________________________________________________________________________________________________________//
        static Key last_key_for_up_{};
        //{   
            // For what it need:
            //
            // Have two problems with high level cache (L1 L2 L3):
            // -- How work with B lists after removing of T in L and how check to add in T1/T2 after upp of L2/L3
            //
            //
            // About check BN and up/down
            // if we search element "E" that placed in L1(B1) after found it: change size + save "key" in static and return "not found"
            // search next and found "E" in L2(T2/T1) -> need up it -> add to L1(T2) if ключи совпали + free(key)
            //
            // About go down by levels: 
            //     if add in L1(T2) -> add in L1(B2) + save key and if (key=key) that checked lower in L2 -> add in L2(T2), else -> L2(T1)
            //
            // About upper:
            //     When found "E" in L2(T1) key is free -> add in L1(T1) else if (key==key) -> L1(T2) + free key
            //
        //}

        Extractor get_key_;

    //_________________________________________________________________________________________________________________________________________//

        size_t size_;       // = T1 + T2
        size_t size_T1_;    // = sizeof (T1)

        size_t size_B1_;      
        size_t size_B2_;        

        struct Node_ARC_T_  // live values
        {
            Key key;
            Value value;
        };

        struct Node_ARC_B_  // meta-data == ghost
        {
            Key key;
        };


        std::list <Node_ARC_T_> T1_list_;    // first time add
        std::unordered_map <Key, typename std::list<Node_ARC_T_>::iterator, Hash> T1_Htable_;

        std::list <Node_ARC_T_> T2_list_;    // >= 2 times needed
        std::unordered_map <Key, typename std::list<Node_ARC_T_>::iterator, Hash> T2_Htable_;

        std::list <Node_ARC_B_> B1_list_;    // trash of T1 with meta-data
        std::unordered_map <Key, typename std::list<Node_ARC_B_>::iterator, Hash> B1_Htable_;

        std::list <Node_ARC_B_> B2_list_;    // trash of T2 with meta-data
        std::unordered_map <Key, typename std::list<Node_ARC_B_>::iterator, Hash> B2_Htable_;

        // in Ht {key, it} -> it = it in list where node = {key, value}
        // find() in Ht is ret it in Ht: it->first  = key
        //                               it->second = it_in_list

    //_________________________________________________________________________________________________________________________________________//

        std::optional<Value> add_in_TN (Node_ARC_T_& n_value,  \
                        size_t& size_of_list_T, \
                        size_t& size_of_list_B,  \
                        std::list<Node_ARC_T_>& T_T_list, \
                        std::list<Node_ARC_B_>& B_B_list, \
                        std::unordered_map <Key, typename std::list<Node_ARC_T_>::iterator, Hash>& T_T_Htable,\
                        std::unordered_map <Key, typename std::list<Node_ARC_B_>::iterator, Hash>& B_B_Htable  )
        {
            std::optional<Value> value_of_last = std::nullopt;

            //////////////CHECK_SIZE////////////////
            if (T_T_list.size() == size_of_list_T)      // move last of TN -> BN
            {
                typename std::list<Node_ARC_T_>::iterator it_last = std::prev(T_T_list.end());   // check it of last in list
                value_of_last = std::move(it_last->value);                              // move владение of last

                remove_out_TN (it_last->key, size_of_list_B, T_T_list, B_B_list, T_T_Htable, B_B_Htable);  // delete out of list+Ht (TN)
            }
            ////////////////////////////////////////

            ////////////////ADD/////////////////////
            T_T_list.push_front(n_value);
            T_T_Htable.emplace(n_value.key, T_T_list.begin());
            ////////////////////////////////////////

            return value_of_last;
        }

        std::optional<Value> add_in_BN (Node_ARC_B_& n_key, size_t& size_of_list_B, \
                                            std::list<Node_ARC_B_>& B_B_list, \
                                            std::unordered_map <Key, typename std::list<Node_ARC_B_>::iterator, Hash>& B_B_Htable )
        {
            std::optional<Value> value_of_last = std::nullopt;

            //////////////CHECK_SIZE////////////////
            if (B_B_list.size() == size_of_list_B)
            {
                typename std::list<Node_ARC_B_>::iterator it_last = std::prev(B_B_list.end());   // check it of last in list
                value_of_last = std::move(it_last->value);                              // move владение of last

                remove_out_BN (it_last->key, B_B_list, B_B_Htable);
            }
            ////////////////////////////////////////

            ////////////////ADD/////////////////////
            B_B_list.push_front(n_key);
            B_B_Htable.emplace(n_key.key, B_B_list.begin());  // add key, value (= it in list)
            ////////////////////////////////////////

            return value_of_last;
        }

        void remove_out_TN (Key& key, \
                            size_t& size_of_list_B,  \
                            std::list<Node_ARC_T_>& T_T_list, \
                            std::list<Node_ARC_B_>& B_B_list,  \
                            std::unordered_map <Key, typename std::list<Node_ARC_T_>::iterator, Hash>& T_T_Htable, \
                            std::unordered_map <Key, typename std::list<Node_ARC_B_>::iterator, Hash>& B_B_Htable   )
        {
            //////////////////IT////////////////////
            auto it = T_T_Htable.find(key);  // it in Ht
            ////////////////////////////////////////

            ////////////////DEL_in_T////////////////
            T_T_list.erase(it->second);             // it->second = value of node in Ht that equal it in list
            T_T_Htable.erase(it);
            ////////////////////////////////////////

            ////////////////ADD_in_B////////////////
            Node_ARC_B_ move_last = {.key = key};
            add_in_BN (move_last, size_of_list_B, B_B_list, B_B_Htable);
            ////////////////////////////////////////
        }

        void remove_out_BN (Key& key, \
                            std::list<Node_ARC_B_>& B_B_list,  \
                            std::unordered_map <Key, typename std::list<Node_ARC_B_>::iterator, Hash>& B_B_Htable )
        {
            //////////////////IT////////////////////
            auto it = B_B_Htable.find(key);  // it in Ht
            ////////////////////////////////////////

            ////////////////DEL_in_B////////////////
            B_B_list.erase(it->second);
            B_B_Htable.erase(it);
            ////////////////////////////////////////
        }

        std::optional<Value> add_in_T2_for_upper (Node_ARC_T_ n_value)
        {
            do_zero(&last_key_for_up_);
            return add_in_TN (n_value, (size_ - size_T1_), size_B2_, T2_list_, B2_list_, T2_Htable_, B2_Htable_);
        }
    
    //_________________________________________________________________________________________________________________________________________//
    public:

        struct AddResult 
        {
            std::optional <CacheTransfer<Value, ARCMeta>> evicted;  // выкинутый <value + meta>
            Value* inserted;
        };

        template <typename AnyMeta>
        AddResult add(CacheTransfer<Value, AnyMeta> elem)
        {
            // удаление из мета-списков происходит при get, тк потом сразу вызывается add => not need check meta lists in add()
            /////////////////////////////////INIT///////////////////////////////////////
            Value value = elem.value;
            Key key = get_key_(value);
            Node_ARC_T_ n_value = {.key = key, .value = value};

            std::optional<Value> value_of_last = std::nullopt;  // for ret вытеснутого
            AddResult ret_add = {};
            ret_add.inserted = &value;
            ////////////////////////////////////////////////////////////////////////////


            ///////////////////////////////////ADD//////////////////////////////////////
            if (last_key_for_up_ == key)  // add element in T2 because / level up / default search and found in any B /
                value_of_last = add_in_T2_for_upper (n_value);  // static key is null in therre
            else
                value_of_last = add_in_TN (n_value, size_T1_, size_B1_, T1_list_, B1_list_, T1_Htable_, B1_Htable_);  // no T2 --> T1
            ////////////////////////////////////////////////////////////////////////////


            /////////////////////////////////RET////////////////////////////////////////
            if (value_of_last == std::nullopt)
                ret_add.evicted = std::nullopt;
            else
                ret_add.evicted.value = value_of_last;

            return ret_add;
            ////////////////////////////////////////////////////////////////////////////
        }

        AddResult add(Value value)
        {
            return add<ARCMeta>({std::move(value)}, {});
        }


        const Value* get (const Key& key)
        {
            ///////////////////////////////T_lines//////////////////////////////////////
            auto it_T1_Ht = T1_Htable_.find(key);
            if (it_T1_Ht != T1_Htable_.end())  // already in T1
            {
                T1_list_.splice(T1_list_.begin(), T1_list_, it_T1_Ht->second);  // move to head T1
                return &(it_T1_Ht->second->value);
            }

            auto it_T2_Ht = T2_Htable_.find(key);
            if (it_T2_Ht != T2_Htable_.end())  // already in T2
            {
                T2_list_.splice(T2_list_.begin(), T2_list_, it_T2_Ht->second);  // move to head T2
                return &(it_T2_Ht->second->value);
            }
            ////////////////////////////////////////////////////////////////////////////


            ///////////////////////////////B_lines//////////////////////////////////////
            auto it_B1_Ht = B1_Htable_.find(key);
            if (it_B1_Ht != B1_Htable_.end())  // already in B1
            {
                size_T1_++;              // need more space to T1
                last_key_for_up_ = key;  // save key in static for next zapros
                
                Node_ARC_T_ last_of_T2 = T2_list_.back();   // take key of last for delete with func T1
                remove_out_BN (key, B1_list_, B1_Htable_);  // delete out of B1
                remove_out_TN (last_of_T2.key, size_B2_, T2_list_, B2_list_, T2_Htable_, B2_Htable_);  // T2 be lower -> need delete in T2 -> go to B2

                return nullptr;
            }

            auto it_B2_Ht = B2_Htable_.find(key);
            if (it_B2_Ht != B2_Htable_.end())  // already in B2
            {
                size_T1_--;              // need more space to T2
                last_key_for_up_ = key;  // save key in static for next zapros

                Node_ARC_T_ last_of_T1 = T1_list_.back();   // take key of last for delete with func T1
                remove_out_BN (key, B2_list_, B2_Htable_);  // delete out of B2
                remove_out_TN (last_of_T1.key, size_B1_, T1_list_, B1_list_, T1_Htable_, B1_Htable_);  // T1 be lower -> need delete in T1 -> go to B1

                return nullptr;
            }
            ////////////////////////////////////////////////////////////////////////////


            ////////////////////////////////MISS////////////////////////////////////////
            return nullptr;
            ////////////////////////////////////////////////////////////////////////////
        }

        bool find (const Key& key)
        {
            ///////////////////////////////T_lines//////////////////////////////////////
            auto it_T1_Ht = T1_Htable_.find(key);
            if (it_T1_Ht != T1_Htable_.end())  // already in T1
                return true;

            auto it_T2_Ht = T2_Htable_.find(key);
            if (it_T2_Ht != T2_Htable_.end())  // already in T2
                return true;
            ////////////////////////////////////////////////////////////////////////////


            // delete in B + ret false + save static key, beause при add -> T2
            // size = const, beacuse это искусственное add повторного of element || level up // change size in extract
            ///////////////////////////////B_lines//////////////////////////////////////
            auto it_B1_Ht = B1_Htable_.find(key);
            if (it_B1_Ht != B1_Htable_.end())  // already in B1
            {
                last_key_for_up_ = key;
                
                remove_out_BN (key, B1_list_, B1_Htable_);  // delete out of B1
                return false;
            }

            auto it_B2_Ht = B2_Htable_.find(key);
            if (it_B2_Ht != B2_Htable_.end())  // already in B2
            {
                last_key_for_up_ = key;
                
                remove_out_BN (key, B2_list_, B2_Htable_);  // delete out of B2
                return false;
            }
            ////////////////////////////////////////////////////////////////////////////


            ////////////////////////////////MISS////////////////////////////////////////
            return false;
            ////////////////////////////////////////////////////////////////////////////
        }

        void remove (const Key& key)  // full delete in all possible lines
        {
            ///////////////////////////////FREE_T///////////////////////////////////////
            auto it_T1_Ht = T1_Htable_.find(key);
            if (it_T1_Ht != T1_Htable_.end())  // already in T1
            {
                remove_out_TN (key, size_B1_, T1_list_, B1_list_, T1_Htable_, B1_Htable_);  // delete in T1 + move to B1 
                remove_out_BN (key, B1_list_, B1_Htable_);                                  // delete in B1
                return;
            }

            auto it_T2_Ht = T2_Htable_.find(key);
            if (it_T2_Ht != T2_Htable_.end())  // already in T2
            {
                remove_out_TN (key, size_B2_, T2_list_, B2_list_, T2_Htable_, B2_Htable_);  // delete in T2 + move to B2
                remove_out_BN (key, B2_list_, B2_Htable_);                                  // delete in B2
                return;
            }
            ////////////////////////////////////////////////////////////////////////////


            ///////////////////////////////FREE_B///////////////////////////////////////
            auto it_B1_Ht = B1_Htable_.find(key);
            if (it_B1_Ht != B1_Htable_.end())  // already in B1
            {
                remove_out_BN (key, B1_list_, B1_Htable_);  // delete in B1
                return;
            }

            auto it_B2_Ht = B2_Htable_.find(key);
            if (it_B2_Ht != B2_Htable_.end())  // already in B2
            {
                remove_out_BN (key, B2_list_, B2_Htable_);  // delete in B2
                return;
            }
            ////////////////////////////////////////////////////////////////////////////

            // removed before //
            return;
        }

        std::optional<CacheTransfer<Value, ARCMeta>> extract(const Key& key)  // for level up  // работает как get => need change size
        {   
            CacheTransfer <Value, ARCMeta> need_ret = {};

            ////////////////////////////////////////////////////////////////////////////
            auto it_T1_Ht = T1_Htable_.find(key);
            if (it_T1_Ht != T1_Htable_.end())  // already in T1
            {
                need_ret.value = std::move(it_T1_Ht->second->value);

                T1_list_.erase(it_T1_Ht->second);
                T1_Htable_.erase(it_T1_Ht);

                return need_ret;
            }
            ////////////////////////////////////////////////////////////////////////////


            ////////////////////////////////////////////////////////////////////////////
            auto it_T2_Ht = T2_Htable_.find(key);
            if (it_T2_Ht != T2_Htable_.end())  // already in T2
            {
                last_key_for_up_ = key;        // save key for add in T2 upper

                need_ret.value = std::move(it_T2_Ht->second->value);

                T2_list_.erase(it_T2_Ht->second);
                T2_Htable_.erase(it_T2_Ht);

                return need_ret;
            }
            ////////////////////////////////////////////////////////////////////////////


            ///////////////////////////////B_lines//////////////////////////////////////
            auto it_B1_Ht = B1_Htable_.find(key);
            if (it_B1_Ht != B1_Htable_.end())  // already in B1
            {
                size_T1_++;              // need more space to T1
                last_key_for_up_ = key;  // save key in static for next zapros
                
                Node_ARC_T_ last_of_T2 = T2_list_.back();   // take key of last for delete with func T1
                remove_out_BN (key, B1_list_, B1_Htable_);  // delete out of B1
                remove_out_TN (last_of_T2.key, size_B2_, T2_list_, B2_list_, T2_Htable_, B2_Htable_);  // T2 be lower -> need delete in T2 -> go to B2

                return std::nullopt;
            }

            auto it_B2_Ht = B2_Htable_.find(key);
            if (it_B2_Ht != B2_Htable_.end())  // already in B2
            {
                size_T1_--;              // need more space to T2
                last_key_for_up_ = key;  // save key in static for next zapros

                Node_ARC_T_ last_of_T1 = T1_list_.back();   // take key of last for delete with func T1
                remove_out_BN (key, B2_list_, B2_Htable_);  // delete out of B2
                remove_out_TN (last_of_T1.key, size_B1_, T1_list_, B1_list_, T1_Htable_, B1_Htable_);  // T1 be lower -> need delete in T1 -> go to B1

                return std::nullopt;
            }
            ////////////////////////////////////////////////////////////////////////////

            return std::nullopt;
        }

    //_________________________________________________________________________________________________________________________________________//

        ARCCacheLevel(Extractor key, size_t sz_of_summ_of_T, size_t sz_of_T_one, size_t sz_of_B_one, size_t sz_of_B_two) :
            size_(sz_of_summ_of_T),
            size_T1_(sz_of_T_one),

            size_B1_(sz_of_B_one),
            size_B2_(sz_of_B_two),

            get_key_(key)
        {}
        ~ARCCacheLevel() = default;
};
}
//_________________________________________________________________________________________________________________________________________//
    
//_________________________________________________________________________________________________________________________________________//
namespace LRUCache
{
    //_________________________________________________________________________________________________________________________________________//
    struct LRUTag {};

    struct LRUMeta
    {
        using compatibility = LRUTag; 
    };
    //_________________________________________________________________________________________________________________________________________//

template <typename Value, typename Key, typename Extractor, typename Hash>
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
            Value value = elem.value;
            Key key = get_key_(value);
            Node_LRU_ n_value = {.key = key, .value = value};

            std::optional<Value> value_of_last = std::nullopt;  // for copy вытеснутого
            AddResult ret_add = {};
            ret_add.inserted = &value;
            ////////////////////////////////////////

    
            //////////////CHECK_SIZE////////////////    
            if (hot_list_.size() == size_)
            {
                typename std::list<Node_LRU_>::iterator it_last = std::prev(hot_list_.end());  // check it of last in list
                value_of_last = std::move(it_last->value);                            // move владение of last
                
                h_table_.erase(it_last->key);  // delete last in Ht  
                hot_list_.pop_back();          // delete in lidt

                ret_add.evicted.value = value_of_last;
            }
            else
                ret_add.evicted = std::nullopt;
            ////////////////////////////////////////


            //////////////////ADD///////////////////
            hot_list_.push_front(n_value);             // add new value in list
            h_table_.emplace(key, hot_list_.begin());  // add new it in Ht
            ////////////////////////////////////////
            
            return ret_add;
        }

        AddResult add(Value value)
        {   
            return add<LRUMeta>({std::move(value)}, {});
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

            CacheTransfer <Value, LRUMeta> need_val = {};
            need_val.value = std::move(it->second->value);  // move владение of need

            hot_list_.erase(it->second);
            h_table_.erase(it);

            return need_val;
        }

    //_________________________________________________________________________________________________________________________________________//

        LRUCacheLevel (size_t size_of_cache, Extractor key) : size_(size_of_cache), get_key_(key) {} 
        ~LRUCacheLevel() = default;
};
}
//_________________________________________________________________________________________________________________________________________//
   
//_________________________________________________________________________________________________________________________________________//
namespace TwoQCache
{
    //_________________________________________________________________________________________________________________________________________//
    struct TwoQTag {};

    struct TwoQMeta
    {
        using compatibility = TwoQTag; 
    };
    //_________________________________________________________________________________________________________________________________________//

template <typename Value, typename Key, typename Extractor, typename Hash>
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
    //_________________________________________________________________________________________________________________________________________//
}
//_________________________________________________________________________________________________________________________________________//
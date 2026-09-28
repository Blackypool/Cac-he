#include "Header.h"

// last_key_for_up_ need обнулять но как незная тип? надо перегрузку для обнуления этой штуки
// need убрать проверку на it == end() в местах где ее точно не надо делать (в приват функциях)

template <typename Value, typename Key, typename Extractor, typename Hash>
class ARCCacheLevel
{
    private:

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
        std::unordered_map <Key, std::list<Node_ARC_T_>::iterator, Hash> T1_Htable_;

        std::list <Node_ARC_T_> T2_list_;    // >= 2 times needed
        std::unordered_map <Key, std::list<Node_ARC_T_>::iterator, Hash> T2_Htable_;

        std::list <Node_ARC_B_> B1_list_;    // trash of T1 with meta-data
        std::unordered_map <Key, std::list<Node_ARC_B_>::iterator, Hash> B1_Htable_;

        std::list <Node_ARC_B_> B2_list_;    // trash of T2 with meta-data
        std::unordered_map <Key, std::list<Node_ARC_B_>::iterator, Hash> B2_Htable_;

        // in Ht {key, it} -> it = it in list where node = {key, value}
        // find() in Ht is ret it in Ht: it->first  = key
        //                               it->second = it_in_list

    //_________________________________________________________________________________________________________________________________________//

        std::optional<Value> add_in_TN (Node_ARC_T_& n_value,  \
                        size_t& size_of_list_T, \
                        size_t& size_of_list_B,  \
                        std::list<Node_ARC_T_>& T_T_list, \
                        std::list<Node_ARC_B_>& B_B_list, \
                        std::unordered_map <Key, std::list<Node_ARC_T_>::iterator, Hash>& T_T_Htable,\
                        std::unordered_map <Key, std::list<Node_ARC_B_>::iterator, Hash>& B_B_Htable  )
        {
            std::optional<Value> value_of_last = std::nullopt;

            //////////////CHECK_SIZE////////////////
            if (T_T_list.size() == size_of_list_T)      // move last of TN -> BN
            {
                std::list<Node_ARC_T_>::iterator it_last = std::prev(T_T_list.end());   // check it of last in list
                value_of_last = std::move(it_last->value);                              // move владение of last

                remove_out_TN (it_last->key, size_of_list_B, T_T_list, B_B_list, T_T_Htable, B_B_Htable);  // delete out of list+Ht (TN)
            }
            ////////////////////////////////////////

            ////////////////ADD/////////////////////
            T_T_list.push_front(n_value);
            T_T_Htable.emplace(n_value.key, hot_list_.begin());
            ////////////////////////////////////////

            return value_of_last;
        }

        std::optional<Value> add_in_BN (Node_ARC_B_& n_key, size_t& size_of_list_B, \
                                            std::list<Node_ARC_B_>& B_B_list, \
                                            std::unordered_map <Key, std::list<Node_ARC_B_>::iterator, Hash>& B_B_Htable )
        {
            std::optional<Value> value_of_last = std::nullopt;

            //////////////CHECK_SIZE////////////////
            if (B_B_list.size() == size_of_list_B)
            {
                std::list<Node_ARC_B_>::iterator it_last = std::prev(B_B_list.end());   // check it of last in list
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
                            std::unordered_map <Key, std::list<Node_ARC_T_>::iterator, Hash>& T_T_Htable, \
                            std::unordered_map <Key, std::list<Node_ARC_B_>::iterator, Hash>& B_B_Htable   )
        {
            //////////////////IT////////////////////
            auto it = T_T_Htable.find(key);  // it in Ht
            if (it == T_T_Htable.end())      // removed before
                return;
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
                            std::unordered_map <Key, std::list<Node_ARC_B_>::iterator, Hash>& B_B_Htable )
        {
            //////////////////IT////////////////////
            auto it = B_B_Htable.find(key);  // it in Ht
            if (it == B_B_Htable.end())      // removed before
                return;
            ////////////////////////////////////////

            ////////////////DEL_in_B////////////////
            B_B_list.erase(it->second);
            B_B_Htable.erase(it);
            ////////////////////////////////////////
        }

        std::optional<Value> add_in_T2_for_upper (Node_ARC_T_ n_value)
        {
            last_key_for_up_ = nullptr;
            return add_in_TN (n_value, (size_ - size_T1_), size_B2_, T2_list_, B2_list_, T2_Htable_, B2_Htable_);
        }
    
    //_________________________________________________________________________________________________________________________________________//
    public:
        
        const Value* get (Key& key)
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
            last_key_for_up_ = nullptr;
            return nullptr;
            ////////////////////////////////////////////////////////////////////////////
        }

        std::optional<Value> add (const Value value)
        {
            Node_ARC_T_ n_value = {.key = get_key_(value), .value = value};
            std::optional<Value> value_of_last = std::nullopt;  // for ret вытеснутого

            ///////////////////////////////UPPER////////////////////////////////////////
            if (last_key_for_up_ != nullptr)  // add element in T2 because / level up / default search and found in any B /
                return add_in_T2_for_upper (n_value);
            ////////////////////////////////////////////////////////////////////////////


            /////////////////////////////ADD_BEFORE/////////////////////////////////////
            const Value* add_before = get (n_value.key);  // can change flag(last_key_for_up_) -> after check flag
            if (add_before != nullptr)  // already in TN
                return std::nullopt;
            ////////////////////////////////////////////////////////////////////////////
        
                        
            ///////////////////////////////UPPER////////////////////////////////////////
            if (last_key_for_up_ != nullptr)  // get() can change flag => need check flag again if found in B with ret nullptr
                return add_in_T2_for_upper (n_value);
            ////////////////////////////////////////////////////////////////////////////


            /////////////////////////////////ADD////////////////////////////////////////
            value_of_last = add_in_TN (n_value, size_T1_, size_B1_, T1_list_, B1_list_, T1_Htable_, B1_Htable_);  // no T2 --> T1
                return value_of_last;
            ////////////////////////////////////////////////////////////////////////////
        }

        void remove (Key& key)  // full delete in all possible lines
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
        std::unordered_map <Key, std::list<Node_LRU_>::iterator, Hash> h_table_;  // Ht -- vector of lists with {it, hash}

    //_________________________________________________________________________________________________________________________________________//
    public:

        std::optional<Value> add (const Value value)  // retrun value of выкинутого element
        {            
            //////////////////IT////////////////////
            Key& key = get_key_(value);

            auto it = h_table_.find(key);
            if (it != h_table_.end())  // already in Ht
                return std::nullopt;
            ////////////////////////////////////////
    
            Node_LRU_ n_value = {.key = key, .value = value};
            std::optional<Value> value_of_last = std::nullopt;  // for copy вытеснутого
    
            //////////////CHECK_SIZE////////////////    
            if (hot_list_.size() == size_)
            {
                std::list<Node_LRU_>::iterator it_last = std::prev(hot_list_.end());  // check it of last in list
                value_of_last = std::move(it_last->value);  // move владение of last
                
                h_table_.erase(it_last->key);  // delete last in Ht  
                hot_list_.pop_back();          // delete in lidt
            }
            ////////////////////////////////////////


            //////////////////ADD///////////////////
            hot_list_.push_front(n_value);             // add new value in list
            h_table_.emplace(key, hot_list_.begin());  // add new it in Ht
            ////////////////////////////////////////
            
            return value_of_last;
        }

        const Value* get (const Key& key)
        {
            auto it = h_table_.find(key);
            if (it == h_table_.end())  // cache miss
                return nullptr;

            hot_list_.splice(hot_list_.begin(), hot_list_, it->second);  // move to head of list
            return &((it->second)->value);
        }

        void remove (const Key& key)
        {
            auto it = h_table_.find(key);
            if (it == h_table_.end())  // removed before
                return;

            hot_list_.erase(it->second);
            h_table_.erase(it);
        }

    //_________________________________________________________________________________________________________________________________________//

        LRUCacheLevel (size_t size_of_cache, Extractor key) : size_(size_of_cache), get_key_(key) {} 
        ~LRUCacheLevel() = default;
};


template <typename Value, typename Key, typename Extractor, typename Hash>
class TwoQCache
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
        std::unordered_map <Key, std::list<Node_2Q_A1_>::iterator, Hash> A1_Htable_;


        // A1_out -- w\ meta-data  // -- keys of вытесненных из A1
        struct Node_2Q_A1out_
        {
            Key key;
        };
        std::list <Node_2Q_A1out_> A1out_list_;
        std::unordered_map <Key, std::list<Node_2Q_A1out_>::iterator, Hash> A1out_Htable_;
        

        // Am -- LRU -- >= 2 запросов
        LRUCacheLevel <Value, Key, Extractor, Hash> Am_LRU_;

    //_________________________________________________________________________________________________________________________________________//
        
        std::optional<Value> add_to_A1 (Node_2Q_A1_& n_value)
        {
            std::optional<Value> value_of_last = std::nullopt;

            //////////////CHECK_SIZE////////////////    
            if (A1_list_.size() == size_A1_)
            {
                std::list<Node_LRU_>::iterator it_last = std::prev(A1_list_.end());  // check it of last in list
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
            if (it == A1_Htable_.end())      // removed before
                return;
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
                std::list<Node_LRU_>::iterator it_last = std::prev(A1out_list_.end());  // check it of last in list
                remove_out_A1_out (it_last->key);
            }
            ////////////////////////////////////////

    
            //////////////////ADD///////////////////            
            A1out_list_.push_front(n_key);                          // add new value in list
            A1out_Htable_.emplace(n_key.key, A1out_list_.begin());  // add new it in Ht
            ////////////////////////////////////////
        }

        void remove_out_A1_out (iterator& it)
        {
            if (it == A1out_Htable_.end())
                return;

            A1out_list_.erase(it->second);
            A1out_Htable_.erase(it);
        }

    //_________________________________________________________________________________________________________________________________________//
    public:

        std::optional<Value> add (const Value& value)
        {
            Key key = get_key_(value);

            ////////////////UPPER///////////////////
            if (last_key_for_up_ == key)  // => go to Am
            {
                // if в L1 выпал из А1 и попал в Aout и попадает в L2 A1 -> при поиске находим в Aout L1 сохраняем флаг, находим в L2 и переносим в Am L1
                std::optional<Value> value_of_last = Am_LRU_.add(value);  // add in Am

                if (value_of_last == std::nullopt)
                    last_key_for_up_ = nullptr;
                else
                    last_key_for_up_ = get_key_(value_of_last);

                return value_of_last;
            }
            ////////////////////////////////////////


            /////////////////A1/////////////////////
            auto it = A1out_Htable_.find(key);
            if (it == A1out_Htable_.end())      // there is no value in ghost      
            {
                Node_2Q_A1_ n_value = {.key = key, .value = value};      
                return add_to_A1 (n_value);       // => add to A1 
            }
            ////////////////////////////////////////


            //////////////////Am////////////////////
            remove_out_A1_out (it);     // delete in ghost
            return Am_LRU_.add(value);  // add in Am
            ////////////////////////////////////////
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

        void remove (Key& key)
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

    //_________________________________________________________________________________________________________________________________________//

        TwoQCache(Extractor key, size_t size_of_A1_cache, size_t size_of_Am_cache, size_t size_of_A1out_cache) :
            get_key_(key),    
        
            size_A1_(size_of_A1_cache),  
            size_Am_(size_of_Am_cache),    
            size_A1out_(size_of_A1out_cache),  

            Am_LRU_(size_Am_, get_key_)
        {}
        ~TwoQCache() = default;
};

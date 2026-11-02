#ifndef ARC_POLICY_HH
#define ARC_POLICY_HH

#include <utility>
#include <optional>
#include <list>
#include <unordered_map>
#include <concepts>
#include <limits>
#include <stdexcept>
#include <iterator>
#include <cstddef>

#include <algorithm>
#include <ostream>
#include <vector>

namespace MyCache::policy
{

struct OptionsARC
{
    size_t sz_of_summ_of_T;
    size_t sz_of_T_one;

    size_t sz_of_B_one;
    size_t sz_of_B_two;
};

struct ARCTag {};

template <typename Key>
struct ARCMeta
{
    std::optional< Key > for_up;
    using compatibility = ARCTag;
};

template <typename AnyMeta, typename Key>
concept IsCompatibleARC = requires(const AnyMeta& meta)
{
    typename AnyMeta::compatibility;

    requires std::same_as<typename AnyMeta::compatibility, ARCTag>;

    {meta.for_up} -> std::convertible_to<std::optional<Key>>;
};

// ======================================================= ARC cachelevel class ==================================================
template <
    typename Key, 
    typename Value, 
    typename Hash
> 
class ARCCacheLevel
{
    public:
        using Entry = CacheEntry<Key, Value>;
        using Meta = ARCMeta<Key>;
        using Transfer = CacheTransfer<Entry, Meta>;

    private:
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

        struct MiddleStruct
        {
            std::optional <std::pair<Key, Value> > evict;
            Value* inser;
        };
    //_________________________________________________________________________________________________________________________________________//

        MiddleStruct add_in_TN (Node_ARC_T_& n_value,
                        size_t size_of_list_T,
                        size_t size_of_list_B,
                        std::list<Node_ARC_T_>& T_T_list,
                        std::list<Node_ARC_B_>& B_B_list,
                        std::unordered_map <Key, typename std::list<Node_ARC_T_>::iterator, Hash>& T_T_Htable,
                        std::unordered_map <Key, typename std::list<Node_ARC_B_>::iterator, Hash>& B_B_Htable  )
        {
            std::optional <std::pair<Key, Value> > last_elem = std::nullopt;

            ////////////////ADD/////////////////////
            T_T_list.push_front(std::move(n_value));
            auto inserted_it = T_T_list.begin();
            T_T_Htable.emplace(inserted_it->key, inserted_it);
            ////////////////////////////////////////

            //////////////CHECK_SIZE////////////////
            if (T_T_list.size() > size_of_list_T)      // move last of TN -> BN
            {
                typename std::list<Node_ARC_T_>::iterator it_last = std::prev(T_T_list.end());   // check it of last in list
                last_elem.emplace(std::move(it_last->key), std::move(it_last->value));

                remove_out_TN (last_elem->first, size_of_list_B, T_T_list, B_B_list, T_T_Htable, B_B_Htable);  // delete out of list+Ht (TN)
            }
            ////////////////////////////////////////

            return {std::move(last_elem), &inserted_it->value};
        }

        void add_in_BN (Node_ARC_B_ n_key, size_t size_of_list_B,
                                            std::list<Node_ARC_B_>& B_B_list,
                                            std::unordered_map <Key, typename std::list<Node_ARC_B_>::iterator, Hash>& B_B_Htable )
        {
            //////////////CHECK_SIZE////////////////
            if (B_B_list.size() >= size_of_list_B)
            {
                typename std::list<Node_ARC_B_>::iterator it_last = std::prev(B_B_list.end());   // check it of last in list
                remove_out_BN (it_last->key, B_B_list, B_B_Htable);
            }
            ////////////////////////////////////////

            ////////////////ADD/////////////////////
            B_B_list.push_front(n_key);
            B_B_Htable.emplace(n_key.key, B_B_list.begin());  // add key, value (= it in list)
            ////////////////////////////////////////
        }

        void remove_out_TN (const Key key,
                            size_t size_of_list_B,
                            std::list<Node_ARC_T_>& T_T_list,
                            std::list<Node_ARC_B_>& B_B_list,
                            std::unordered_map <Key, typename std::list<Node_ARC_T_>::iterator, Hash>& T_T_Htable,
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

        void remove_out_BN (const Key key,
                            std::list<Node_ARC_B_>& B_B_list,
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
    
    //_________________________________________________________________________________________________________________________________________//
    public:
        // extract change size + not delete in B
        // get change size + not delete in B (it is work for add)
        // add not change sizes + delete in B and check need li to T2
        // contains just check

        template <typename AnyMeta>
        AddResult<Transfer, Value>
        add(CacheTransfer<Entry, AnyMeta> came)
        {
            ///////////////////////////////META/////////////////////////////////////////
            Meta meta{};

            if constexpr (IsCompatibleARC<AnyMeta, Key>)
                meta.for_up = came.meta.for_up;

            Transfer elem
            {
                Entry{
                    std::move(came.entry.key),
                    std::move(came.entry.value) },
                std::move(meta)
            };
            ////////////////////////////////////////////////////////////////////////////

            /////////////////////////////////INIT///////////////////////////////////////
            Node_ARC_T_ n_value = {elem.entry.key, std::move(elem.entry.value)};

            MiddleStruct value_of_last_it_add{};  // for ret вытеснутого
            std::optional<Transfer> evicted = std::nullopt;
            ////////////////////////////////////////////////////////////////////////////

            auto it_B1_Ht = B1_Htable_.find(elem.entry.key);
            if (it_B1_Ht != B1_Htable_.end())
            {
                remove_out_BN (elem.entry.key, B1_list_, B1_Htable_);
                elem.meta.for_up.emplace(elem.entry.key);
            }

            auto it_B2_Ht = B2_Htable_.find(elem.entry.key);
            if (it_B2_Ht != B2_Htable_.end())
            {
                remove_out_BN (elem.entry.key, B2_list_, B2_Htable_);
                elem.meta.for_up.emplace(elem.entry.key);
            }
            
            ///////////////////////////////////ADD//////////////////////////////////////
            if (elem.meta.for_up == elem.entry.key)  // add element in T2 because / level up / default search and found in any B /
            {
                elem.meta.for_up.reset();

                value_of_last_it_add = add_in_TN (n_value, (size_ - size_T1_), size_B2_, T2_list_, B2_list_, T2_Htable_, B2_Htable_);
                if (value_of_last_it_add.evict == std::nullopt && T1_list_.size() > size_T1_)
                {
                    Node_ARC_T_ last_of_T1 = std::move(T1_list_.back());   // take key of last for delete with func T1
                    evicted.emplace(Transfer { Entry {std::move(last_of_T1.key), 
                                                      std::move(last_of_T1.value)},
                                                      Meta{std::nullopt} });

                    remove_out_TN (evicted->entry.key, size_B1_, T1_list_, B1_list_, T1_Htable_, B1_Htable_);  // T1 be lower -> need delete in T1 -> go to B1
                }
                else if (value_of_last_it_add.evict)
                    {
                        evicted.emplace(Transfer { Entry {std::move(value_of_last_it_add.evict->first), 
                                                          std::move(value_of_last_it_add.evict->second)}, 
                                                          Meta{} });
                        evicted->meta.for_up = evicted->entry.key;
                    }
            }
            else
            {
                value_of_last_it_add = add_in_TN (n_value, size_T1_, size_B1_, T1_list_, B1_list_, T1_Htable_, B1_Htable_);  // no T2 --> T1
                if (value_of_last_it_add.evict == std::nullopt && T2_list_.size() > (size_ - size_T1_))
                {
                    Node_ARC_T_ last_of_T2 = std::move(T2_list_.back());   // take key of last for delete with func T1
                    evicted.emplace( Transfer{ Entry {std::move(last_of_T2.key), 
                                                      std::move(last_of_T2.value)},
                                                      Meta{} });
                    evicted->meta.for_up = evicted->entry.key;

                    remove_out_TN (evicted->entry.key, size_B2_, T2_list_, B2_list_, T2_Htable_, B2_Htable_);  // T2 be lower -> need delete in T2 -> go to B2
                }
                else if (value_of_last_it_add.evict)
                         evicted.emplace( Transfer{ Entry {std::move(value_of_last_it_add.evict->first), 
                                                           std::move(value_of_last_it_add.evict->second)}, 
                                                           Meta{std::nullopt} });
            }
            ////////////////////////////////////////////////////////////////////////////
            return {std::move(evicted), value_of_last_it_add.inser};
            ////////////////////////////////////////////////////////////////////////////
        }

        AddResult<Transfer, Value> add(Entry entry)
        {
            return add(Transfer
                        { Entry {std::move(entry)}, {} });
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
                if (size_T1_ < size_ - 1)
                    size_T1_++;              // need more space to T1

                return nullptr;
            }

            auto it_B2_Ht = B2_Htable_.find(key);
            if (it_B2_Ht != B2_Htable_.end())  // already in B2
            {
                if (size_T1_ > 1)
                    size_T1_--;              // need more space to T2

                return nullptr;
            }
            ////////////////////////////////////////////////////////////////////////////


            ////////////////////////////////MISS////////////////////////////////////////
            return nullptr;
            ////////////////////////////////////////////////////////////////////////////
        }

        bool contains(const Key& key) const
        {
            return T1_Htable_.find(key) != T1_Htable_.end()
                || T2_Htable_.find(key) != T2_Htable_.end();
        }

        void remove (const Key& key)  // full delete in all possible lines
        {
            ///////////////////////////////FREE_T///////////////////////////////////////
            auto it_T1_Ht = T1_Htable_.find(key);
            if (it_T1_Ht != T1_Htable_.end())  // already in T1
            {
                T1_list_.erase(it_T1_Ht->second);
                T1_Htable_.erase(it_T1_Ht);

                return;
            }

            auto it_T2_Ht = T2_Htable_.find(key);
            if (it_T2_Ht != T2_Htable_.end())  // already in T2
            {
                T2_list_.erase(it_T2_Ht->second);
                T2_Htable_.erase(it_T2_Ht);

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

            return;
        }
        
        std::optional<Transfer> extract(const Key& key)  // for level up  // работает как get => need change size
        {
            ////////////////////////////////////////////////////////////////////////////
            auto it_T1_Ht = T1_Htable_.find(key);
            if (it_T1_Ht != T1_Htable_.end())  // already in T1
            {
                Transfer need_ret( Entry {std::move(it_T1_Ht->second->key),
                                          std::move(it_T1_Ht->second->value)}, 
                                          Meta{} );

                T1_list_.erase(it_T1_Ht->second);
                T1_Htable_.erase(it_T1_Ht);

                return need_ret;
            }
            ////////////////////////////////////////////////////////////////////////////


            ////////////////////////////////////////////////////////////////////////////
            auto it_T2_Ht = T2_Htable_.find(key);
            if (it_T2_Ht != T2_Htable_.end())  // already in T2
            {
                Transfer need_ret( Entry {std::move(it_T2_Ht->second->key),
                                          std::move(it_T2_Ht->second->value)}, 
                                          Meta{std::move(key)} );

                T2_list_.erase(it_T2_Ht->second);
                T2_Htable_.erase(it_T2_Ht);

                return need_ret;
            }
            ////////////////////////////////////////////////////////////////////////////


            ///////////////////////////////B_lines//////////////////////////////////////
            auto it_B1_Ht = B1_Htable_.find(key);
            if (it_B1_Ht != B1_Htable_.end())  // already in B1
            {
                if (size_T1_ < size_ - 1)
                    size_T1_++;              // need more space to T1
            
                return std::nullopt;
            }

            auto it_B2_Ht = B2_Htable_.find(key);
            if (it_B2_Ht != B2_Htable_.end())  // already in B2
            {
                if (size_T1_ > 1)
                    size_T1_--;              // need more space to T2

                return std::nullopt;
            }
            ////////////////////////////////////////////////////////////////////////////

            return std::nullopt;
        }

        void dump(std::ostream& out) const
        {
            out << "  [ ARC Policy State ]\n"
                << "    Size      : "
                << T1_list_.size() + T2_list_.size()
                << '/' << size_ << '\n'
                << "    T1 target : " << size_T1_ << '\n'
                << "    T2 target : " << size_ - size_T1_ << '\n'
                << "    T1 size   : " << T1_list_.size() << '\n'
                << "    T2 size   : " << T2_list_.size() << '\n'
                << "    B1 size   : "
                << B1_list_.size() << '/' << size_B1_ << '\n'
                << "    B2 size   : "
                << B2_list_.size() << '/' << size_B2_ << '\n';

            const auto dump_list =
                [&out](const char* name, const auto& list)
                {
                    out << "\n  [ " << name << " (MRU -> LRU) ]\n"
                        << "  --------------------------------\n";

                    if (list.empty())
                    {
                        out << "      (empty)\n";
                    }
                    else
                    {
                        std::size_t index = 1;
                        for (const auto& node : list)
                        {
                            out << "      " << index++
                                << ". [" << node.key << "]\n";
                        }
                    }

                    out << "  --------------------------------\n";
                };

            dump_list("T1 Queue", T1_list_);
            dump_list("T2 Queue", T2_list_);
            dump_list("B1 Ghost Queue", B1_list_);
            dump_list("B2 Ghost Queue", B2_list_);
        }

    //_________________________________________________________________________________________________________________________________________//

        ARCCacheLevel(const OptionsARC& options, Hash hash_func) :
            size_(options.sz_of_summ_of_T),
            size_T1_(options.sz_of_T_one),

            size_B1_(options.sz_of_B_one),
            size_B2_(options.sz_of_B_two),

            T1_Htable_(0, hash_func),
            T2_Htable_(0, hash_func),
            B1_Htable_(0, hash_func),
            B2_Htable_(0, hash_func)
        {
            if (size_ < 2 || size_T1_ == 0 || size_T1_ >= size_ || size_B1_ == 0 || size_B2_ == 0)
            {
                throw std::invalid_argument(
                    "ARC requires capacity >= 2, "
                    "0 < T1 < capacity, and positive ghost capacities"
                );
            }

            T1_Htable_.reserve(size_T1_);
            T2_Htable_.reserve(size_ - size_T1_);
            B1_Htable_.reserve(size_B1_);
            B2_Htable_.reserve(size_B2_);
        }
        ARCCacheLevel(size_t capacity, Hash hash_func) :
            size_(capacity),
            size_T1_(capacity / 2),

            size_B1_(capacity / 2),
            size_B2_(capacity / 2),

            T1_Htable_(0, hash_func),
            T2_Htable_(0, hash_func),
            B1_Htable_(0, hash_func),
            B2_Htable_(0, hash_func)
        {
            if (size_ < 2)
                throw std::invalid_argument("need size of cache ARC > 0");

            T1_Htable_.reserve(size_T1_);
            T2_Htable_.reserve(size_ - size_T1_);
            B1_Htable_.reserve(size_B1_);
            B2_Htable_.reserve(size_B2_);
        }
};

inline LevelSpec<ARCCacheLevel, OptionsARC>
ARC(size_t sz_of_summ_of_T, size_t sz_of_T_one, size_t sz_of_B_one, size_t sz_of_B_two)
{
    return level<ARCCacheLevel, OptionsARC>(OptionsARC{sz_of_summ_of_T, sz_of_T_one, sz_of_B_one, sz_of_B_two});
}

inline LevelSpec<ARCCacheLevel, OptionsARC>
ARC(size_t capacity)
{
    return level<ARCCacheLevel, OptionsARC>(
    OptionsARC {capacity,
                capacity / 2,
                capacity / 2,
                capacity / 2});
}

}

#endif // ARC_POLICY_HH
#ifndef TWO_Q_POLICY_HH
#define TWO_Q_POLICY_HH

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

#include "lru_policy.hpp"

namespace MyCache::policy
{

struct OptionsTwoQ
{
    size_t size_of_A1_cache;
    size_t size_of_Am_cache;

    size_t size_of_A1out_cache;
};

struct TwoQTag {};

template <typename Key>
struct TwoQMetaFull
{
    std::optional< Key > for_up;
    using compatibility = TwoQTag; 
};

template <typename AnyMeta, typename Key>
concept IsCompatible2Q = requires(const AnyMeta& meta)
{
    typename AnyMeta::compatibility;

    requires std::same_as<typename AnyMeta::compatibility, TwoQTag>;

    {meta.for_up} -> std::convertible_to<std::optional<Key>>;
};


// ======================================================= 2Q cachelevel class ==================================================
template <
    typename Key, 
    typename Value, 
    typename Hash
>
class TwoQCacheLevel
{
    public:
        using TwoQMeta = TwoQMetaFull<Key>;
        using Entry = CacheEntry<Key, Value>;
        using Transfer = CacheTransfer<Entry, TwoQMeta>;
    
    private:
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
        LRUCacheLevel <Key, Value, Hash> Am_LRU_;

        struct MiddleStruct
        {
            std::optional <std::pair<Key, Value> > evict;
            Value* inser;
        };

    //_________________________________________________________________________________________________________________________________________//
        
        MiddleStruct add_to_A1 (Node_2Q_A1_ n_value)
        {
            std::optional <std::pair<Key, Value> > value_of_last = std::nullopt;

            //////////////CHECK_SIZE////////////////    
            if (A1_list_.size() >= size_A1_)
            {
                typename std::list<Node_2Q_A1_>::iterator it_last = std::prev(A1_list_.end());  // check it of last in list
                
                value_of_last.emplace(
                                        std::move(it_last->key),
                                        std::move(it_last->value));

                remove_out_A1(value_of_last->first);
            }
            ////////////////////////////////////////


            //////////////////ADD///////////////////            
            A1_list_.push_front(std::move(n_value));                     // add new value in list
            auto inserted_it = A1_list_.begin();
            A1_Htable_.emplace(inserted_it->key, inserted_it);  // add new it in Ht
            ////////////////////////////////////////
            
            return {std::move(value_of_last), &inserted_it->value};
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
            A1out_list_.push_front(n_key);                          // add new key in list
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

        // extract not delete in A1out
        // get not delete in A1out (it is work for add)
        // add delete in A1out and check need li to Am
        // contains just check

        template <typename AnyMeta>
        AddResult<Transfer, Value>
        add(CacheTransfer<Entry, AnyMeta> came)
        {
            ///////////////////////////////META/////////////////////////////////////////
            TwoQMeta meta{};

            if constexpr (IsCompatible2Q<AnyMeta, Key>)
                meta.for_up = came.meta.for_up;

            Transfer elem
            {
                Entry{
                    std::move(came.entry.key),
                    std::move(came.entry.value) },
                std::move(meta)
            };
            ////////////////////////////////////////////////////////////////////////////


            ////////////////UPPER///////////////////
            auto it_A1_out_Ht = A1out_Htable_.find(elem.entry.key);
            if (it_A1_out_Ht != A1out_Htable_.end())
            {
                remove_out_A1_out(it_A1_out_Ht);
                elem.meta.for_up.emplace(elem.entry.key);
            }

            if (elem.meta.for_up == elem.entry.key)  // => go to Am
            {
                auto last_ret_add = Am_LRU_.add(std::move(elem.entry));
                std::optional<Transfer> evicted = std::nullopt;

                if (last_ret_add.evicted)
                {
                    evicted.emplace(
                        Transfer{ Entry {
                                        std::move(last_ret_add.evicted->entry.key),
                                        std::move(last_ret_add.evicted->entry.value)}, 
                                        TwoQMeta{}
                        }
                    );

                    evicted->meta.for_up = evicted->entry.key;
                }
                else
                    elem.meta.for_up.reset();

                return {std::move(evicted), last_ret_add.inserted};
            }
            ////////////////////////////////////////


            //////////////////A1///////////////////            
            Node_2Q_A1_ n_value = {elem.entry.key, std::move(elem.entry.value)};      
            MiddleStruct last_val = add_to_A1 (std::move(n_value));

            std::optional<Transfer> evicted = std::nullopt;
            if (last_val.evict != std::nullopt)
                evicted.emplace(Transfer {Entry {std::move(last_val.evict->first), 
                                                 std::move(last_val.evict->second)}, 
                                                 TwoQMeta{}});

            return {std::move(evicted), last_val.inser};
            ////////////////////////////////////////
        }

        AddResult<Transfer, Value> add(Entry entry)
        {
            return add(Transfer{std::move(entry), TwoQMeta{}});
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


            // cache-miss // A1_out
            return nullptr;
        }

        bool contains(const Key& key) const
        {
            return A1_Htable_.find(key) != A1_Htable_.end()
                || Am_LRU_.contains(key);
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

        std::optional<Transfer> extract(const Key& key)
        {
            /////////////////A1/////////////////////
            auto it_A1 = A1_Htable_.find(key);
            if (it_A1 != A1_Htable_.end())
            {
                Transfer result{ Entry {std::move(it_A1->second->key),
                                        std::move(it_A1->second->value)}, 
                                        TwoQMeta{}};

                A1_list_.erase(it_A1->second);
                A1_Htable_.erase(it_A1);

                return result;
            }
            ////////////////////////////////////////


            ////////////////A1_out//////////////////
            auto it_A1out = A1out_Htable_.find(key);
            if (it_A1out != A1out_Htable_.end())
                return std::nullopt;
            ////////////////////////////////////////


            //////////////////Am////////////////////
            auto inner = Am_LRU_.extract(key);
            if (!inner)
                return std::nullopt;

            return Transfer{std::move(inner->entry), TwoQMeta{key}};
            ////////////////////////////////////////
        }

        void dump(std::ostream& out) const
        {
            out << "  [ 2Q Policy State ]\n"
                << "    Capacity  : " << size_A1_ + size_Am_ << '\n'
                << "    A1 size   : " << A1_list_.size()
                << '/' << size_A1_ << '\n'
                << "    Am limit  : " << size_Am_ << '\n'
                << "    A1out size: " << A1out_list_.size()
                << '/' << size_A1out_ << '\n';

            const auto dump_list =
                [&out](const char* name, const auto& list)
                {
                    out << "\n  [ " << name << " (newest -> oldest) ]\n"
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

            dump_list("A1 FIFO Queue", A1_list_);

            out << "\n  [ Am LRU Cache ]\n";
            Am_LRU_.dump(out);

            dump_list("A1out Ghost Queue", A1out_list_);
        }

    //_________________________________________________________________________________________________________________________________________//

        TwoQCacheLevel(const OptionsTwoQ& options, Hash hash_func) :        
            size_A1_(options.size_of_A1_cache),  
            size_Am_(options.size_of_Am_cache),    
            size_A1out_(options.size_of_A1out_cache),  

            A1_Htable_(size_A1_, hash_func),
            A1out_Htable_(size_A1out_, hash_func),
            Am_LRU_(OptionsLRU{size_Am_}, hash_func)
        {
            if ( (size_A1_ < 1) || (size_Am_ < 1) || (size_A1out_ < 1))
                throw std::invalid_argument("need size of cache 2Q > 0");
        }

        TwoQCacheLevel(size_t capacity, Hash hash_func) :        
            size_A1_(capacity / 4),  
            size_Am_(capacity - capacity / 4),
            size_A1out_(capacity),  

            A1_Htable_(size_A1_, hash_func),
            A1out_Htable_(size_A1out_, hash_func),
            Am_LRU_(OptionsLRU{size_Am_}, hash_func)
        {
            if ( (capacity < 4))
                throw std::invalid_argument("need size of cache 2Q > 0");
        }
};

inline LevelSpec<TwoQCacheLevel, OptionsTwoQ>
TwoQ(size_t size_of_A1_cache, size_t size_of_Am_cache, size_t size_of_A1out_cache)
{
    return level<TwoQCacheLevel, OptionsTwoQ>(OptionsTwoQ{size_of_A1_cache, size_of_Am_cache, size_of_A1out_cache});
}

inline LevelSpec<TwoQCacheLevel, OptionsTwoQ>
TwoQ(size_t capacity)
{
    if (capacity < 4)
        throw std::invalid_argument("need capacity of cache 2Q >= 4");

    return level<TwoQCacheLevel, OptionsTwoQ>(OptionsTwoQ{capacity / 4, capacity - capacity / 4, capacity});
}

}

#endif // TWO_Q_POLICY_HH
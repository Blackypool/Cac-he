#ifndef LFU_POLICY_HH
#define LFU_POLICY_HH

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

struct LFUTag {};
struct LFUMeta
{
    size_t freq;
    using compatibility = LFUTag;
};

template<typename AnyMeta>
concept IsCompatibleLFU = requires(AnyMeta meta)
{
    typename AnyMeta::compatibility;
    
    requires std::same_as<typename AnyMeta::compatibility, LFUTag>;

    {meta.freq} -> std::convertible_to<size_t>;
};

// =================================================== LFU cachelevel class ================================================
template <
    typename Value, 
    typename Key, 
    typename Extractor, 
    typename Hash
> 
class LFUCacheLevel
{   
    private:

        struct ListNode
        {
            Value value;
            LFUMeta meta;
        };

        using UMD = std::list<ListNode>;
        using UMP = std::list<ListNode>::iterator;

        size_t cur_size_ = 0;
        size_t min_freq_ = 0;
        size_t capacity_;
        Extractor get_key_;

        std::unordered_map <Key, UMP, Hash> table_p_;
        std::unordered_map <size_t, UMD> table_d_;

        template <typename AnyMeta>
        size_t extract_freq(const AnyMeta& meta)
        {
            if constexpr (IsCompatibleLFU<AnyMeta>)
            {
                return meta.freq;
            }

            else return 1;
        }

        ListNode replace_elem()
        {
            UMD& list = table_d_[min_freq_];
            auto it_d = --list.end();

            Key key = get_key_(it_d->value);
            auto evicted = std::move(*it_d);

            table_p_.erase(key);
            list.erase(it_d);
            --cur_size_;

            if (list.empty())
            {
                update_min_freq();
            }

            return evicted;
        }

        void update_min_freq()
        {
            if (cur_size_== 0)
            {
                min_freq_ = 0;
                return;
            }

            size_t new_freq = std::numeric_limits<size_t>::max();

            for (const auto& [freq, list] : table_d_)
            {
                if (!list.empty() && freq < new_freq)
                {
                    new_freq = freq;
                }
            }

            min_freq_ = new_freq;
        }

    public:

        LFUCacheLevel (size_t size, Extractor key, Hash hash_func)  
            : capacity_(size),
              get_key_ (key) ,
              table_p_ (size, hash_func),
              table_d_ (size) 
        {}

        using Transfer = CacheTransfer<Value, LFUMeta>;

        struct AddResult
        {
            std::optional<Transfer> evicted;
            Value* inserted;
        };

        AddResult add(Value value)
        {   
            Transfer new_elem = {std::move(value), {1}};
            return add(std::move(new_elem));
        }

        template <typename AnyMeta>
        AddResult add(CacheTransfer<Value, AnyMeta> elem)
        {
            size_t freq = extract_freq(elem.meta);
            auto& list = table_d_[freq];
            std::optional<Transfer> evicted;

            if (cur_size_ >= capacity_)
            {
                ListNode old_node = replace_elem();
                evicted = Transfer {
                    std::move(old_node.value),
                    LFUMeta{old_node.meta.freq}
                };
            }

            ListNode insert = {std::move(elem.value), {freq}};
            list.push_front(std::move(insert));
            UMP it  = list.begin();
            Key key = get_key_(it->value);

            table_p_.emplace(key, it);
            if (freq < min_freq_ || min_freq_ == 0)
            {
                min_freq_ = freq;
            }
            
            cur_size_++;
            return {std::move(evicted), &(it->value)};
        }

        bool find(const Key& key) const
        {
            return table_p_.find(key) != table_p_.end();
        }

        std::optional<Transfer> extract(const Key& key)
        {
            auto it_p = table_p_.find(key);
            if (it_p == table_p_.end())
            {
                return std::nullopt;
            }

            UMP it_d = it_p->second;
            Transfer extract = {
                std::move(it_d->value),
                {it_d->meta.freq}
            };

            size_t freq = it_d->meta.freq;
            auto&  list = table_d_[freq];

            table_p_.erase(it_p);
            list.erase(it_d);
            --cur_size_;

            if (list.empty() && freq == min_freq_)
            {
                update_min_freq();
            }

            return extract;
        }

        Value* get(const Key& key)
        {
            auto it_p = table_p_.find(key);
            if (it_p == table_p_.end()) 
            {
                return nullptr;
            } 
            
            UMP it_d = it_p->second;
            
            size_t freq = it_d->meta.freq;
            auto& list_from = table_d_[freq];
            auto& list_to   = table_d_[++it_d->meta.freq];

            list_to.splice(
                list_to.begin(),
                list_from,
                it_d
            );

            if (list_from.empty())
            {
                table_d_.erase(freq);

                if (min_freq_ == freq)
                {
                    update_min_freq();
                }
            }

            return &(it_d->value);
        }

        void remove(const Key& key)
        {
            auto it_p = table_p_.find(key);
            if (it_p == table_p_.end()) return;
            
            UMP it_d = it_p->second;
            size_t freq = it_d->meta.freq;
            auto& list = table_d_[freq];

            table_p_.erase(it_p);
            list.erase(it_d);
            --cur_size_;
            
            if (list.empty() && freq == min_freq_)
            {
                update_min_freq();
            }
        } 
};


}

#endif // LFU_POLICY_HH
#ifndef LIRS_POLICY_HH
#define LIRS_POLICY_HH

#include "cache.hpp"

#include <concepts>
#include <cstddef>
#include <list>
#include <limits>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "types.h"

namespace MyCache {

struct LIRSTag {};

struct LIRSMeta
{
    bool is_lir;
    using compatibility = LIRSTag;
};

template <typename AnyMeta>
concept IsCompatibleLIRS = requires(const AnyMeta& meta)
{
    typename AnyMeta::compatibility;

    requires std::same_as<typename AnyMeta::compatibility, LIRSTag>;

    { meta.is_lir } -> std::convertible_to<bool>;
};

// =================================================== LIRS cachelevel class ===============================================
template <
    typename Value,
    typename Key,
    typename Extractor,
    typename Hash
>
class LIRSCacheLevel
{
    private:

        using KeyList = std::list<Key>;
        using KeyIterator = typename KeyList::iterator;

        struct ResidentNode
        {
            Value value;
            bool is_lir = false;

            bool in_stack = false;
            KeyIterator stack_pos;

            bool in_queue = false;
            KeyIterator queue_pos;
        };

        using ResidentMap = std::unordered_map<Key, ResidentNode, Hash>;
        using ResidentIterator = typename ResidentMap::iterator;

        std::size_t capacity_;
        std::size_t lir_capacity_;
        std::size_t lir_size_ = 0;

        Extractor get_key_;

        // Stack S: live LIR, live HIR and non-resident HIR (ghost) keys.
        KeyList stack_;
        std::unordered_map<Key, KeyIterator, Hash> stack_pos_;

        // Queue Q: only live HIR keys. Front is MRU, back is eviction candidate.
        KeyList hir_queue_;
        ResidentMap resident_;

        template <typename AnyMeta>
        bool extract_lir_hint(const AnyMeta& meta) const
        {
            if constexpr (IsCompatibleLIRS<AnyMeta>)
            {
                return static_cast<bool>(meta.is_lir);
            }

            return false;
        }

        void move_stack_to_front(const Key& key)
        {
            auto pos = stack_pos_.find(key);

            if (pos == stack_pos_.end())
            {
                stack_.push_front(key);
                pos = stack_pos_.emplace(key, stack_.begin()).first;
            }
            else
            {
                stack_.splice(stack_.begin(), stack_, pos->second);
            }

            auto resident = resident_.find(key);
            if (resident != resident_.end())
            {
                resident->second.in_stack = true;
                resident->second.stack_pos = pos->second;
            }
        }

        void move_queue_to_front(ResidentIterator resident)
        {
            if (resident->second.in_queue)
            {
                hir_queue_.splice(
                    hir_queue_.begin(),
                    hir_queue_,
                    resident->second.queue_pos
                );
            }
            else
            {
                hir_queue_.push_front(resident->first);
                resident->second.in_queue = true;
                resident->second.queue_pos = hir_queue_.begin();
            }
        }

        void remove_from_queue(ResidentIterator resident)
        {
            if (!resident->second.in_queue)
            {
                return;
            }

            hir_queue_.erase(resident->second.queue_pos);
            resident->second.in_queue = false;
        }

        void prune_stack()
        {
            while (!stack_.empty())
            {
                Key key = stack_.back();
                auto resident = resident_.find(key);

                if (resident != resident_.end() && resident->second.is_lir)
                {
                    break;
                }

                if (resident != resident_.end())
                {
                    resident->second.in_stack = false;
                }

                stack_pos_.erase(key);
                stack_.pop_back();
            }

            // A pure sequential scan must not grow ghost history forever.
            const std::size_t history_limit =
                capacity_ > std::numeric_limits<std::size_t>::max() / 2
                    ? std::numeric_limits<std::size_t>::max()
                    : capacity_ * 2;

            while (stack_.size() > history_limit)
            {
                bool ghost_removed = false;
                auto pos = stack_.end();

                while (pos != stack_.begin())
                {
                    --pos;

                    if (resident_.find(*pos) == resident_.end())
                    {
                        Key ghost_key = *pos;
                        stack_pos_.erase(ghost_key);
                        stack_.erase(pos);
                        ghost_removed = true;
                        break;
                    }
                }

                if (!ghost_removed)
                {
                    break;
                }
            }
        }

        bool demote_bottom_lir(const Key* protected_key)
        {
            for (auto pos = stack_.rbegin(); pos != stack_.rend(); ++pos)
            {
                if (protected_key != nullptr && *pos == *protected_key)
                {
                    continue;
                }

                auto resident = resident_.find(*pos);
                if (resident == resident_.end() || !resident->second.is_lir)
                {
                    continue;
                }

                resident->second.is_lir = false;
                --lir_size_;
                move_queue_to_front(resident);
                prune_stack();
                return true;
            }

            return false;
        }

        void promote_to_lir(ResidentIterator resident)
        {
            move_stack_to_front(resident->first);

            if (lir_capacity_ == 0)
            {
                move_queue_to_front(resident);
                prune_stack();
                return;
            }

            remove_from_queue(resident);

            if (!resident->second.is_lir)
            {
                resident->second.is_lir = true;
                ++lir_size_;
            }

            if (lir_size_ > lir_capacity_)
            {
                demote_bottom_lir(&resident->first);
            }

            prune_stack();
        }

        void fill_lir_part()
        {
            while (lir_size_ < lir_capacity_ && !hir_queue_.empty())
            {
                Key key = hir_queue_.front();
                auto resident = resident_.find(key);

                if (resident == resident_.end())
                {
                    hir_queue_.pop_front();
                    continue;
                }

                remove_from_queue(resident);
                resident->second.is_lir = true;
                ++lir_size_;
                move_stack_to_front(key);
            }

            prune_stack();
        }

        std::optional<CacheTransfer<Value, LIRSMeta>> evict_hir_if_needed()
        {
            if (resident_.size() <= capacity_)
            {
                return std::nullopt;
            }

            if (hir_queue_.empty())
            {
                demote_bottom_lir(nullptr);
            }

            if (hir_queue_.empty())
            {
                throw std::logic_error("LIRS invariant is broken: no HIR victim");
            }

            Key victim_key = hir_queue_.back();
            auto victim = resident_.find(victim_key);

            if (victim == resident_.end())
            {
                throw std::logic_error("LIRS invariant is broken: stale HIR key");
            }

            hir_queue_.pop_back();
            victim->second.in_queue = false;

            CacheTransfer<Value, LIRSMeta> evicted {
                std::move(victim->second.value),
                LIRSMeta{victim->second.is_lir}
            };

            resident_.erase(victim);
            prune_stack();
            return evicted;
        }

    public:

        using Transfer = CacheTransfer<Value, LIRSMeta>;

        struct AddResult
        {
            std::optional<Transfer> evicted;
            Value* inserted;
        };

        LIRSCacheLevel(
            std::size_t capacity,
            Extractor get_key,
            Hash hash_func
        )
            : capacity_(capacity),
              lir_capacity_(capacity > 1 ? capacity - 1 : 0),
              get_key_(std::move(get_key)),
              stack_pos_(capacity + 1, hash_func),
              resident_(capacity, std::move(hash_func))
        {
            if (capacity_ == 0)
            {
                throw std::invalid_argument("LIRS cache capacity must be positive");
            }
        }

        LIRSCacheLevel(const LIRSCacheLevel&) = delete;
        LIRSCacheLevel& operator=(const LIRSCacheLevel&) = delete;

        AddResult add(Value value)
        {
            return add(
                CacheTransfer<Value, EmptyMeta> {
                    std::move(value), {}
                }
            );
        }

        template <typename AnyMeta>
        AddResult add(CacheTransfer<Value, AnyMeta> elem)
        {
            Key key = get_key_(elem.value);

            auto old = resident_.find(key);
            if (old != resident_.end())
            {
                Value* inserted = get(key);
                return {std::nullopt, inserted};
            }

            const bool was_in_stack = stack_pos_.find(key) != stack_pos_.end();
            const bool lir_hint = extract_lir_hint(elem.meta);

            auto [resident, was_inserted] = resident_.emplace(
                key,
                ResidentNode {
                    std::move(elem.value),
                    false,
                    false,
                    KeyIterator{},
                    false,
                    KeyIterator{}
                }
            );

            if (!was_inserted)
            {
                return {std::nullopt, &(resident->second.value)};
            }

            move_stack_to_front(key);

            if (lir_size_ < lir_capacity_)
            {
                resident->second.is_lir = true;
                ++lir_size_;
            }
            else if ((was_in_stack || lir_hint) && lir_capacity_ != 0)
            {
                promote_to_lir(resident);
            }
            else
            {
                move_queue_to_front(resident);
            }

            std::optional<Transfer> evicted = evict_hir_if_needed();
            auto inserted = resident_.find(key);

            return {
                std::move(evicted),
                inserted == resident_.end() ? nullptr : &(inserted->second.value)
            };
        }

        bool find(const Key& key) const
        {
            return resident_.find(key) != resident_.end();
        }

        Value* get(const Key& key)
        {
            auto resident = resident_.find(key);
            if (resident == resident_.end())
            {
                return nullptr;
            }

            if (resident->second.is_lir)
            {
                move_stack_to_front(key);
                prune_stack();
            }
            else if (resident->second.in_stack)
            {
                promote_to_lir(resident);
            }
            else
            {
                move_stack_to_front(key);
                move_queue_to_front(resident);
                prune_stack();
            }

            return &(resident->second.value);
        }

        std::optional<Transfer> extract(const Key& key)
        {
            auto resident = resident_.find(key);
            if (resident == resident_.end())
            {
                return std::nullopt;
            }

            const bool was_lir = resident->second.is_lir;
            Transfer extracted {
                std::move(resident->second.value),
                LIRSMeta{was_lir}
            };

            remove_from_queue(resident);

            if (was_lir)
            {
                --lir_size_;
            }

            // The key stays in stack S as a non-resident HIR history entry.
            resident_.erase(resident);
            prune_stack();
            fill_lir_part();
            return extracted;
        }

        void remove(const Key& key)
        {
            auto resident = resident_.find(key);
            if (resident != resident_.end())
            {
                const bool was_lir = resident->second.is_lir;
                remove_from_queue(resident);
                resident_.erase(resident);

                if (was_lir)
                {
                    --lir_size_;
                }
            }

            auto stack_pos = stack_pos_.find(key);
            if (stack_pos != stack_pos_.end())
            {
                stack_.erase(stack_pos->second);
                stack_pos_.erase(stack_pos);
            }

            fill_lir_part();
            prune_stack();
        }

        void dump(std::ostream& out) const
        {
            out << "LIRS level\n";
            out << "size: " << resident_.size() << '\n';
            out << "LIR: " << lir_size_ << '/' << lir_capacity_ << '\n';

            out << "stack S: ";
            for (const Key& key : stack_)
            {
                auto resident = resident_.find(key);

                if (resident == resident_.end())
                {
                    out << key << "(ghost) ";
                }
                else
                {
                    out << key
                        << (resident->second.is_lir ? "(LIR) " : "(HIR) ");
                }
            }
            out << '\n';

            out << "queue Q: ";
            for (const Key& key : hir_queue_)
            {
                out << key << "(HIR) ";
            }
            out << '\n';
        }
};

} // namespace MyCache

#endif // LIRS_POLICY_HH

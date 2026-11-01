#ifndef LIRS_POLICY_HH
#define LIRS_POLICY_HH

#include "cache.hpp"

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <iomanip>
#include <iterator>
#include <list>
#include <optional>
#include <ostream>
#include <stdexcept>
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

struct OptionsLIRS
{
    std::size_t capacity;
    std::size_t hir_capacity;
    std::size_t history_capacity;
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
    typename Key,
    typename Value,
    typename Hash
>
class LIRSCacheLevel
{
    private:

        using KeyList = std::list<Key>;
        using KeyIterator = typename KeyList::iterator;
        using 

        struct ResidentNode
        {
            Value value;
            bool is_lir = false;

            bool in_queue = false;
            KeyIterator queue_pos;
        };

        using ResidentMap = std::unordered_map<Key, ResidentNode, Hash>;
        using ResidentIterator = typename ResidentMap::iterator;

        std::size_t capacity_;
        std::size_t hir_capacity_;
        std::size_t lir_capacity_;
        std::size_t history_capacity_;

        std::size_t lir_size_ = 0;
        std::size_t ghost_size_ = 0;

        // Stack S contains resident LIR, resident HIR and non-resident HIR keys.
        // The front is MRU, the back is the bottom of the stack.
        KeyList stack_;
        std::unordered_map<Key, KeyIterator, Hash> stack_pos_;

        // Queue Q contains only resident HIR keys.
        // The front is MRU, the back is the eviction candidate.
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
                stack_pos_.emplace(key, stack_.begin());
                return;
            }

            stack_.splice(stack_.begin(), stack_, pos->second);
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
                return;
            }

            hir_queue_.push_front(resident->first);
            resident->second.in_queue = true;
            resident->second.queue_pos = hir_queue_.begin();
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

        void erase_stack_entry(KeyIterator pos)
        {
            const Key& key = *pos;

            if (resident_.find(key) == resident_.end())
            {
                --ghost_size_;
            }

            stack_pos_.erase(key);
            stack_.erase(pos);
        }

        void trim_ghost_history()
        {
            while (ghost_size_ > history_capacity_)
            {
                auto ghost = std::find_if(
                    stack_.rbegin(),
                    stack_.rend(),
                    [&](const Key& key)
                    {
                        return resident_.find(key) == resident_.end();
                    }
                );

                if (ghost == stack_.rend())
                {
                    throw std::logic_error(
                        "LIRS invariant is broken: ghost counter is invalid"
                    );
                }

                erase_stack_entry(std::prev(ghost.base()));
            }
        }

        void prune_stack()
        {
            while (!stack_.empty())
            {
                auto resident = resident_.find(stack_.back());

                if (resident != resident_.end() && resident->second.is_lir)
                {
                    break;
                }

                erase_stack_entry(std::prev(stack_.end()));
            }

            trim_ghost_history();
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
            remove_from_queue(resident);

            if (!resident->second.is_lir)
            {
                resident->second.is_lir = true;
                ++lir_size_;
            }

            if (lir_size_ > lir_capacity_ &&
                !demote_bottom_lir(&resident->first))
            {
                throw std::logic_error(
                    "LIRS invariant is broken: no LIR block to demote"
                );
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

        std::optional<CacheTransfer<Key, Value, LIRSMeta>>
        evict_hir_if_needed()
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
                throw std::logic_error(
                    "LIRS invariant is broken: no HIR block to evict"
                );
            }

            Key victim_key = hir_queue_.back();
            auto victim = resident_.find(victim_key);

            if (victim == resident_.end())
            {
                throw std::logic_error(
                    "LIRS invariant is broken: stale HIR queue entry"
                );
            }

            hir_queue_.pop_back();
            victim->second.in_queue = false;

            CacheTransfer<Key, Value, LIRSMeta> evicted {
                victim->first,
                std::move(victim->second.value),
                LIRSMeta{false}
            };

            if (stack_pos_.find(victim_key) != stack_pos_.end())
            {
                ++ghost_size_;
            }

            resident_.erase(victim);
            prune_stack();
            return evicted;
        }

        void touch(ResidentIterator resident)
        {
            if (resident->second.is_lir)
            {
                move_stack_to_front(resident->first);
                prune_stack();
                return;
            }

            if (stack_pos_.find(resident->first) != stack_pos_.end())
            {
                promote_to_lir(resident);
                return;
            }

            move_stack_to_front(resident->first);
            move_queue_to_front(resident);
            prune_stack();
        }

    public:

        using Transfer = CacheTransfer<Key, Value, LIRSMeta>;

        LIRSCacheLevel(OptionsLIRS options, Hash hash_func)
            : capacity_(options.capacity),
              hir_capacity_(options.hir_capacity),
              lir_capacity_(
                  options.hir_capacity <= options.capacity
                      ? options.capacity - options.hir_capacity
                      : 0
              ),
              history_capacity_(options.history_capacity),
              stack_pos_(options.capacity, hash_func),
              resident_(options.capacity, std::move(hash_func))
        {
            if (capacity_ == 0)
            {
                throw std::invalid_argument(
                    "LIRS cache capacity must be positive"
                );
            }

            if (hir_capacity_ == 0 || hir_capacity_ > capacity_)
            {
                throw std::invalid_argument(
                    "LIRS HIR capacity must be in range [1, capacity]"
                );
            }

            if (capacity_ > 1 && hir_capacity_ == capacity_)
            {
                throw std::invalid_argument(
                    "LIRS cache with capacity greater than one must keep "
                    "at least one LIR block"
                );
            }
        }

        AddResult<Transfer, Value> add(Value value, Key key)
        {
            return add(
                Transfer {
                    std::move(key),
                    std::move(value),
                    LIRSMeta{false}
                }
            );
        }

        template <typename AnyMeta>
        AddResult<Transfer, Value> add(
            CacheTransfer<Key, Value, AnyMeta> elem
        )
        {
            auto old = resident_.find(elem.key);

            if (old != resident_.end())
            {
                touch(old);
                return {std::nullopt, &(old->second.value)};
            }

            const bool was_ghost =
                stack_pos_.find(elem.key) != stack_pos_.end();
            const bool lir_hint = extract_lir_hint(elem.meta);

            auto [resident, inserted] = resident_.emplace(
                std::move(elem.key),
                ResidentNode {
                    std::move(elem.value),
                    false,
                    false,
                    KeyIterator{}
                }
            );

            if (!inserted)
            {
                return {std::nullopt, &(resident->second.value)};
            }

            if (was_ghost)
            {
                --ghost_size_;
            }

            move_stack_to_front(resident->first);

            if (lir_size_ < lir_capacity_)
            {
                resident->second.is_lir = true;
                ++lir_size_;
            }
            else if ((was_ghost || lir_hint) && lir_capacity_ != 0)
            {
                promote_to_lir(resident);
            }
            else
            {
                move_queue_to_front(resident);
            }

            std::optional<Transfer> evicted = evict_hir_if_needed();
            Value* inserted_value = &(resident->second.value);

            return {std::move(evicted), inserted_value};
        }

        bool contains(const Key& key) const
        {
            return resident_.find(key) != resident_.end();
        }

        const Value* get(const Key& key)
        {
            auto resident = resident_.find(key);

            if (resident == resident_.end())
            {
                return nullptr;
            }

            touch(resident);
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
            const bool stays_as_ghost =
                stack_pos_.find(key) != stack_pos_.end();

            Transfer extracted {
                resident->first,
                std::move(resident->second.value),
                LIRSMeta{was_lir}
            };

            remove_from_queue(resident);

            if (was_lir)
            {
                --lir_size_;
            }

            resident_.erase(resident);

            if (stays_as_ghost)
            {
                ++ghost_size_;
            }

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

                auto stack_pos = stack_pos_.find(key);
                if (stack_pos != stack_pos_.end())
                {
                    stack_.erase(stack_pos->second);
                    stack_pos_.erase(stack_pos);
                }

                resident_.erase(resident);

                if (was_lir)
                {
                    --lir_size_;
                }

                fill_lir_part();
                return;
            }

            auto stack_pos = stack_pos_.find(key);
            if (stack_pos != stack_pos_.end())
            {
                erase_stack_entry(stack_pos->second);
            }

            prune_stack();
        }

        void dump(std::ostream& out) const
        {
            const auto old_flags = out.flags();
            const auto old_fill = out.fill();

            out << "  [ LIRS Policy State ]\n"
                << "    Capacity       : " << capacity_ << '\n'
                << "    Resident       : " << resident_.size() << '\n'
                << "    LIR             : " << lir_size_
                << " / " << lir_capacity_ << '\n'
                << "    Resident HIR    : " << hir_queue_.size()
                << " / " << hir_capacity_ << '\n'
                << "    Ghost history   : " << ghost_size_
                << " / " << history_capacity_ << "\n\n";

            out << "  [ Stack S | MRU -> LRU ]\n"
                << "  --------------------------------------------------\n"
                << "    " << std::left
                << std::setw(6) << "#"
                << std::setw(12) << "State"
                << "Key\n"
                << "  --------------------------------------------------\n";

            if (stack_.empty())
            {
                out << "    (empty)\n";
            }

            std::size_t index = 1;
            for (const Key& key : stack_)
            {
                auto resident = resident_.find(key);
                const char* state = "GHOST";

                if (resident != resident_.end())
                {
                    state = resident->second.is_lir ? "LIR" : "HIR";
                }

                out << "    "
                    << std::setw(6) << index++
                    << std::setw(12) << state
                    << key << '\n';
            }

            out << "  --------------------------------------------------\n\n"
                << "  [ Queue Q | resident HIR | MRU -> LRU ]\n"
                << "  --------------------------------------------------\n"
                << "    " << std::left
                << std::setw(6) << "#"
                << "Key\n"
                << "  --------------------------------------------------\n";

            if (hir_queue_.empty())
            {
                out << "    (empty)\n";
            }

            index = 1;
            for (const Key& key : hir_queue_)
            {
                out << "    "
                    << std::setw(6) << index++
                    << key << '\n';
            }

            out << "  --------------------------------------------------\n";

            out.flags(old_flags);
            out.fill(old_fill);
        }
};

inline LevelSpec<LIRSCacheLevel, OptionsLIRS> LIRS(
    std::size_t capacity,
    std::size_t hir_capacity,
    std::size_t history_capacity
)
{
    return level<LIRSCacheLevel, OptionsLIRS>(
        OptionsLIRS {
            capacity,
            hir_capacity,
            history_capacity
        }
    );
}

inline LevelSpec<LIRSCacheLevel, OptionsLIRS> LIRS(
    std::size_t capacity,
    std::size_t hir_capacity
)
{
    return LIRS(capacity, hir_capacity, capacity);
}

inline LevelSpec<LIRSCacheLevel, OptionsLIRS> LIRS(std::size_t capacity)
{
    const std::size_t hir_capacity =
        capacity <= 1
            ? capacity
            : std::max<std::size_t>(1, capacity / 4);

    return LIRS(capacity, hir_capacity, capacity);
}

} // namespace MyCache

#endif // LIRS_POLICY_HH

#ifndef FACTORY_HH
#define FACTORY_HH

namespace MyCache::runtime {

template <
    typename Key,
    typename Value,
    typename Spec,
    typename Hash = std::hash<Key>
>
auto make_level(Spec spec, Hash hash_func = std::hash<Key>{})
{
    auto policy = std::move(spec).template build<Key, Value, Hash>(hash_func);

    using Level  = ICacheLevel<Key, Value>;
    using Policy = Spec::template Level<Key, Value, Hash>;

    return std::unique_ptr<Level> (
        std::make_unique<PolicyAdapter<Key, Value, Policy>>(std::move(policy))
    );
}

template <
    typename Key,
    typename Value,
    typename Hash = std::hash<Key>
>
class CacheBuilder
{
    public:

        using Level  = ICacheLevel<Key, Value>;
        using LevelPtr = std::unique_ptr<Level>;
        using Loader = std::function<Value(const Key&)>;

    private:

        std::vector<LevelPtr> levels_;
        Loader loader_;

    public:

        CacheBuilder(Loader loader) 
        :   loader_(std::move(loader))
        {}

        template<typename Spec>
        void add_level(Spec spec)
        {
            levels_.push_back(
                make_level<Key, Value, Spec>(
                    std::move(spec)
                )
            );
        }
        
        void add_level(LevelPtr level)
        {
            levels_.push_back(std::move(level));
        }

        Cache<Key, Value> build()
        {
            return Cache<Key, Value> {
                std::move(loader_),
                std::move(levels_)
            };
        };
};

} // MyCache::runtime

#endif // FACTORY_HH

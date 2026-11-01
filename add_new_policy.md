# Добавление новой cache policy

Эта инструкция описывает актуальный контракт политики для двух вариантов кэша:

- `MyCache::pbd` — compile-time cache (policy-based design);
- `MyCache::runtime` — runtime cache с type erasure через `PolicyAdapter`.

## 1. Структура проекта и namespace

| Сущность | Файл | Namespace |
|---|---|---|
| Общие типы `CacheEntry`, `CacheTransfer`, `AddResult`, `EmptyMeta` | `my_cache/types.hpp` | `MyCache` |
| `LevelSpec` и `level` | `my_cache/level_spec.hpp` | `MyCache` |
| Все конкретные политики и их фабричные функции | `my_cache/policies/*.hpp` | `MyCache::policy` |
| PBD-кэш и `make_cache` | `my_cache/pbd/*` | `MyCache::pbd` |
| Runtime-кэш, `CacheBuilder`, adapter | `my_cache/runtime/*` | `MyCache::runtime` |

Новая политика должна целиком находиться в `MyCache::policy`:

```cpp
namespace MyCache::policy {

// Options, Tag, Meta, concept, policy class and factory function.

} // namespace MyCache::policy
```

Не размещай новую политику непосредственно в `MyCache`, `MyCache::pbd` или
`MyCache::runtime`.

## 2. Общие типы нового контракта

Ключ и значение сначала объединяются в `CacheEntry`:

```cpp
template <typename Key, typename Value>
struct CacheEntry
{
    Key key;
    Value value;
};
```

Между PBD-уровнями передаётся entry вместе с метаданными политики:

```cpp
template <typename Entry, typename Meta>
struct CacheTransfer
{
    Entry entry;
    Meta meta;
};
```

Результат добавления общий для обеих архитектур:

```cpp
template <typename Evicted, typename Value>
struct AddResult
{
    std::optional<Evicted> evicted;
    Value* inserted;
};
```

Поэтому внутри класса политики обязательны aliases:

```cpp
using Entry = CacheEntry<Key, Value>;
using Transfer = CacheTransfer<Entry, CustomMeta>;
```

## 3. Options, Tag и Meta

Минимальные настройки:

```cpp
struct OptionsCustom
{
    std::size_t capacity;
};
```

В конструкторе проверь все инварианты. В частности, если алгоритм не умеет
работать с нулевой ёмкостью, выброси `std::invalid_argument`.

Метаданные нужны для сохранения состояния объекта при вытеснении с одного
PBD-уровня и добавлении на другой. Для политики без переносимого состояния
достаточно пустой меты:

```cpp
struct CustomTag {};

struct CustomMeta
{
    using compatibility = CustomTag;
};
```

Если состояние есть, добавь его в `CustomMeta`:

```cpp
struct CustomMeta
{
    std::size_t score;
    using compatibility = CustomTag;
};
```

Tag определяет не имя класса, а семантическую совместимость метаданных. Две
реализации могут использовать один tag только тогда, когда одинаково трактуют
все переносимые поля.

## 4. Совместимость метаданных

PBD может передать в `add` мету любого предыдущего уровня. Поэтому основная
перегрузка `add` должна быть шаблонной и безопасно обрабатывать несовместимый
тип.

Для политики с полем `score`:

```cpp
template <typename AnyMeta>
concept IsCompatibleCustom = requires(const AnyMeta& meta)
{
    typename AnyMeta::compatibility;

    requires std::same_as<
        typename AnyMeta::compatibility,
        CustomTag
    >;

    { meta.score } -> std::convertible_to<std::size_t>;
};

template <typename AnyMeta>
std::size_t extract_score(const AnyMeta& meta)
{
    if constexpr (IsCompatibleCustom<AnyMeta>)
    {
        return static_cast<std::size_t>(meta.score);
    }

    return 1; // начальное состояние для новой или несовместимой меты
}
```

Правила:

- совместимая мета восстанавливает состояние объекта;
- несовместимая мета и `EmptyMeta` создают начальное состояние;
- для проверки полей используй `requires`, не обращайся к ним до
  `if constexpr`;
- политика без переносимого состояния может просто игнорировать `AnyMeta`, как
  это делает LRU.

## 5. Шаблон и конструктор policy class

Порядок шаблонных параметров фиксирован, потому что именно его ожидает
`LevelSpec`:

```cpp
template <typename Key, typename Value, typename Hash>
class CustomCacheLevel;
```

Конструктор также имеет фиксированную форму:

```cpp
CustomCacheLevel(OptionsCustom options, Hash hash_func);
```

Передавай `hash_func` во все `std::unordered_map<Key, ... , Hash>`. Не создавай
внутри политики новый `Hash{}`, иначе пользовательский stateful hash потеряется.

Пример:

```cpp
CustomCacheLevel(OptionsCustom options, Hash hash_func)
    : capacity_(options.capacity),
      index_(capacity_, std::move(hash_func))
{
    if (capacity_ == 0)
    {
        throw std::invalid_argument(
            "Custom cache capacity must be positive"
        );
    }
}
```

## 6. Обязательный интерфейс политики

Политика должна предоставлять этот базовый контракт:

```cpp
using Entry = CacheEntry<Key, Value>;
using Transfer = CacheTransfer<Entry, CustomMeta>;

AddResult<Transfer, Value> add(Entry entry);

template <typename AnyMeta>
AddResult<Transfer, Value> add(
    CacheTransfer<Entry, AnyMeta> transfer
);

const Value* get(const Key& key);
bool contains(const Key& key) const;
std::optional<Transfer> extract(const Key& key);
void remove(const Key& key);
void dump(std::ostream& out) const;
```

### `add(Entry)`

Эта перегрузка обязательна для runtime-архитектуры: `PolicyAdapter` передаёт в
политику именно `CacheEntry`, без меты.

```cpp
AddResult<Transfer, Value> add(Entry entry)
{
    return add(
        Transfer {
            std::move(entry),
            CustomMeta{1}
        }
    );
}
```

### `add(CacheTransfer<Entry, AnyMeta>)`

Это основная перегрузка для PBD. Она должна:

1. Прочитать совместимую мету или создать начальное состояние.
2. При необходимости выбрать и полностью удалить жертву.
3. Сформировать для жертвы `Transfer` с метой текущей политики.
4. Переместить `transfer.entry.key` и `transfer.entry.value` в хранилище.
5. Вернуть указатель именно на значение, уже лежащее внутри политики.

Правильная форма transfer при вытеснении:

```cpp
Transfer evicted {
    Entry {
        std::move(victim.key),
        std::move(victim.value)
    },
    std::move(victim.meta)
};
```

Результат:

```cpp
return AddResult<Transfer, Value> {
    std::move(evicted),
    &inserted_node.value
};
```

Если жертвы нет, первое поле равно `std::nullopt`.

Указатель `inserted` не должен указывать на локальную переменную, параметр
функции или уже перемещённый объект.

### `get`

Возвращает `nullptr` при miss. При hit обновляет состояние алгоритма и возвращает
указатель на значение в хранилище.

### `contains`

Только проверяет наличие resident-объекта. Не меняет recency, frequency или
другие данные политики. Ghost/history-запись без значения не считается
resident-объектом.

### `extract`

Используется при подъёме объекта с нижнего PBD-уровня. Метод должен:

1. вернуть `std::nullopt`, если resident-объекта нет;
2. сформировать `Transfer` с метой текущей политики;
3. удалить объект из всех resident-структур и индексов;
4. сохранить или удалить ghost/history-запись согласно алгоритму;
5. вернуть transfer.

Если сам алгоритм считает `extract` обращением, обнови мету до формирования
transfer. Например, LFU передаёт увеличенную частоту.

### `remove`

Полностью удаляет ключ из политики. Для history-based алгоритма заранее уточни,
должна ли удаляться и ghost/history-запись. Повторный вызов для отсутствующего
ключа должен быть безопасным.

### `dump`

Показывает данные, достаточные для проверки решения о следующем вытеснении:

- имя политики;
- resident size и capacity;
- порядок или bucket'ы;
- частоты, классы и размеры history, если они влияют на алгоритм.

Не изменяй состояние политики в `dump`. Если меняешь flags/fill потока,
восстанови их перед выходом.

## 7. Фабричная функция и `LevelSpec`

Фабричная функция политики также находится в `MyCache::policy` и возвращает
`LevelSpec`:

```cpp
inline LevelSpec<CustomCacheLevel, OptionsCustom>
Custom(std::size_t capacity)
{
    return level<CustomCacheLevel, OptionsCustom>(
        OptionsCustom{capacity}
    );
}
```

Если параметров больше:

```cpp
inline LevelSpec<CustomCacheLevel, OptionsCustom>
Custom(std::size_t capacity, std::size_t window_capacity)
{
    return level<CustomCacheLevel, OptionsCustom>(
        OptionsCustom{capacity, window_capacity}
    );
}
```

Функцию делай `inline`, потому что она определяется в header-файле.

## 8. Каркас файла новой политики

Файл размещается как `my_cache/policies/custom_policy.hpp`. Для автономности
header рекомендуется явно подключать свои проектные зависимости, а не зависеть
от случайного порядка include у пользователя.

```cpp
#ifndef CUSTOM_POLICY_HPP
#define CUSTOM_POLICY_HPP

#include "../types.hpp"
#include "../level_spec.hpp"

#include <concepts>
#include <cstddef>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace MyCache::policy {

struct CustomTag {};

struct CustomMeta
{
    std::size_t score;
    using compatibility = CustomTag;
};

struct OptionsCustom
{
    std::size_t capacity;
};

template <typename AnyMeta>
concept IsCompatibleCustom = requires(const AnyMeta& meta)
{
    typename AnyMeta::compatibility;
    requires std::same_as<
        typename AnyMeta::compatibility,
        CustomTag
    >;
    { meta.score } -> std::convertible_to<std::size_t>;
};

template <typename Key, typename Value, typename Hash>
class CustomCacheLevel
{
public:
    using Entry = CacheEntry<Key, Value>;
    using Transfer = CacheTransfer<Entry, CustomMeta>;

private:
    std::size_t capacity_;

    // TODO: resident storage, indexes, queues/buckets and history.

    template <typename AnyMeta>
    static std::size_t extract_score(const AnyMeta& meta)
    {
        if constexpr (IsCompatibleCustom<AnyMeta>)
        {
            return static_cast<std::size_t>(meta.score);
        }

        return 1;
    }

public:
    CustomCacheLevel(OptionsCustom options, Hash hash_func)
        : capacity_(options.capacity)
    {
        // TODO: pass hash_func to every Key-based unordered container.
        (void)hash_func;

        if (capacity_ == 0)
        {
            throw std::invalid_argument(
                "Custom cache capacity must be positive"
            );
        }
    }

    AddResult<Transfer, Value> add(Entry entry)
    {
        return add(
            Transfer {
                std::move(entry),
                CustomMeta{1}
            }
        );
    }

    template <typename AnyMeta>
    AddResult<Transfer, Value> add(
        CacheTransfer<Entry, AnyMeta> transfer
    )
    {
        const std::size_t score = extract_score(transfer.meta);
        (void)score;

        // TODO: evict if needed, insert entry and return
        // {evicted transfer or std::nullopt, pointer to stored value}.
    }

    const Value* get(const Key& key)
    {
        // TODO
    }

    bool contains(const Key& key) const
    {
        // TODO
    }

    std::optional<Transfer> extract(const Key& key)
    {
        // TODO
    }

    void remove(const Key& key)
    {
        // TODO
    }

    void dump(std::ostream& out) const
    {
        // TODO
    }
};

inline LevelSpec<CustomCacheLevel, OptionsCustom>
Custom(std::size_t capacity)
{
    return level<CustomCacheLevel, OptionsCustom>(
        OptionsCustom{capacity}
    );
}

} // namespace MyCache::policy

#endif // CUSTOM_POLICY_HPP
```

Каркас показывает контракт, но не является готовой политикой: все `TODO` должны
быть реализованы согласно алгоритму.

## 9. Подключение к PBD

PBD хранит типы уровней в `std::tuple`, а между уровнями передаёт полный
`CacheTransfer<Entry, Meta>`. Здесь совместимая мета действительно сохраняется.

```cpp
#include "my_cache/pbd.hpp"
#include "my_cache/policies/custom_policy.hpp"

#include <functional>
#include <string>

auto cache = MyCache::pbd::make_cache<std::string, int>(
    [](const std::string& key)
    {
        return static_cast<int>(key.size());
    },
    std::hash<std::string>{},
    MyCache::policy::Custom(10),
    MyCache::policy::Custom(20)
);
```

PBD требует хотя бы один и сейчас поддерживает не более пяти уровней.

## 0. Подключение к runtime

Runtime хранит уровни через `ICacheLevel` и `PolicyAdapter`:

```cpp
#include "my_cache/runtime.hpp"
#include "my_cache/policies/custom_policy.hpp"

#include <string>

MyCache::runtime::CacheBuilder<std::string, int> builder(
    [](const std::string& key)
    {
        return static_cast<int>(key.size());
    }
);

builder.add_level(MyCache::policy::Custom(10));
builder.add_level(MyCache::policy::Custom(20));

auto cache = builder.build();
```

Текущий runtime adapter стирает тип меты:

- при вытеснении берёт только `result.evicted->entry`;
- при добавлении вызывает `policy.add(Entry)`;
- при `extract` наружу также отдаёт только `Entry`.

Следовательно, runtime сохраняет ключ и значение, но не policy metadata между
уровнями. Новая политика обязана корректно работать в этом режиме и создавать
начальную мету в `add(Entry)`.

С текущей реализацией `runtime::make_level` policy class также должна быть
copy-constructible при создании адаптера. Сам кэш позднее не копирует уже
заполненную политику.

## 11. Минимальные проверки новой политики

### Проверка контракта

- policy находится в `MyCache::policy`;
- шаблон класса имеет параметры `<Key, Value, Hash>` именно в этом порядке;
- конструктор принимает `(OptionsCustom, Hash)`;
- объявлены `Entry = CacheEntry<Key, Value>` и
  `Transfer = CacheTransfer<Entry, CustomMeta>`;
- есть обе перегрузки `add`: для `Entry` и для transfer с `AnyMeta`;
- обе перегрузки возвращают `AddResult<Transfer, Value>`;
- реализованы `get`, `contains`, `extract`, `remove`, `dump`;
- `contains` и `dump` имеют `const` qualifier;
- фабричная функция находится в `MyCache::policy`, объявлена `inline` и
  возвращает правильный `LevelSpec`.

### Проверка поведения

- capacity `1` работает;
- недопустимые options отклоняются;
- добавление без вытеснения возвращает `evicted == std::nullopt`;
- при переполнении вытесняется ожидаемая жертва;
- `inserted` указывает на реально сохранённое значение;
- `get` изменяет состояние только при hit;
- `contains` не изменяет состояние;
- `extract` возвращает корректные key, value и meta и удаляет resident-объект;
- `remove` безопасен для отсутствующего ключа;
- после каждого удаления очищаются все индексы и не остаются stale iterators;
- custom/stateful `Hash` действительно используется;
- несовместимая мета сбрасывается в начальное состояние;
- совместимая мета сохраняется в PBD;
- одна и та же policy собирается и работает через PBD и runtime.

### Обязательные smoke tests

Собери два отдельных минимальных примера с `-std=c++20`:

1. `MyCache::pbd::make_cache` минимум с двумя уровнями новой политики;
2. `MyCache::runtime::CacheBuilder` минимум с двумя уровнями новой политики.

На момент обновления инструкции в `my_cache/runtime/cache.hpp` ветка cache miss
вызывает `load_(key)`, хотя поле называется `loader_`. Пока эта инфраструктурная
опечатка не исправлена, любой runtime-тест, вызывающий `Cache::get`, не соберётся
вне зависимости от корректности новой политики. Сборку `add`, eviction, `remove`
и `dump` всё равно нужно проверить, а `get` — отдельно на объекте самой policy.

После сборки проверь последовательность: add до переполнения, вытеснение вниз,
hit на первом уровне, подъём с нижнего уровня, miss с вызовом loader, `remove` и
`dump`.

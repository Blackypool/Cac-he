# Инструкция по интеграции пользовательских уровней кэша (Cache Level API)

Данный документ описывает контракт (интерфейс), который необходимо реализовать для создания собственного уровня кэширования и его успешной стыковки с многоуровневым кэшем `MyCache::Cache`.

Многоуровневый кэш поддерживает передачу вытесненных элементов между уровнями, поэтому ваш класс должен уметь принимать элементы с чужими метаданными и корректно отдавать свои.

## 1. Пространство имен и Метаданные

Все компоненты вашего кэша должны находиться в пространстве имен `MyCache`.
Для идентификации вашего алгоритма вытеснения вам необходимо создать два `struct`:

1. **Tag (Тег)** — пустая структура-маркер.

2. **Meta (Метаданные)** — структура, хранящая служебную информацию (если она нужна). Она обязана содержать псевдоним `compatibility`, указывающий на ваш тег.

```cpp
namespace MyCache {

    struct MyLevelTag {};

    struct MyLevelMeta {
        // Ваши данные, например: size_t my_data; 
        using compatibility = MyLevelTag; 
    };

}
```

## 2. Шаблон класса (Template Signature)

Ваш класс уровня кэша обязан принимать следующие четыре шаблонных параметра в строгом порядке:

```cpp
template <
    typename Value, 
    typename Key, 
    typename Extractor, 
    typename Hash
> 
class MyCustomCacheLevel {
    // ...
};
```

## 3. Внутренние типы и структура AddResult

Внутри `public` секции вашего класса необходимо определить тип данных, который вы будете возвращать при вытеснении элементов, а также структуру `AddResult`.

```cpp
public:
    using Transfer = CacheTransfer<Value, MyLevelMeta>;

    struct AddResult {
        std::optional<Transfer> evicted;
        Value* inserted;
    };
```

## 4. Обязательный публичный интерфейс (Методы)

Ваш класс должен реализовывать следующий набор методов.

### Добавление элементов (`add`)

Требуется **два** перегруженных метода `add`.

**1. Добавление "сырого" значения** (выступает как обертка над вторым методом):

```cpp
AddResult add(Value value);
```

**2. Шаблонное добавление с переносом метаданных.** Вызывается, когда элемент приходит с другого уровня кэша:

```cpp
template <typename AnyMeta>
AddResult add(CacheTransfer<Value, AnyMeta> elem);
```

### Безопасное извлечение метаданных (Паттерн для `add`)

Так как метод `add` принимает `AnyMeta` (метаданные с любого другого уровня), вашему кэшу может потребоваться извлечь из них полезную информацию.

В пространстве имен `MyCache` **уже реализованы** готовые C++20 концепты для стандартных уровней (например, `IsCompatibleLFU`). Вам не нужно писать их с нуля — просто используйте их в своей функции-экстракторе.

Пример концепта, который **уже есть** в библиотеке:

```cpp
template<typename AnyMeta>
concept IsCompatibleLFU = requires(AnyMeta meta)
{
    typename AnyMeta::compatibility;
    requires std::same_as<typename AnyMeta::compatibility, LFUTag>;
    {meta.freq} -> std::convertible_to<size_t>;
};
```

Пример того, как использовать этот концепт внутри вашего класса:

```cpp
// Функция-помощник внутри класса кэша
template <typename AnyMeta>
size_t extract_meta_info(const AnyMeta& meta) {
    if constexpr (IsCompatibleLFU<AnyMeta>) {
        return meta.freq; // Если пришла LFU-мета, забираем частоту
    } else {
        return 1; // Безопасное значение по умолчанию для других алгоритмов
    }
}
```

### Доступ к элементам (`get` и `find`)

**1. Поиск с обновлением состояния (Hit):**

```cpp
Value* get(const Key& key);
```

**2. Поиск без изменения состояния:**

```cpp
bool find(const Key& key);
```

### Извлечение и Удаление (`extract` и `remove`)

**1. Удаление с возвратом:**

```cpp
std::optional<Transfer> extract(const Key& key);
```
Используется при "поднятии" элемента на уровень выше. Если элемент найден, он удаляется из текущего кэша и возвращается вместе со своими метаданными.

**2. Безусловное удаление:**

```cpp
void remove(const Key& key);
```

## 5. Boilerplate (Шаблон для копирования)

Минимальный каркас для создания нового уровня кэша (без лишнего кода):

```cpp
#include <optional>
#include <concepts>

namespace MyCache {

struct MyLevelTag {};

struct MyLevelMeta {
    using compatibility = MyLevelTag;
};

template <
    typename Value, 
    typename Key, 
    typename Extractor, 
    typename Hash
> 
class MyCustomCacheLevel {
public:
    using Transfer = CacheTransfer<Value, MyLevelMeta>;

    struct AddResult {
        std::optional<Transfer> evicted;
        Value* inserted;
    };

    MyCustomCacheLevel(size_t capacity, Extractor key_extr, Hash hash_func) {}

    AddResult add(Value value) {
        // Обертка
        return add(CacheTransfer<Value, EmptyMeta>{std::move(value), {}});
    }

    template <typename AnyMeta>
    AddResult add(CacheTransfer<Value, AnyMeta> elem) {
        return {std::nullopt, nullptr};
    }

    Value* get(const Key& key) {
        return nullptr;
    }

    bool find(const Key& key) {
        return false;
    }

    std::optional<Transfer> extract(const Key& key) {
        return std::nullopt;
    }

    void remove(const Key& key) {}
    
private:
    template <typename AnyMeta>
    size_t extract_meta_info(const AnyMeta& meta) {
        // Пример использования существующего концепта:
        // if constexpr (IsCompatibleLFU<AnyMeta>) { return meta.freq; }
        return 0; 
    }
};

} // namespace MyCache
```
# Аудит проекта и план развития

Дата аудита: 3 ноября 2026 года.

## 1. Краткий вывод

Архитектурная основа проекта удачная и перспективная:

- одна реализация policy используется в PBD и runtime;
- `LevelSpec` отделяет описание уровня от его конкретных `Key`, `Value` и `Hash`;
- PBD хранит конкретные типы и допускает полную инлайнизацию;
- runtime предоставляет динамическое число и порядок уровней через type erasure;
- `CacheTransfer` позволяет переносить policy metadata между PBD-уровнями;
- JSON-builder изолирован от основной библиотеки.

Однако проект пока не готов к большим сравнительным исследованиям. Сначала нужно
закрыть ошибки сборки и API, определить единую семантику PBD/runtime, добавить
автоматические тесты, а уже затем измерять производительность. Иначе benchmark
может сравнивать не стоимость virtual-вызова, а разные алгоритмические результаты.

Текущий общий статус:

| Область | Статус |
|---|---|
| LRU | базово работает |
| LFU | базово работает, есть критичные edge cases и ODR-проблема |
| ARC | runtime/PBD smoke-тест пройден, нужна проверка алгоритма по oracle |
| 2Q | runtime/PBD smoke-тест пройден, нужна проверка ghost/invariant semantics |
| LIRS | сейчас не подключается и не соответствует актуальному контракту |
| Runtime | идея хорошая, есть ошибки custom hash и пустой иерархии |
| PBD | идея хорошая, `remove()` фактически не удаляет элемент |
| JSON-builder | LRU/LFU работают, ARC/2Q ещё не подключены |
| Тесты | отсутствуют как постоянная автоматическая система |
| CMake | отсутствует |
| Benchmark | есть полезные старые исследования, но текущий файл не собирается с новым API |

## 2. Текущая архитектура

```text
types.hpp + level_spec.hpp
          |
          +-----------------------------+
          |                             |
      policies/*                    CacheTransfer
          |                             |
          +--------------+--------------+
                         |
              +----------+----------+
              |                     |
             PBD                 runtime
       tuple<Policies...>   vector<unique_ptr<ICacheLevel>>
              |                     |
       прямые вызовы          virtual + PolicyAdapter
                                    |
                              json_builder
```

Это хорошее разбиение. JSON, тестовые workloads и исследовательские инструменты
не должны попадать в `pbd.hpp`, `runtime.hpp` или основные policy headers.

## 3. Что было проверено во время аудита

Проверки выполнялись GCC 15.2, C++20, с `-Wall -Wextra -Wpedantic`.

### 3.1 ARC и 2Q

Проверены одноуровневые LRU, LFU, ARC и 2Q в runtime и PBD:

- построение policy;
- повторные `get()`;
- корректность возвращаемого значения;
- `contains()`;
- `remove()` в runtime;
- смешанная конфигурация `ARC / 2Q / LFU`.

Обе архитектуры собраны и завершили smoke-тест без падения. Это подтверждает,
что ARC и 2Q уже можно включать в последующие тесты. Это пока не является
доказательством правильности самих алгоритмов.

### 3.2 Поведение 2Q на последовательном цикле

Трасса: восемь проходов по ключам `0..23`, уровни `4 / 8 / 16`.

| Конфигурация | Loader calls из 192 запросов |
|---|---:|
| LRU / LRU / LRU | 24 |
| ARC / LRU / LRU | 24 |
| LRU / 2Q / LRU | 192 |
| ARC / 2Q / LFU | 192 |
| LRU / LRU / LFU | 24 |

Это может быть следствием scan-resistant семантики 2Q: A1 имеет только четверть
номинальной вместимости, а Am начинает заполняться после повторного обращения к
ghost key. Нельзя автоматически объявлять это ошибкой, но это обязательно нужно
зафиксировать policy-specific тестами. Сумма номинальных capacity не всегда
означает, что вся эта память сразу используется resident entries.

### 3.3 Различие PBD и runtime metadata

На одинаковой LFU/LFU/LFU-трассе получено:

| Архитектура | Loader calls |
|---|---:|
| runtime | 231 |
| PBD | 222 |

Причина заложена в архитектуре:

- PBD передаёт `LFUMeta` через `CacheTransfer`;
- runtime `PolicyAdapter` преобразует вытеснённый transfer в простой `Entry` и
  выбрасывает metadata;
- LFU в runtime начинает частоту заново при переходе на следующий уровень.

Следовательно, одинаковые названия policies ещё не означают одинаковое поведение.
Перед benchmark `PBD vs runtime` необходимо принять одно из решений:

1. Признать различие частью публичной семантики и сравнивать только время при
   одинаковом числе miss.
2. В PBD также сбрасывать metadata для полного semantic parity.
3. Добавить type-erased metadata в runtime, понимая дополнительную стоимость.
4. Разделить исследования: `архитектурный overhead` и `эффект переноса metadata`.

Рекомендуется вариант 4. Для чистого overhead использовать LRU или гарантированные
L1-hit трассы. Перенос metadata исследовать отдельно как алгоритмическую функцию.

## 4. Критичные проблемы, которые нужно исправить первыми

### P0.1 Одинаковый include guard у двух factory headers

Статус: исправлено 3 ноября 2026 года — публичные и внутренние headers переведены
на `#pragma once`.

`my_cache/runtime/factory.hpp` и `my_cache/pbd/factory.hpp` используют:

```cpp
#ifndef FACTORY_HH
```

После подключения `runtime.hpp` содержимое PBD factory пропускается. Совместный
тест не видит `MyCache::pbd::make_cache`.

Решение: перейти на `#pragma once` во всех headers либо использовать уникальные
guards. Для поддерживаемых GCC, Clang и MSVC практичнее `#pragma once`.

### P0.2 Policy headers не самодостаточны

Статус: исправлено 3 ноября 2026 года — каждый policy header напрямую подключает
`types.hpp` и `level_spec.hpp`; отдельная компиляция всех headers проходит.

Например, отдельный `#include "my_cache/policies/lru_policy.hpp"` не компилируется,
потому что header не подключает `types.hpp` и `level_spec.hpp`.

Каждый policy header должен напрямую подключать свои зависимости:

```cpp
#include "../types.hpp"
#include "../level_spec.hpp"
```

Нужны отдельные compile-tests на каждый публичный header.

### P0.3 Runtime `Hash` сейчас не работает

Статус: исправлено 3 ноября 2026 года — builder принимает и хранит `Hash`, после
чего передаёт его каждому уровню. Проверено недефолтным stateful hash.

`CacheBuilder<Key, Value, Hash>` объявляет `Hash`, но `add_level()` вызывает
`make_level<Key, Value, Spec>()`, после чего используется `std::hash<Key>`.

Проверка с пользовательским ключом и собственным hash не компилируется.

Нужно хранить hash в builder и передавать его в каждый уровень:

```cpp
make_level<Key, Value>(std::move(spec), hash_);
```

Также нужно решить, копируется ли stateful hash на каждый уровень или создаётся
отдельным factory.

### P0.4 PBD `remove()` удаляет из копии

Статус: исправлено 3 ноября 2026 года — обход уровней использует ссылку и
публичный `remove(key)` больше не требует template-индекса.

Сейчас:

```cpp
auto level = std::get<I>(levels_);
```

После `remove<0>(key)` исходный cache всё ещё содержит key. Проверка это
подтвердила.

Минимальное исправление:

```cpp
auto& level = std::get<I>(levels_);
```

Лучше заменить публичный `remove<I>()` на обычный `remove(key)`, который проходит
все уровни.

### P0.5 Не определена семантика полного удаления

ARC и 2Q имеют ghost entries. Текущие cache-level `remove()` сначала вызывают
`contains()`, а `contains()` намеренно не считает ghosts resident entries.
Следовательно, ghost history может остаться после пользовательского `remove()`.

Нужно определить контракт:

- `remove(key)` удаляет только resident value; или
- `erase(key)` полностью очищает resident value и policy history на всех уровнях.

Для пользовательского API рекомендуется второй вариант. Cache должен без
предварительного `contains()` вызвать purge на каждом уровне.

### P0.6 LFU допускает capacity = 0

`LFU(0)` успешно строится. Первый `add()` затем может попытаться вытеснить элемент
из пустого списка. Конструктор LFU должен выбрасывать configuration exception при
нулевой ёмкости.

### P0.7 LFU нарушает ODR

Статус: исправлено — factory-функция `LFU()` помечена `inline`.

Функция в header:

```cpp
LevelSpec<LFUCacheLevel, OptionsLFU> LFU(size_t capacity)
```

не помечена `inline`. Линковка двух translation units дала multiple definition.
Нужно добавить `inline`.

### P0.8 Пустой runtime cache

`CacheBuilder::build()` допускает ноль уровней, а `Cache::get()` обращается к
`levels_[0]`. Builder обязан отклонять пустую иерархию. Дополнительно сам
конструктор `Cache` должен защищать инвариант, чтобы его нельзя было обойти.

### P0.9 LIRS не соответствует текущему проекту

Статус: исправлена совместимость 3 ноября 2026 года — LIRS переведён на текущие
`CacheEntry`, `CacheTransfer`, `LevelSpec` и namespace `MyCache::policy`. Runtime
и PBD smoke-тесты проходят; алгоритмические oracle-тесты ещё нужны.

Сейчас LIRS:

- подключает несуществующие/устаревшие `cache.hpp` и `types.h`;
- находится не в `MyCache::policy`;
- использует старую перегрузку `add(Value, Key)`;
- не собирается даже после `runtime.hpp`.

LIRS нужно либо явно исключить из поддерживаемого набора, либо мигрировать по
`add_new_policy.md` после стабилизации общего контракта.

### P0.10 Нет формального policy concept

Сейчас ошибки несовместимой policy возникают глубоко в template instantiation.
Нужны concepts для:

- `PolicyOptions`;
- `LevelSpec`;
- PBD policy contract;
- runtime-adaptable policy contract.

Это улучшит сообщения компилятора и позволит написать compile-time contract tests.

## 5. Мелкие и средние недочёты API

### P1

- `ICacheLevel::get()` и `Cache::get()` переведены на `const Key&`. На cache hit
  ключ больше не копируется; на miss выполняется необходимая копия для хранения
  ключа внутри кэша.
- Runtime `remove()` делает `contains()` и затем `remove()`, то есть два hash lookup.
- `Cache::add()` сначала обходит все уровни через `contains()`, затем снова выполняет
  lookup внутри policy.
- `dump()` в interface должен быть `const`.
- Runtime loader хранится в `std::function`, а PBD сохраняет конкретный тип. Это
  добавляет runtime overhead, не связанный с virtual policy calls.
- PBD constructor копирует loader вместо move.
- `build()` должен быть rvalue-qualified: `build() &&`, поскольку builder после
  перемещения больше не должен использоваться.
- Нужны `reserve_levels(count)` или builder constructor с ожидаемым числом уровней.
- Ограничение PBD `MAX_LEVELS_NUM 5` является произвольным macro. Лучше удалить его
  или заменить документированным `inline constexpr`.
- `size_t` следует писать как `std::size_t`.
- Возвращаемые `const Value*` требуют документации времени жизни: любой add,
  promotion, eviction или remove может инвалидировать указатель.
- Нужны `size()`, `level_count()`, `clear()` и, возможно, результат `add()`/`remove()`.
- Нет задокументированной thread-safety. По умолчанию cache следует объявить
  не thread-safe.
- Нет формального требования к exception safety `Key`, `Value`, `Hash` и loader.

### P2

- Возможность передать allocator/`std::pmr` для исследовательских workloads.
- Eviction callback или observer для внешнего хранения/логирования.
- Опциональная statistics/observer policy без стоимости в production build.
- Отдельный API для read-only lookup без изменения policy state.

## 6. Исключения

Сейчас используются разрозненные `std::invalid_argument`, `std::logic_error`,
`std::out_of_range` и `std::runtime_error`. Ошибки существуют, но у библиотеки нет
собственного стабильного контракта исключений.

Рекомендуемый `my_cache/errors.hpp`:

```cpp
namespace MyCache {

class cache_error : public std::runtime_error {};
class configuration_error : public cache_error {};
class invalid_capacity : public configuration_error {};
class empty_hierarchy : public configuration_error {};
class cache_invariant_error : public cache_error {};

} // namespace MyCache
```

Для JSON-tool отдельно:

```cpp
namespace JsonConfig {

class config_error : public std::runtime_error {};
class parse_error : public config_error {};
class unsupported_policy : public config_error {};

} // namespace JsonConfig
```

Правила:

- неверные пользовательские options -> `configuration_error`;
- capacity, нарушающая policy constraint -> `invalid_capacity`;
- внутренняя рассинхронизация list/map -> `cache_invariant_error`;
- неизвестная JSON policy -> `unsupported_policy`;
- исключения loader не оборачивать, чтобы пользователь не терял исходный тип;
- каждое сообщение содержит policy, level index и нарушенный параметр;
- задокументировать basic/strong exception guarantee для публичных операций.

Не следует создавать десятки исключений по одному на каждый `if`. Достаточно
небольшой иерархии, которую пользователь может стабильно перехватывать.

## 7. Архитектура тестов

Рекомендуется GoogleTest с pinned FetchContent dependency. Typed tests подходят
для общего policy contract.

```text
tests/
├── CMakeLists.txt
├── header/
│   ├── runtime_header.cpp
│   ├── pbd_header.cpp
│   ├── lru_header.cpp
│   ├── lfu_header.cpp
│   ├── arc_header.cpp
│   └── two_q_header.cpp
├── contract/
│   ├── policy_compile_tests.cpp
│   └── policy_behavior_typed_tests.cpp
├── policies/
│   ├── lru_tests.cpp
│   ├── lfu_tests.cpp
│   ├── arc_tests.cpp
│   └── two_q_tests.cpp
├── cache/
│   ├── runtime_tests.cpp
│   ├── pbd_tests.cpp
│   ├── hierarchy_tests.cpp
│   └── metadata_transfer_tests.cpp
├── json/
│   └── json_builder_tests.cpp
└── randomized/
    └── state_machine_tests.cpp
```

### 7.1 Общий policy contract

Каждая policy проверяется на:

- invalid capacity;
- cold miss;
- hit;
- duplicate/add contract;
- eviction key/value;
- `contains()`;
- `extract()`;
- `remove()`;
- pointer validity до следующей мутации;
- custom key/hash;
- copyable и move-only Value, если они заявлены поддерживаемыми;
- корректную обработку совместимой и несовместимой metadata.

### 7.2 Policy-specific tests

LRU:

- точный порядок MRU/LRU;
- hit перемещает узел в начало;
- вытесняется точный LRU key.

LFU:

- частота увеличивается;
- корректный tie-break;
- обновление `min_freq_`;
- перенос и сброс metadata;
- большое значение frequency;
- capacity 0.

ARC:

- размеры и membership T1/T2/B1/B2 после каждого шага;
- resident count не превышает capacity;
- ghost count ограничен;
- B1/B2 hit корректно меняет target;
- сравнение с небольшой эталонной реализацией ARC;
- отдельная проверка, почему capacity 8 и рабочий набор 6 дают 8 загрузок.

2Q:

- A1 FIFO;
- A1out содержит только keys;
- повторный key из A1out попадает в Am;
- Am использует LRU;
- scan pollution;
- отсутствие list/map mismatch;
- resident и ghost capacity;
- последовательный цикл больше `L1 + A1 + L3`.

### 7.3 Hierarchy tests

- элемент resident только на одном уровне, если это заявленная exclusive semantics;
- promotion из каждого уровня;
- cascading eviction;
- loader вызывается только при полном miss;
- remove очищает все уровни и ghosts;
- loader exception не повреждает hierarchy;
- policy exception не теряет значение;
- PBD/runtime parity для LRU;
- ожидаемое различие metadata для LFU/ARC/2Q.

### 7.4 Randomized/property tests

Нужен state-machine test с фиксированным seed:

```text
add / get / contains / remove
```

После каждой операции проверяются:

- отсутствие duplicate resident keys;
- размер каждого уровня;
- соответствие list и hash index;
- возвращаемое значение;
- число loader calls;
- checksum.

Для LRU/LFU можно написать маленькие медленные reference models. Для ARC/2Q
желательно использовать независимо написанную модель, а не копировать production
алгоритм.

### 7.5 Tooling tests

- ASan/UBSan на Linux/Clang или GCC;
- MSVC AddressSanitizer на Windows;
- Debug iterators, где доступны;
- warning-as-error в CI;
- отдельная сборка каждого public header;
- multi-TU link test для поиска ODR-проблем вроде текущей LFU.

## 8. CMake

Проект header-only, поэтому «сборка кэшей» означает INTERFACE target, который
передаёт include directories и требование C++20.

Предлагаемая структура targets:

```text
MyCache::core          header-only PBD/runtime/policies
MyCache::json_builder optional, зависит от nlohmann_json
my_cache_example      пример/ручной smoke executable
my_cache_tests        GoogleTest
my_cache_benchmarks   Google Benchmark или собственный research runner
```

Root options:

```cmake
option(MY_CACHE_BUILD_TESTS      "Build tests" OFF)
option(MY_CACHE_BUILD_BENCHMARKS "Build benchmarks" OFF)
option(MY_CACHE_BUILD_EXAMPLES   "Build examples" OFF)
option(MY_CACHE_BUILD_JSON       "Enable JSON builder" OFF)
option(MY_CACHE_ENABLE_SANITIZERS "Enable sanitizers" OFF)
```

Минимальный core target:

```cmake
add_library(my_cache INTERFACE)
add_library(MyCache::core ALIAS my_cache)

target_compile_features(my_cache INTERFACE cxx_std_20)
target_include_directories(my_cache INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)
```

Dependencies должны быть pinned:

- nlohmann/json для JSON target;
- GoogleTest для tests;
- Google Benchmark только для microbenchmarks.

Research runner может остаться собственным executable, поскольку Google Benchmark
не заменяет генерацию сложных traces и отчётов.

Нужны presets:

- `debug`;
- `release`;
- `asan-ubsan`;
- `tests`;
- `bench-release`;
- MSVC и GCC/Clang CI builds.

## 9. Оптимизация

Оптимизировать нужно только после correctness baseline и постоянных benchmarks.

### 9.1 Быстрые и безопасные улучшения

1. Передавать `Key` по `const&` там, где ownership не требуется.
2. Убрать двойные `contains() + remove()` lookup.
3. Использовать `reserve()` для builder levels и hash tables.
4. Перемещать loader в PBD constructor.
5. Пометить маленькие header factory functions `inline`.
6. Исправить custom hash path.
7. Сделать `dump()` const.
8. Включить LTO для release benchmark.

### 9.2 Runtime loader без `std::function`

Сейчас сравнение PBD/runtime включает одновременно:

- virtual policy calls;
- `PolicyAdapter`;
- `std::function` loader;
- различие metadata.

Runtime policies могут оставаться type-erased, а loader можно хранить конкретным
типом:

```cpp
template<typename Key, typename Value, typename Loader>
class runtime::Cache;
```

Builder всё равно возвращается через `auto`. Это позволит отдельно измерять
virtual overhead без `std::function`.

### 9.3 LFU

`update_min_freq()` линейно проходит все frequency buckets. На больших переносимых
частотах это может стать основной стоимостью. Варианты исследования:

- поддерживать следующий непустой bucket;
- ordered map частот;
- linked frequency buckets;
- периодическое aging/normalization frequency;
- ограничение/saturation frequency.

Каждый вариант нужно сравнить по времени и качеству eviction.

### 9.4 Аллокации и layout

Позднее исследовать:

- `std::pmr`;
- pool allocation list nodes;
- flat/open-addressing hash map;
- уменьшение числа отдельных containers;
- small-object optimization для type-erased level;
- хранение уровней в contiguous type-erased buffer вместо отдельных `unique_ptr`.

Эти изменения имеют смысл только после измерения allocations, cache misses CPU и
branch misses.

## 10. Benchmark и исследования

Существующий `benchmark/benchmark.cpp` содержит полезные workloads:

- постоянные уровни популярности;
- плавный frequency drift;
- резкую смену hot/cold roles;
- Zipf;
- случайно меняющуюся популярность;
- scan pollution;
- deterministic seeds, checksum и median repetitions.

Его стоит сохранить как источник workloads, но сейчас он не собирается: использует
старые `../cache.hpp`, `MyCache::make_cache`, старые namespaces и отдельную старую
LFU policy. Его нужно мигрировать после стабилизации API, а не исправлять точечно.

### 10.1 Разделить виды benchmark

```text
benchmarks/
├── micro/
│   ├── policy_hit.cpp
│   ├── add_evict.cpp
│   ├── extract_promote.cpp
│   ├── adapter_overhead.cpp
│   └── build_cost.cpp
├── architecture/
│   ├── pbd_vs_runtime.cpp
│   ├── metadata_vs_reset.cpp
│   └── virtual_vs_concrete_adapter.cpp
├── workloads/
│   ├── uniform.hpp
│   ├── zipf.hpp
│   ├── hotset.hpp
│   ├── phase_change.hpp
│   ├── scan.hpp
│   ├── loop.hpp
│   └── markov.hpp
├── research/
│   ├── policy_matrix.cpp
│   └── hierarchy_matrix.cpp
└── tools/
    ├── run_matrix.py
    ├── summarize.py
    └── plot.py
```

### 10.2 Две независимые группы метрик

Нельзя объединять в один вывод:

1. Качество policy:
   - miss count;
   - hit rate;
   - hit level distribution;
   - evictions;
   - ghost hits;
   - loader calls.

2. Стоимость реализации:
   - ns/op без loader;
   - cycles/op;
   - instructions;
   - branch misses;
   - CPU cache misses;
   - allocations;
   - binary size и compile time.

Более медленная policy может давать меньше дорогих miss и выигрывать end-to-end.

### 10.3 Обязательные оси исследования

- `working_set / total_capacity`: 0.25, 0.5, 1, 2, 4, 16;
- capacity каждого уровня и их отношение;
- 1, 2, 3, 5 уровней;
- L1 hit, L2 hit, L3 hit и полный miss отдельно;
- доля hot set;
- скорость изменения popularity;
- длина последовательного scan;
- Zipf exponent;
- размер `Key` и `Value`;
- дешёвый и дорогой Hash;
- loader cost от 0 ns до условных micro/milliseconds;
- cold и warmed cache;
- metadata preserve/reset;
- O0, O2, O3, LTO; PGO отдельным экспериментом.

### 10.4 Честное сравнение PBD/runtime

Для pure overhead:

- один и тот же заранее созданный trace;
- одинаковые cache misses и checksum;
- loader либо не вызывается, либо его время исключается;
- одинаковый concrete loader type, если это возможно;
- отдельные direct policy, concrete adapter и virtual interface baselines;
- random/rotating order запуска;
- warmup;
- не менее 10 измерений, median и dispersion;
- CPU power mode, affinity и частота фиксируются в отчёте.

Если loader calls различаются, такой результат нельзя использовать как доказательство
стоимости virtual dispatch.

### 10.5 API для исследований

Текущего API достаточно для первых latency/miss benchmarks через loader counter.
Для серьёзного исследования не хватает наблюдаемости:

- какой уровень дал hit;
- какой уровень вытеснил entry;
- ghost hit;
- promotion/demotion count;
- resident count и history count;
- allocations.

Не следует добавлять unconditional counters в hot path production cache. Лучше:

- compile-time `NullObserver`, который полностью инлайнится;
- отдельный instrumented adapter для runtime;
- benchmark-only policy wrappers;
- debug snapshot API для policy invariants.

## 11. JSON-builder

JSON-builder правильно вынесен из основной библиотеки. Следующие шаги:

1. Подключить ARC и 2Q через простой `capacity` режим.
2. Затем добавить policy-specific options:
   - ARC: total/T1/B1/B2;
   - 2Q: A1/Am/A1out.
3. Использовать собственные config exceptions.
4. Добавить JSON tests с временными файлами.
5. Подключить pinned nlohmann/json только к optional JSON CMake target.
6. Не включать JSON-builder из `runtime.hpp` или `pbd.hpp`.

## 12. Рекомендуемый порядок работ

### Этап 0. Зафиксировать семантику

- exclusive hierarchy;
- metadata preserve/reset;
- resident и ghost capacity;
- remove/purge;
- duplicate add;
- pointer lifetime;
- loader exception behavior.

Результат: короткий `docs/semantics.md`.

### Этап 1. Исправить P0

- guards/`#pragma once`;
- self-contained headers;
- custom runtime hash;
- PBD remove;
- LFU zero capacity и inline;
- empty hierarchy;
- LIRS временно исключить или мигрировать;
- concepts.

Критерий: все public headers компилируются отдельно и вместе, multi-TU link test
проходит.

### Этап 2. Исключения и API contract

- `errors.hpp`;
- exception guarantees;
- const/reference cleanup;
- remove/purge semantics;
- builder consumed-state;
- документация.

### Этап 3. CMake и tests

- INTERFACE library;
- GoogleTest;
- header tests;
- typed policy contract tests;
- ARC/2Q oracle tests;
- sanitizers;
- CI на Windows и Linux.

Критерий: один `ctest` проверяет библиотеку целиком.

### Этап 4. JSON и примеры

- ARC/2Q;
- invalid configs;
- optional dependency target;
- пример executable.

### Этап 5. Корректностный research baseline

- мигрировать существующие workloads;
- отделить miss metrics от timing;
- сохранить traces и seeds;
- CSV/JSON output;
- автоматические графики.

### Этап 6. Оптимизация

- профилирование;
- const-ref key;
- lookup reduction;
- loader type erasure;
- LFU buckets;
- allocations/layout;
- LTO/PGO.

Каждая оптимизация принимается только при неизменных correctness tests и с
измеримым результатом.

### Этап 7. Полное исследование

- policy matrix;
- hierarchy capacity matrix;
- workload matrix;
- PBD/runtime overhead;
- metadata effect;
- break-even loader cost;
- воспроизводимый итоговый отчёт.

## 13. Финальная оценка готовности API

API уже удобно выражает конфигурации:

```cpp
builder.add_level(policy::ARC(64));
builder.add_level(policy::TwoQ(256));
```

и:

```cpp
pbd::make_cache<Key, Value>(loader, hash, specs...);
```

Этого достаточно для генерации большого числа runtime и compile-time benchmark
cases. Основная проблема сейчас не в форме API, а в незафиксированной семантике,
отсутствии test oracle и нескольких correctness/build дефектах.

После этапов 0-3 API будет пригоден для серьёзных исследований. До этого крупные
таблицы производительности могут быть воспроизводимыми технически, но неверно
интерпретированными алгоритмически.

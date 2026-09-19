#include "M_cacher.h"
#include <sys/stat.h>


template <typename val_t, typename hash_t>
class Cache_cell
{
    private:
        std::unique_ptr<val_t> value{};         // int / *ptr / for ret

        std::unique_ptr<val_t> under_hash{};    // int / *ptr / check after search on hash because collisium
        hash_t hash{};                          // != *ptr => default template w/ default ~

        // линейный поиск по хешам!! время на создание хеша? хранить еще последний посчитанный хеш
        // хеш таблица на списках с особенностью переноса коллизионных списков в начало

    public:
        Cache_cell<val_t, hash_t>* prev{nullptr};
        Cache_cell<val_t, hash_t>* next{nullptr};

        Cache_cell(val_t v, hash_t h) : value(std::unique_ptr<val_t>(std::move(v))), hash(h) {}

        ~Cache_cell() = default;                // unique_ptr has auto clean

        // доп функ для сравнения тру валуе? + для ретерна страницы самой
        // hash func по-byte-овая


};


template <typename val_t, typename hash_t>
class L_one
{
    private:
        size_t num_cells = L1_LEN;

        Cache_cell* all_lists = new Cache_cell[num_cells] {};

        // номер свободного элемента
    public:
        Cache_cell<val_t, hash_t>* head_of_list{nullptr};
        Cache_cell<val_t, hash_t>* tail_of_list{nullptr};

        void do_hash(Cache_cell<val_t, hash_t>* list)
        {
            list->hash = XXH3_64bits(list->under_hash, check_byte_size(list->under_hash));
        }
};


int main(int argc, char* argv[])
{

    return 0;
}

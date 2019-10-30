#pragma once

#include "../wasms/table.hpp"
#include "state_history.hpp"

#include <abieos_exception.hpp>
#include <eosio/vm/backend.hpp>

namespace history_tools {

class iterator_cache {
  private:
    using db_view = state_history::rdb::db_view;
    db_view& view;

    struct table_key {
        uint64_t code  = {};
        uint64_t table = {};
        uint64_t scope = {};

        std::tuple<uint64_t, uint64_t, uint64_t> order() const { return {code, table, scope}; }
        friend bool                              operator<(const table_key& a, const table_key& b) { return a.order() < b.order(); }
    };
    std::map<table_key, int32_t> table_to_index;

    struct row_key {
        int32_t  table_index = {};
        uint64_t key         = {};

        std::tuple<int32_t, uint64_t> order() const { return {table_index, key}; }
        friend bool                   operator<(const row_key& a, const row_key& b) { return a.order() < b.order(); }
    };

    struct iterator {
        std::optional<db_view::iterator> view_it;
        std::vector<char>                value;
    };
    std::vector<iterator>      iterators;
    std::vector<iterator>      end_iterators;
    std::map<row_key, int32_t> key_to_iterator_index;

    int32_t get_table_index(table_key key) {
        auto map_it = table_to_index.find(key);
        if (map_it != table_to_index.end())
            return map_it->second;
        if (!exists(
                view.db,
                state_history::rdb::to_slice(abieos::native_to_key(std::make_tuple(
                    abieos::name{"system"}, abieos::name{"contract.tab"}, abieos::name{"primary"}, key.code, key.table, key.scope)))))
            return -1;
        if (table_to_index.size() != end_iterators.size())
            throw std::runtime_error("internal error: table_to_index.size() != end_iterators.size()");
        auto result = table_to_index.size();
        if (result > std::numeric_limits<int32_t>::max())
            throw std::runtime_error("too many open tables");
        table_to_index[key] = result;
        end_iterators.push_back({});
        return result;
    }

    int32_t get_iterator(row_key rk, db_view::iterator&& view_it) {
        iterator* it;
        int32_t   result;
        if (view_it.is_end()) {
            std::cout << "...end\n";
            it     = &end_iterators[rk.table_index];
            result = index_to_end_iterator(rk.table_index);
        } else {
            auto map_it = key_to_iterator_index.find(rk);
            if (map_it != key_to_iterator_index.end()) {
                std::cout << "...existing it (b)\n";
                it     = &iterators[map_it->second];
                result = map_it->second;
            } else {
                std::cout << "...new it\n";
                if (iterators.size() > std::numeric_limits<int32_t>::max())
                    throw std::runtime_error("too many iterators");
                result = iterators.size();
                iterators.emplace_back();
                it          = &iterators.back();
                auto  row   = abieos::bin_to_native<state_history::contract_row>(view_it.get_kv()->value);
                auto& value = std::get<0>(row).value;
                it->value.insert(it->value.end(), value.pos, value.end);
            }
        }
        if (!it->view_it)
            it->view_it = std::move(view_it);
        return result;
    }

    // Precondition: std::numeric_limits<int32_t>::min() < ei < -1
    // Iterator of -1 is reserved for invalid iterators (i.e. when the appropriate table has not yet been created).
    size_t end_iterator_to_index(int32_t ei) const { return (-ei - 2); }
    // Precondition: indx < _end_iterator_to_table.size() <= std::numeric_limits<int32_t>::max()
    int32_t index_to_end_iterator(size_t indx) const { return -(indx + 2); }

  public:
    iterator_cache(db_view& view)
        : view{view} {}

    size_t db_get_i64(int itr, char* buffer, uint32_t buffer_size) {
        if (itr < 0)
            throw std::runtime_error("dereference end iterator");
        if (itr >= iterators.size())
            throw std::runtime_error("dereference non-existing iterator");
        auto& it = iterators[itr];
        return copy_to_wasm(buffer, buffer_size, it.value.data(), it.value.size());
    }

    int32_t lower_bound(uint64_t code, uint64_t scope, uint64_t table, uint64_t key) {
        int32_t table_index = get_table_index({code, table, scope});
        if (table_index < 0) {
            std::cout << "...no table\n";
            return -1;
        }
        row_key rk{table_index, key};
        auto    map_it = key_to_iterator_index.find(rk);
        if (map_it != key_to_iterator_index.end()) {
            std::cout << "...existing it (a)\n";
            return map_it->second;
        }
        db_view::iterator it{view, abieos::native_to_key(std::make_tuple(
                                       abieos::name{"system"}, abieos::name{"contract.row"}, abieos::name{"primary"}, code, table, scope))};
        it.lower_bound(abieos::native_to_key(
            std::make_tuple(abieos::name{"system"}, abieos::name{"contract.row"}, abieos::name{"primary"}, code, table, scope, key)));
        return get_iterator(rk, std::move(it));
    }
}; // iterator_cache

struct chaindb_state {
    std::unique_ptr<history_tools::iterator_cache> iterator_cache;
};

template <typename Derived>
struct chaindb_callbacks {
    Derived& derived() { return static_cast<Derived&>(*this); }

    history_tools::iterator_cache& get_iterator_cache() {
        auto& chaindb_state = derived().get_chaindb_state();
        if (!chaindb_state.iterator_cache)
            chaindb_state.iterator_cache = std::make_unique<iterator_cache>(derived().get_db_view_state().view);
        return *chaindb_state.iterator_cache;
    }

    int db_store_i64(uint64_t scope, uint64_t table, uint64_t payer, uint64_t id, const char* buffer, uint32_t buffer_size) {
        throw std::runtime_error("unimplemented: db_store_i64");
    }

    void db_update_i64(int itr, uint64_t payer, const char* buffer, uint32_t buffer_size) {
        throw std::runtime_error("unimplemented: db_update_i64");
    }

    void db_remove_i64(int itr) { throw std::runtime_error("unimplemented: db_remove_i64"); }

    int db_get_i64(int itr, char* buffer, uint32_t buffer_size) {
        derived().check_bounds(buffer, buffer_size);
        return get_iterator_cache().db_get_i64(itr, buffer, buffer_size);
    }

    int db_next_i64(int itr, uint64_t& primary) {
        //
        throw std::runtime_error("unimplemented: db_next_i64");
    }

    int db_previous_i64(int itr, uint64_t& primary) {
        //
        throw std::runtime_error("unimplemented: db_previous_i64");
    }

    int db_find_i64(uint64_t code, uint64_t scope, uint64_t table, uint64_t id) {
        //
        throw std::runtime_error("unimplemented: db_find_i64");
    }

    int db_lowerbound_i64(uint64_t code, uint64_t scope, uint64_t table, uint64_t id) {
        return get_iterator_cache().lower_bound(code, scope, table, id);
    }

    int db_upperbound_i64(uint64_t code, uint64_t scope, uint64_t table, uint64_t id) {
        //
        throw std::runtime_error("unimplemented: db_upperbound_i64");
    }

    int db_end_i64(uint64_t code, uint64_t scope, uint64_t table) {
        //
        throw std::runtime_error("unimplemented: db_end_i64");
    }

    template <typename Rft, typename Allocator>
    static void register_callbacks() {
        Rft::template add<Derived, &Derived::db_store_i64, Allocator>("env", "db_store_i64");
        Rft::template add<Derived, &Derived::db_update_i64, Allocator>("env", "db_update_i64");
        Rft::template add<Derived, &Derived::db_remove_i64, Allocator>("env", "db_remove_i64");
        Rft::template add<Derived, &Derived::db_get_i64, Allocator>("env", "db_get_i64");
        Rft::template add<Derived, &Derived::db_next_i64, Allocator>("env", "db_next_i64");
        Rft::template add<Derived, &Derived::db_previous_i64, Allocator>("env", "db_previous_i64");
        Rft::template add<Derived, &Derived::db_find_i64, Allocator>("env", "db_find_i64");
        Rft::template add<Derived, &Derived::db_lowerbound_i64, Allocator>("env", "db_lowerbound_i64");
        Rft::template add<Derived, &Derived::db_upperbound_i64, Allocator>("env", "db_upperbound_i64");
        Rft::template add<Derived, &Derived::db_end_i64, Allocator>("env", "db_end_i64");
    }
};

} // namespace history_tools

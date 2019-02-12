// copyright defined in LICENSE.txt

#include "wasm_ql_lmdb_plugin.hpp"
#include "util.hpp"

#include <fc/exception/exception.hpp>

using namespace appbase;
namespace lmdb = state_history::lmdb;

static abstract_plugin& _wasm_ql_lmdb_plugin = app().register_plugin<wasm_ql_lmdb_plugin>();

struct lmdb_database_interface : database_interface, std::enable_shared_from_this<lmdb_database_interface> {
    std::shared_ptr<::lmdb_inst> lmdb_inst = app().find_plugin<lmdb_plugin>()->get_lmdb_inst();

    virtual ~lmdb_database_interface() {}

    virtual std::unique_ptr<query_session> create_query_session();
};

struct lmdb_query_session : query_session {
    std::shared_ptr<lmdb_database_interface> db_iface;
    lmdb::transaction                        tx;
    state_history::fill_status               fill_status;

    lmdb_query_session(const std::shared_ptr<lmdb_database_interface>& db_iface)
        : db_iface(db_iface)
        , tx{db_iface->lmdb_inst->lmdb_env, false} {

        auto f = lmdb::get<state_history::fill_status>(tx, db_iface->lmdb_inst->db, lmdb::make_fill_status_key(), false);
        if (f)
            fill_status = *f;
    }

    virtual ~lmdb_query_session() {}

    virtual state_history::fill_status get_fill_status() override { return fill_status; }

    virtual std::optional<abieos::checksum256> get_block_id(uint32_t block_index) override {
        auto rb = lmdb::get<lmdb::received_block>(tx, db_iface->lmdb_inst->db, lmdb::make_received_block_key(block_index), false);
        if (rb)
            return rb->block_id;
        return {};
    }

    virtual std::vector<char> exec_query(abieos::input_buffer query_bin, uint32_t head) override {
        abieos::name query_name;
        abieos::bin_to_native(query_name, query_bin);

        // todo: check if index is populated in lmdb
        auto it = db_iface->lmdb_inst->query_config.query_map.find(query_name);
        if (it == db_iface->lmdb_inst->query_config.query_map.end())
            throw std::runtime_error("exec_query: unknown query: " + (std::string)query_name);
        auto& query = *it->second;

        uint32_t max_block_index = 0;
        if (query.limit_block_index)
            max_block_index = std::min(head, abieos::bin_to_native<uint32_t>(query_bin));

        auto table_name_it = lmdb::table_names.find(query._table);
        if (table_name_it == lmdb::table_names.end())
            throw std::runtime_error("exec_query: unknown table: " + query._table);

        auto first = lmdb::make_table_index_key(table_name_it->second, query_name);
        auto last  = first;

        // todo
        if (!query.arg_types.empty())
            throw std::runtime_error("exec_query: query: " + (std::string)query_name + " not yet implemented");

        auto add_fields = [&](auto& dest, auto& types) {
            for (auto& type : types)
                type.bin_to_bin_key(dest, query_bin);
        };
        add_fields(first, query.range_types);
        add_fields(last, query.range_types);

        auto max_results = std::min(abieos::read_raw<uint32_t>(query_bin), query.max_results);

        std::vector<std::vector<char>> rows;
        uint32_t                       num_results = 0;
        for_each_subkey(tx, db_iface->lmdb_inst->db, first, last, [&](const auto& index_key, auto) {
            std::vector index_key_limit_block = index_key;
            if (query.is_state)
                lmdb::append_table_index_state_suffix(index_key_limit_block, max_block_index);
            // todo: unify lmdb's and pg's handling of negative result because of max_block_index
            for_each(tx, db_iface->lmdb_inst->db, index_key_limit_block, index_key, [&](auto zzz, auto delta_key) {
                auto delta_value = get_raw(tx, db_iface->lmdb_inst->db, delta_key);
                rows.emplace_back(delta_value.pos, delta_value.end);
                return false;
            });
            return ++num_results < max_results;
        });

        auto result = abieos::native_to_bin(rows);
        if ((uint32_t)result.size() != result.size())
            throw std::runtime_error("exec_query: result is too big");
        return result;
    }
}; // lmdb_query_session

std::unique_ptr<query_session> lmdb_database_interface::create_query_session() {
    auto session = std::make_unique<lmdb_query_session>(shared_from_this());
    return session;
}

struct wasm_ql_lmdb_plugin_impl {
    std::shared_ptr<lmdb_database_interface> interface;
};

wasm_ql_lmdb_plugin::wasm_ql_lmdb_plugin()
    : my(std::make_shared<wasm_ql_lmdb_plugin_impl>()) {}

wasm_ql_lmdb_plugin::~wasm_ql_lmdb_plugin() { ilog("wasm_ql_lmdb_plugin stopped"); }

void wasm_ql_lmdb_plugin::set_program_options(options_description& cli, options_description& cfg) {}

void wasm_ql_lmdb_plugin::plugin_initialize(const variables_map& options) {
    try {
        if (!my->interface)
            my->interface = std::make_shared<lmdb_database_interface>();
        app().find_plugin<wasm_ql_plugin>()->set_database(my->interface);
    }
    FC_LOG_AND_RETHROW()
}

void wasm_ql_lmdb_plugin::plugin_startup() {}
void wasm_ql_lmdb_plugin::plugin_shutdown() {}

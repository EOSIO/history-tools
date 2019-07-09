// copyright defined in LICENSE.txt

#include "wasm_ql_lmdb_plugin.hpp"
#include "util.hpp"

#include <fc/exception/exception.hpp>

using namespace appbase;
namespace lmdb = state_history::lmdb;

static abstract_plugin& _wasm_ql_lmdb_plugin = app().register_plugin<wasm_ql_lmdb_plugin>();

struct lmdb_database_interface : database_interface, std::enable_shared_from_this<lmdb_database_interface> {
    std::shared_ptr<::lmdb_inst> lmdb_inst;

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

    virtual std::optional<abieos::checksum256> get_block_id(uint32_t block_num) override {
        auto rb = lmdb::get<lmdb::received_block>(tx, db_iface->lmdb_inst->db, lmdb::make_received_block_key(block_num), false);
        if (rb)
            return rb->block_id;
        return {};
    }

    void append_fields(std::vector<char>& dest, abieos::input_buffer src, const std::vector<lmdb::key>& keys, bool xform_key) {
        for (auto& key : keys) {
            if (!key.field->byte_position)
                throw std::runtime_error("key " + key.name + " has unknown position");
            if (*key.field->byte_position > src.end - src.pos)
                throw std::runtime_error("key position is out of range");
            abieos::input_buffer key_pos{src.pos + *key.field->byte_position, src.end};
            if (xform_key)
                key.field->type_obj->bin_to_bin_key(dest, key_pos);
            else
                key.field->type_obj->bin_to_bin(dest, key_pos);
        }
    }

    virtual std::vector<char> query_database(abieos::input_buffer query_bin, uint32_t head) override {
        abieos::name query_name;
        abieos::bin_to_native(query_name, query_bin);

        // todo: check if index is populated in lmdb
        auto it = db_iface->lmdb_inst->query_config.query_map.find(query_name);
        if (it == db_iface->lmdb_inst->query_config.query_map.end())
            throw std::runtime_error("query_database: unknown query: " + (std::string)query_name);
        auto& query = *it->second;
        if (!query.arg_types.empty())
            throw std::runtime_error("query_database: query: " + (std::string)query_name + " not implemented");

        uint32_t max_block_num = 0;
        if (query.limit_block_num)
            max_block_num = std::min(head, abieos::bin_to_native<uint32_t>(query_bin));

        auto first = lmdb::make_table_index_key(query.table_obj->short_name, query_name);
        auto last  = first;

        auto add_fields = [&](auto& dest, auto& types) {
            for (auto& type : types) {
                auto p = query_bin.pos;
                type.query_to_bin_key(dest, query_bin);
            }
        };
        add_fields(first, query.range_types);
        add_fields(last, query.range_types);

        auto max_results = std::min(abieos::read_raw<uint32_t>(query_bin), query.max_results);

        std::vector<std::vector<char>> rows;
        uint32_t                       num_results = 0;
        for_each_subkey(tx, db_iface->lmdb_inst->db, first, last, [&](const auto& index_key, auto, auto) {
            std::vector index_key_limit_block = index_key;
            if (query.is_state)
                lmdb::append_table_index_state_suffix(index_key_limit_block, max_block_num);
            // todo: unify lmdb's and pg's handling of negative result because of max_block_num
            for_each(tx, db_iface->lmdb_inst->db, index_key_limit_block, index_key, [&](auto, auto delta_key) {
                auto delta_value = lmdb::get_raw(tx, db_iface->lmdb_inst->db, delta_key);
                rows.emplace_back(delta_value.pos, delta_value.end);
                if (query.join_table) {
                    auto join_key = lmdb::make_table_index_key(query.join_table->short_name, query.join_query_wasm_name);
                    append_fields(join_key, delta_value, query.join_key_values, true);
                    auto join_key_limit_block = join_key;
                    if (query.join_query->is_state)
                        lmdb::append_table_index_state_suffix(join_key_limit_block, max_block_num);

                    auto& row        = rows.back();
                    bool  found_join = false;
                    for_each(tx, db_iface->lmdb_inst->db, join_key_limit_block, join_key, [&](auto, auto join_delta_key) {
                        found_join            = true;
                        auto join_delta_value = lmdb::get_raw(tx, db_iface->lmdb_inst->db, join_delta_key);
                        append_fields(row, join_delta_value, query.fields_from_join, false);
                        return false;
                    });

                    if (!found_join)
                        rows.pop_back(); // todo: fill in empty instead?
                }
                return false;
            });
            return ++num_results < max_results;
        });

        auto result = abieos::native_to_bin(rows);
        if ((uint32_t)result.size() != result.size())
            throw std::runtime_error("query_database: result is too big");
        return result;
    }
}; // lmdb_query_session

std::unique_ptr<query_session> lmdb_database_interface::create_query_session() {
    if (!lmdb_inst)
        lmdb_inst = app().find_plugin<lmdb_plugin>()->get_lmdb_inst();
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

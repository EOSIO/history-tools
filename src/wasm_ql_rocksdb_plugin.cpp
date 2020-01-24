// copyright defined in LICENSE.txt

#include "wasm_ql_rocksdb_plugin.hpp"
#include "util.hpp"

#include <fc/exception/exception.hpp>

using namespace appbase;
namespace kv  = state_history::kv;
namespace rdb = state_history::rdb;

static abstract_plugin& _wasm_ql_rocksdb_plugin = app().register_plugin<wasm_ql_rocksdb_plugin>();

struct rocksdb_database_interface : database_interface, std::enable_shared_from_this<rocksdb_database_interface> {
    std::shared_ptr<::rocksdb_inst> rocksdb_inst;

    virtual ~rocksdb_database_interface() {}

    virtual std::unique_ptr<query_session> create_query_session();
};

struct rocksdb_query_session : query_session {
    std::shared_ptr<rocksdb_database_interface> db_iface;
    state_history::fill_status                  fill_status;
    std::unique_ptr<rocksdb::Iterator>          it_for_get;
    std::unique_ptr<rocksdb::Iterator>          it0;
    std::unique_ptr<rocksdb::Iterator>          it1;
    std::unique_ptr<rocksdb::Iterator>          it2;
    std::unique_ptr<rocksdb::Iterator>          it3;
    std::unique_ptr<rocksdb::Iterator>          it4;

    rocksdb_query_session(const std::shared_ptr<rocksdb_database_interface>& db_iface)
        : db_iface(db_iface)
        , it_for_get{db_iface->rocksdb_inst->database.db->NewIterator(rocksdb::ReadOptions())}
        , it0{db_iface->rocksdb_inst->database.db->NewIterator(rocksdb::ReadOptions())}
        , it1{db_iface->rocksdb_inst->database.db->NewIterator(rocksdb::ReadOptions())}
        , it2{db_iface->rocksdb_inst->database.db->NewIterator(rocksdb::ReadOptions())}
        , it3{db_iface->rocksdb_inst->database.db->NewIterator(rocksdb::ReadOptions())}
        , it4{db_iface->rocksdb_inst->database.db->NewIterator(rocksdb::ReadOptions())} {

        auto f = rdb::get<state_history::fill_status>(*it_for_get, kv::make_fill_status_key(), false);
        if (f)
            fill_status = *f;
    }

    virtual ~rocksdb_query_session() {}

    virtual state_history::fill_status get_fill_status() override { return fill_status; }

    virtual std::optional<abieos::checksum256> get_block_id(uint32_t block_num) override {
        auto rb = rdb::get<kv::received_block>(*it_for_get, kv::make_received_block_key(block_num), false);
        if (rb)
            return rb->block_id;
        return {};
    }

    void append_fields(
        std::vector<char>& dest, abieos::input_buffer src, const std::vector<kv::key>& keys,
        std::vector<std::optional<uint32_t>>& positions, bool xform_key) {

        for (auto& key : keys) {
            auto pos = positions.at(key.field->field_index);
            if (!pos)
                throw std::runtime_error("key " + key.name + " has unknown position");
            if (*pos > src.end - src.pos)
                throw std::runtime_error("key position is out of range");
            abieos::input_buffer key_pos{src.pos + *pos, src.end};
            if (xform_key)
                key.field->type_obj->bin_to_key(dest, key_pos);
            else
                key.field->type_obj->bin_to_bin(dest, key_pos);
        }
    }
}; // rocksdb_query_session

std::unique_ptr<query_session> rocksdb_database_interface::create_query_session() {
    auto session = std::make_unique<rocksdb_query_session>(shared_from_this());
    return session;
}

struct wasm_ql_rocksdb_plugin_impl {
    std::shared_ptr<rocksdb_database_interface> interface;
};

wasm_ql_rocksdb_plugin::wasm_ql_rocksdb_plugin()
    : my(std::make_shared<wasm_ql_rocksdb_plugin_impl>()) {}

wasm_ql_rocksdb_plugin::~wasm_ql_rocksdb_plugin() {}

void wasm_ql_rocksdb_plugin::set_program_options(options_description& cli, options_description& cfg) {}

void wasm_ql_rocksdb_plugin::plugin_initialize(const variables_map& options) {
    try {
        if (!my->interface) {
            my->interface               = std::make_shared<rocksdb_database_interface>();
            my->interface->rocksdb_inst = app().find_plugin<rocksdb_plugin>()->get_rocksdb_inst(true);
        }
        app().find_plugin<wasm_ql_plugin>()->set_database(my->interface);
    }
    FC_LOG_AND_RETHROW()
}

void wasm_ql_rocksdb_plugin::plugin_startup() {}
void wasm_ql_rocksdb_plugin::plugin_shutdown() { ilog("wasm_ql_rocksdb_plugin stopped"); }

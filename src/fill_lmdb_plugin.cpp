// copyright defined in LICENSE.txt

#include "fill_lmdb_plugin.hpp"
#include "queries.hpp"
#include "state_history_lmdb.hpp"
#include "util.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <fc/exception/exception.hpp>

using namespace abieos;
using namespace appbase;
using namespace std::literals;
namespace lmdb = state_history::lmdb;

using std::enable_shared_from_this;
using std::exception;
using std::make_shared;
using std::make_unique;
using std::map;
using std::max;
using std::min;
using std::optional;
using std::runtime_error;
using std::shared_ptr;
using std::string;
using std::string_view;
using std::to_string;
using std::unique_ptr;
using std::variant;
using std::vector;

namespace asio      = boost::asio;
namespace bpo       = boost::program_options;
namespace websocket = boost::beast::websocket;

using asio::ip::tcp;
using boost::beast::flat_buffer;
using boost::system::error_code;

struct lmdb_type {
    void (*bin_to_bin)(std::vector<char>&, input_buffer&)     = nullptr;
    void (*bin_to_bin_key)(std::vector<char>&, input_buffer&) = nullptr;
};

template <typename T>
void bin_to_bin(std::vector<char>& dest, input_buffer& bin) {
    abieos::native_to_bin(dest, abieos::bin_to_native<T>(bin));
}

template <>
void bin_to_bin<abieos::uint128>(std::vector<char>& dest, input_buffer& bin) {
    bin_to_bin<uint64_t>(dest, bin);
    bin_to_bin<uint64_t>(dest, bin);
}

template <>
void bin_to_bin<abieos::int128>(std::vector<char>& dest, input_buffer& bin) {
    bin_to_bin<uint64_t>(dest, bin);
    bin_to_bin<uint64_t>(dest, bin);
}

template <>
void bin_to_bin<state_history::transaction_status>(std::vector<char>& dest, input_buffer& bin) {
    return bin_to_bin<std::underlying_type_t<state_history::transaction_status>>(dest, bin);
}

template <typename T>
void bin_to_bin_key(std::vector<char>& dest, input_buffer& bin) {
    lmdb::fixup_key<T>(dest, [&] { bin_to_bin<T>(dest, bin); });
}

template <typename T>
constexpr lmdb_type make_lmdb_type_for() {
    return lmdb_type{bin_to_bin<T>, bin_to_bin_key<T>};
}

// clang-format off
const map<string, lmdb_type> abi_type_to_lmdb_type = {
    {"bool",                    make_lmdb_type_for<bool>()},
    {"varuint32",               make_lmdb_type_for<varuint32>()},
    {"uint8",                   make_lmdb_type_for<uint8_t>()},
    {"uint16",                  make_lmdb_type_for<uint16_t>()},
    {"uint32",                  make_lmdb_type_for<uint32_t>()},
    {"uint64",                  make_lmdb_type_for<uint64_t>()},
    {"uint128",                 make_lmdb_type_for<abieos::uint128>()},
    {"int8",                    make_lmdb_type_for<int8_t>()},
    {"int16",                   make_lmdb_type_for<int16_t>()},
    {"int32",                   make_lmdb_type_for<int32_t>()},
    {"int64",                   make_lmdb_type_for<int64_t>()},
    {"int128",                  make_lmdb_type_for<abieos::int128>()},
    {"float64",                 make_lmdb_type_for<double>()},
    {"float128",                make_lmdb_type_for<float128>()},
    {"name",                    make_lmdb_type_for<name>()},
    {"string",                  make_lmdb_type_for<string>()},
    {"time_point",              make_lmdb_type_for<time_point>()},
    {"time_point_sec",          make_lmdb_type_for<time_point_sec>()},
    {"block_timestamp_type",    make_lmdb_type_for<block_timestamp>()},
    {"checksum256",             make_lmdb_type_for<checksum256>()},
    {"public_key",              make_lmdb_type_for<public_key>()},
    {"bytes",                   make_lmdb_type_for<bytes>()},
    {"transaction_status",      make_lmdb_type_for<state_history::transaction_status>()},
};
// clang-format on

struct session;

struct fill_lmdb_config {
    string                            host;
    string                            port;
    uint32_t                          db_size_mb   = 0;
    uint32_t                          skip_to      = 0;
    uint32_t                          stop_before  = 0;
    bool                              enable_trim  = false;
    ::query_config::config<lmdb_type> query_config = {};
};

struct fill_lmdb_plugin_impl : std::enable_shared_from_this<fill_lmdb_plugin_impl> {
    shared_ptr<fill_lmdb_config> config = make_shared<fill_lmdb_config>();
    shared_ptr<::session>        session;

    ~fill_lmdb_plugin_impl();
};

struct session : enable_shared_from_this<session> {
    fill_lmdb_plugin_impl*                          my = nullptr;
    shared_ptr<fill_lmdb_config>                    config;
    lmdb::env                                       lmdb_env;
    lmdb::database                                  db{lmdb_env};
    tcp::resolver                                   resolver;
    websocket::stream<tcp::socket>                  stream;
    bool                                            received_abi    = false;
    std::map<std::string, std::vector<std::string>> table_keys      = {};
    bool                                            created_trim    = false;
    uint32_t                                        head            = 0;
    abieos::checksum256                             head_id         = {};
    uint32_t                                        irreversible    = 0;
    abieos::checksum256                             irreversible_id = {};
    uint32_t                                        first           = 0;
    uint32_t                                        first_bulk      = 0;
    abi_def                                         abi             = {};
    map<string, abi_type>                           abi_types       = {};

    session(fill_lmdb_plugin_impl* my, asio::io_context& ioc)
        : my(my)
        , config(my->config)
        , lmdb_env(config->db_size_mb)
        , resolver(ioc)
        , stream(ioc) {

        ilog("connect to lmdb");
        stream.binary(true);
        stream.read_message_max(1024 * 1024 * 1024);
    }

    void start() {
        ilog("connect to ${h}:${p}", ("h", config->host)("p", config->port));
        resolver.async_resolve(
            config->host, config->port, [self = shared_from_this(), this](error_code ec, tcp::resolver::results_type results) {
                callback(ec, "resolve", [&] {
                    asio::async_connect(
                        stream.next_layer(), results.begin(), results.end(), [self = shared_from_this(), this](error_code ec, auto&) {
                            callback(ec, "connect", [&] {
                                stream.async_handshake(config->host, "/", [self = shared_from_this(), this](error_code ec) {
                                    callback(ec, "handshake", [&] { //
                                        start_read();
                                    });
                                });
                            });
                        });
                });
            });
    }

    void start_read() {
        auto in_buffer = make_shared<flat_buffer>();
        stream.async_read(*in_buffer, [self = shared_from_this(), this, in_buffer](error_code ec, size_t) {
            callback(ec, "async_read", [&] {
                if (!received_abi)
                    receive_abi(in_buffer);
                else {
                    if (!receive_result(in_buffer)) {
                        close();
                        return;
                    }
                }
                start_read();
            });
        });
    }

    void receive_abi(const shared_ptr<flat_buffer>& p) {
        auto data = p->data();
        json_to_native(abi, string_view{(const char*)data.data(), data.size()});
        check_abi_version(abi.version);
        abi_types    = create_contract(abi).abi_types;
        received_abi = true;

        std::string error;
        jvalue      j;
        if (!json_to_jvalue(j, error, string_view{(const char*)data.data(), data.size()}))
            throw std::runtime_error(error);
        for (auto& t : std::get<jarray>(std::get<jobject>(j.value)["tables"].value)) {
            auto& o          = std::get<jobject>(t.value);
            auto& table_name = std::get<std::string>(o["name"].value);
            auto& keys       = table_keys[table_name];
            for (auto& key : std::get<jarray>(o["key_names"].value))
                keys.push_back(std::get<std::string>(key.value));
        }

        lmdb::transaction t{lmdb_env, true};
        load_fill_status(t);
        auto positions = get_positions(t);
        truncate(t, head + 1);
        t.commit();

        send_request(positions);
    }

    void load_fill_status(lmdb::transaction& t) {
        auto r          = lmdb::get<lmdb::fill_status>(t, db, lmdb::make_fill_status_key(), false);
        head            = r.head;
        head_id         = r.head_id;
        irreversible    = r.irreversible;
        irreversible_id = r.irreversible_id;
        first           = r.first;
    }

    jarray get_positions(lmdb::transaction& t) {
        jarray result;
        if (head) {
            for (uint32_t i = irreversible; i <= head; ++i) {
                auto rb = lmdb::get<lmdb::received_block>(t, db, lmdb::make_received_block_key(i));
                result.push_back(jvalue{jobject{
                    {{"block_num"s}, jvalue{std::to_string(rb.block_index)}},
                    {{"block_id"s}, jvalue{(string)rb.block_id}},
                }});
            }
        }
        return result;
    }

    void write_fill_status(lmdb::transaction& t) {
        if (irreversible < head)
            put(t, db, lmdb::make_fill_status_key(),
                lmdb::fill_status{
                    .head = head, .head_id = head_id, .irreversible = irreversible, .irreversible_id = irreversible_id, .first = first},
                true);
        else
            put(t, db, lmdb::make_fill_status_key(),
                lmdb::fill_status{.head = head, .head_id = head_id, .irreversible = head, .irreversible_id = head_id, .first = first},
                true);
    }

    void truncate(lmdb::transaction& t, uint32_t block) {
        for_each(t, db, lmdb::make_block_key(block), lmdb::make_block_key(), [&](auto k, auto v) {
            lmdb::check(mdb_del(t.tx, db.db, lmdb::addr(lmdb::to_const_val(k)), lmdb::addr(lmdb::to_const_val(v))), "truncate: ");
        });

        auto rb = lmdb::get<lmdb::received_block>(t, db, lmdb::make_received_block_key(block - 1), false);
        if (!rb.block_index) {
            head    = 0;
            head_id = {};
        } else {
            head    = block - 1;
            head_id = rb.block_id;
        }
        first = std::min(first, head);
    }

    bool receive_result(const shared_ptr<flat_buffer>& p) {
        auto         data = p->data();
        input_buffer bin{(const char*)data.data(), (const char*)data.data() + data.size()};
        check_variant(bin, get_type("result"), "get_blocks_result_v0");

        state_history::get_blocks_result_v0 result;
        bin_to_native(result, bin);

        if (!result.this_block)
            return true;

        if (config->stop_before && result.this_block->block_num >= config->stop_before) {
            ilog("block ${b}: stop requested", ("b", result.this_block->block_num));
            return false;
        }

        if (result.this_block->block_num <= head)
            ilog("switch forks at block ${b}", ("b", result.this_block->block_num));

        trim();
        ilog("block ${b}", ("b", result.this_block->block_num));

        lmdb::transaction t{lmdb_env, true};
        if (result.this_block->block_num <= head)
            truncate(t, result.this_block->block_num);
        if (head_id != abieos::checksum256{} && (!result.prev_block || result.prev_block->block_id != head_id))
            throw runtime_error("prev_block does not match");
        if (result.block)
            receive_block(result.this_block->block_num, result.this_block->block_id, *result.block, t);
        if (result.deltas)
            receive_deltas(t, result.this_block->block_num, *result.deltas);
        if (result.traces)
            receive_traces(result.this_block->block_num, *result.traces);

        head            = result.this_block->block_num;
        head_id         = result.this_block->block_id;
        irreversible    = result.last_irreversible.block_num;
        irreversible_id = result.last_irreversible.block_id;
        if (!first)
            first = head;
        write_fill_status(t);

        put(t, db, lmdb::make_received_block_key(result.this_block->block_num),
            lmdb::received_block{result.this_block->block_num, result.this_block->block_id});

        t.commit();
        return true;
    } // receive_result()

    void fill(
        std::vector<char>& key, std::vector<char>& data, const std::vector<std::string>* keys, size_t& next_key, input_buffer& bin,
        abi_field& field) {
        if (field.type->filled_struct) {
            for (auto& f : field.type->fields)
                fill(key, data, keys, next_key, bin, f);
        } else if (field.type->filled_variant && field.type->fields.size() == 1 && field.type->fields[0].type->filled_struct) {
            auto v = read_varuint32(bin);
            if (v)
                throw std::runtime_error("invalid variant in " + field.type->name);
            abieos::push_varuint32(data, v);
            for (auto& f : field.type->fields[0].type->fields)
                fill(key, data, keys, next_key, bin, f);
        } else if (field.type->array_of && field.type->array_of->filled_struct) {
            uint32_t n = read_varuint32(bin);
            abieos::push_varuint32(data, n);
            for (uint32_t i = 0; i < n; ++i)
                for (auto& f : field.type->array_of->fields)
                    fill(key, data, keys, next_key, bin, f);
        } else {
            auto abi_type    = field.type->name;
            bool is_optional = false;
            if (abi_type.size() >= 1 && abi_type.back() == '?') {
                is_optional = true;
                abi_type.resize(abi_type.size() - 1);
            }
            auto it = abi_type_to_lmdb_type.find(abi_type);
            if (it == abi_type_to_lmdb_type.end())
                throw std::runtime_error("don't know lmdb type for abi type: " + abi_type);
            if (!it->second.bin_to_bin)
                throw std::runtime_error("don't know how to process " + field.type->name);
            if (is_optional) {
                bool exists = read_raw<bool>(bin);
                abieos::push_raw<bool>(data, exists);
                if (exists)
                    it->second.bin_to_bin(data, bin);
            } else {
                if (keys && next_key < keys->size() && (*keys)[next_key] == field.name) {
                    auto key_bin = bin;
                    it->second.bin_to_bin_key(key, key_bin);
                    ++next_key;
                }
                it->second.bin_to_bin(data, bin);
            }
        }
    } // fill

    void receive_block(uint32_t block_index, const checksum256& block_id, input_buffer bin, lmdb::transaction& t) {
        state_history::signed_block block;
        bin_to_native(block, bin);
        lmdb::block_info info{
            .block_index       = block_index,
            .block_id          = block_id,
            .timestamp         = block.timestamp,
            .producer          = block.producer,
            .confirmed         = block.confirmed,
            .previous          = block.previous,
            .transaction_mroot = block.transaction_mroot,
            .action_mroot      = block.action_mroot,
            .schedule_version  = block.schedule_version,
            .new_producers     = block.new_producers ? *block.new_producers : state_history::producer_schedule{},
        };
        put(t, db, lmdb::make_block_info_key(block_index), info);
    } // receive_block

    void receive_deltas(lmdb::transaction& t, uint32_t block_num, input_buffer buf) {
        auto         data = zlib_decompress(buf);
        input_buffer bin{data.data(), data.data() + data.size()};

        auto     num     = read_varuint32(bin);
        unsigned numRows = 0;
        for (uint32_t i = 0; i < num; ++i) {
            check_variant(bin, get_type("table_delta"), "table_delta_v0");
            state_history::table_delta_v0 table_delta;
            bin_to_native(table_delta, bin);

            auto table_name_it = lmdb::table_names.find(table_delta.name);
            if (table_name_it == lmdb::table_names.end())
                throw std::runtime_error("unknown table \"" + table_delta.name + "\"");
            auto table_name = table_name_it->second;

            auto& variant_type = get_type(table_delta.name);
            if (!variant_type.filled_variant || variant_type.fields.size() != 1 || !variant_type.fields[0].type->filled_struct)
                throw std::runtime_error("don't know how to proccess " + variant_type.name);
            auto&       type = *variant_type.fields[0].type;
            const auto& keys = table_keys[table_delta.name];

            size_t num_processed = 0;
            for (auto& row : table_delta.rows) {
                if (table_delta.rows.size() > 10000 && !(num_processed % 10000))
                    ilog(
                        "block ${b} ${t} ${n} of ${r}",
                        ("b", block_num)("t", table_delta.name)("n", num_processed)("r", table_delta.rows.size()));
                check_variant(row.data, variant_type, 0u);
                auto              key = lmdb::make_delta_key(block_num, row.present, table_name);
                std::vector<char> data;
                size_t            next_key = 0;
                for (auto& field : type.fields)
                    fill(key, data, &keys, next_key, row.data, field);
                if (next_key < keys.size())
                    throw std::runtime_error("missing table \"" + table_delta.name + "\" key \"" + keys[next_key] + "\"");
                lmdb::put(t, db, key, data);
                ++num_processed;
            }
            numRows += table_delta.rows.size();
        }
    } // receive_deltas

    void receive_traces(uint32_t block_num, input_buffer buf) {
        // todo
    }

    void trim() {
        if (!config->enable_trim)
            return;
        auto end_trim = min(head, irreversible);
        if (first >= end_trim)
            return;
        ilog("trim  ${b} - ${e}", ("b", first)("e", end_trim));
        // todo
        ilog("      done");
        first = end_trim;
    }

    void send_request(const jarray& positions) {
        send(jvalue{jarray{{"get_blocks_request_v0"s},
                           {jobject{
                               {{"start_block_num"s}, {to_string(max(config->skip_to, head + 1))}},
                               {{"end_block_num"s}, {"4294967295"s}},
                               {{"max_messages_in_flight"s}, {"4294967295"s}},
                               {{"have_positions"s}, {positions}},
                               {{"irreversible_only"s}, {false}},
                               {{"fetch_block"s}, {true}},
                               {{"fetch_traces"s}, {true}},
                               {{"fetch_deltas"s}, {true}},
                           }}}});
    }

    const abi_type& get_type(const string& name) {
        auto it = abi_types.find(name);
        if (it == abi_types.end())
            throw runtime_error("unknown type "s + name);
        return it->second;
    }

    void send(const jvalue& value) {
        auto bin = make_shared<vector<char>>();
        json_to_bin(*bin, &get_type("request"), value);
        stream.async_write(
            asio::buffer(*bin), [self = shared_from_this(), bin, this](error_code ec, size_t) { callback(ec, "async_write", [&] {}); });
    }

    void check_variant(input_buffer& bin, const abi_type& type, uint32_t expected) {
        auto index = read_varuint32(bin);
        if (!type.filled_variant)
            throw runtime_error(type.name + " is not a variant"s);
        if (index >= type.fields.size())
            throw runtime_error("expected "s + type.fields[expected].name + " got " + to_string(index));
        if (index != expected)
            throw runtime_error("expected "s + type.fields[expected].name + " got " + type.fields[index].name);
    }

    void check_variant(input_buffer& bin, const abi_type& type, const char* expected) {
        auto index = read_varuint32(bin);
        if (!type.filled_variant)
            throw runtime_error(type.name + " is not a variant"s);
        if (index >= type.fields.size())
            throw runtime_error("expected "s + expected + " got " + to_string(index));
        if (type.fields[index].name != expected)
            throw runtime_error("expected "s + expected + " got " + type.fields[index].name);
    }

    template <typename F>
    void catch_and_close(F f) {
        try {
            f();
        } catch (const exception& e) {
            elog("${e}", ("e", e.what()));
            close();
        } catch (...) {
            elog("unknown exception");
            close();
        }
    }

    template <typename F>
    void callback(error_code ec, const char* what, F f) {
        if (ec)
            return on_fail(ec, what);
        catch_and_close(f);
    }

    void on_fail(error_code ec, const char* what) {
        try {
            elog("${w}: ${m}", ("w", what)("m", ec.message()));
            close();
        } catch (...) {
            elog("exception while closing");
        }
    }

    void close() {
        stream.next_layer().close();
        if (my)
            my->session.reset();
    }

    ~session() { ilog("fill-lmdb stopped"); }
}; // session

static abstract_plugin& _fill_lmdb_plugin = app().register_plugin<fill_lmdb_plugin>();

fill_lmdb_plugin_impl::~fill_lmdb_plugin_impl() {
    if (session)
        session->my = nullptr;
}

fill_lmdb_plugin::fill_lmdb_plugin()
    : my(make_shared<fill_lmdb_plugin_impl>()) {}

fill_lmdb_plugin::~fill_lmdb_plugin() {}

void fill_lmdb_plugin::set_program_options(options_description& cli, options_description& cfg) {
    auto op   = cfg.add_options();
    auto clop = cli.add_options();
    op("endpoint,e", bpo::value<string>()->default_value("localhost:8080"), "State-history endpoint to connect to (nodeos)");
    op("query-config,q", bpo::value<std::string>()->default_value("../src/query-config.json"), "Query configuration");
    op("trim,t", "Trim history before irreversible");
    clop(
        "set-db-size-mb", bpo::value<uint32_t>(),
        "Increase database size to [arg]. This option will grow the database size limit, but not shrink it");
    clop("skip-to,k", bpo::value<uint32_t>(), "Skip blocks before [arg]");
    clop("stop,x", bpo::value<uint32_t>(), "Stop before block [arg]");
}

void fill_lmdb_plugin::plugin_initialize(const variables_map& options) {
    try {
        auto endpoint           = options.at("endpoint").as<string>();
        auto port               = endpoint.substr(endpoint.find(':') + 1, endpoint.size());
        auto host               = endpoint.substr(0, endpoint.find(':'));
        my->config->host        = host;
        my->config->port        = port;
        my->config->db_size_mb  = options.count("set-db-size-mb") ? options["set-db-size-mb"].as<uint32_t>() : 0;
        my->config->skip_to     = options.count("skip-to") ? options["skip-to"].as<uint32_t>() : 0;
        my->config->stop_before = options.count("stop") ? options["stop"].as<uint32_t>() : 0;
        my->config->enable_trim = options.count("trim");

        auto x = read_string(options["query-config"].as<std::string>().c_str());
        try {
            json_to_native(my->config->query_config, x);
        } catch (const std::exception& e) {
            throw std::runtime_error("error processing " + options["query-config"].as<std::string>() + ": " + e.what());
        }
        my->config->query_config.prepare(abi_type_to_lmdb_type);
    }
    FC_LOG_AND_RETHROW()
}

void fill_lmdb_plugin::plugin_startup() {
    my->session = make_shared<session>(my.get(), app().get_io_service());
    my->session->start();
}

void fill_lmdb_plugin::plugin_shutdown() {
    if (my->session)
        my->session->close();
}

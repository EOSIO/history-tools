// copyright defined in LICENSE.txt

#pragma once
#include "query_config.hpp"
#include "state_history.hpp"

namespace state_history {
namespace kv {

using namespace abieos::literals;

// clang-format off
inline const std::map<std::string, abieos::name> table_names = {
    {"block_info",                  "block.info"_n},
    {"transaction_trace",           "ttrace"_n},
    {"action_trace",                "atrace"_n},

    {"account",                     "account"_n},
    {"account_metadata",            "account.meta"_n},
    {"code",                        "code"_n},
    {"contract_table",              "c.table"_n},
    {"contract_row",                "c.row"_n},
    {"contract_index64",            "c.index64"_n},
    {"contract_index128",           "c.index128"_n},
    {"contract_index256",           "c.index128"_n},
    {"contract_index_double",       "c.index.d"_n},
    {"contract_index_long_double",  "c.index.ld"_n},
    {"global_property",             "glob.prop"_n},
    {"generated_transaction",       "gen.tx"_n},
    {"protocol_state",              "protocol.st"_n},
    {"permission",                  "permission"_n},
    {"permission_link",             "perm.link"_n},
    {"resource_limits",             "res.lim"_n},
    {"resource_usage",              "res.usage"_n},
    {"resource_limits_state",       "res.lim.stat"_n},
    {"resource_limits_config",      "res.lim.conf"_n},
};
// clang-format on

inline void inc_key(std::vector<char>& key) {
    for (auto it = key.rbegin(); it != key.rend(); ++it)
        if (++*it)
            return;
}

template <typename F>
void reverse_bin(std::vector<char>& bin, F f) {
    auto s = bin.size();
    f();
    std::reverse(bin.begin() + s, bin.end());
}

// Modify serialization of types so lexigraphical sort matches data sort
template <typename T, typename F>
void fixup_key(std::vector<char>& bin, F f) {
    if constexpr (
        std::is_unsigned_v<T> || std::is_same_v<std::decay_t<T>, abieos::name> || std::is_same_v<std::decay_t<T>, abieos::uint128> ||
        std::is_same_v<std::decay_t<T>, abieos::checksum256>)
        reverse_bin(bin, f);
    else
        throw std::runtime_error("unsupported key type");
}

template <typename T>
void native_to_bin_key(std::vector<char>& bin, const T& obj) {
    fixup_key<T>(bin, [&] { abieos::native_to_bin(obj, bin); });
}

template <typename T>
T bin_to_native_key(abieos::input_buffer& b) {
    if constexpr (
        std::is_unsigned_v<T> || std::is_same_v<std::decay_t<T>, abieos::name> || std::is_same_v<std::decay_t<T>, abieos::uint128> ||
        std::is_same_v<std::decay_t<T>, abieos::checksum256>) {
        if (b.pos + sizeof(T) > b.end)
            throw std::runtime_error("key deserialization error");
        std::vector<char> v(b.pos, b.pos + sizeof(T));
        b.pos += sizeof(T);
        std::reverse(v.begin(), v.end());
        auto br = abieos::input_buffer{v.data(), v.data() + v.size()};
        return abieos::bin_to_native<T>(br);
    } else {
        throw std::runtime_error("unsupported key type");
    }
}

struct type {
    void (*bin_to_bin)(std::vector<char>&, abieos::input_buffer&)   = nullptr;
    void (*bin_to_key)(std::vector<char>&, abieos::input_buffer&)   = nullptr;
    void (*key_to_key)(std::vector<char>&, abieos::input_buffer&)   = nullptr;
    void (*query_to_key)(std::vector<char>&, abieos::input_buffer&) = nullptr;
    void (*lower_bound_key)(std::vector<char>&)                     = nullptr;
    void (*upper_bound_key)(std::vector<char>&)                     = nullptr;
    bool (*skip_bin)(abieos::input_buffer&)                         = nullptr;
    bool (*skip_key)(abieos::input_buffer&)                         = nullptr;
    void (*fill_empty)(std::vector<char>&)                          = nullptr;
};

template <typename T>
void bin_to_bin(std::vector<char>& dest, abieos::input_buffer& bin) {
    abieos::native_to_bin(abieos::bin_to_native<T>(bin), dest);
}

template <>
inline void bin_to_bin<abieos::uint128>(std::vector<char>& dest, abieos::input_buffer& bin) {
    bin_to_bin<uint64_t>(dest, bin);
    bin_to_bin<uint64_t>(dest, bin);
}

template <>
inline void bin_to_bin<abieos::int128>(std::vector<char>& dest, abieos::input_buffer& bin) {
    bin_to_bin<uint64_t>(dest, bin);
    bin_to_bin<uint64_t>(dest, bin);
}

template <>
inline void bin_to_bin<state_history::transaction_status>(std::vector<char>& dest, abieos::input_buffer& bin) {
    return bin_to_bin<std::underlying_type_t<state_history::transaction_status>>(dest, bin);
}

template <typename T>
void bin_to_key(std::vector<char>& dest, abieos::input_buffer& bin) {
    if constexpr (std::is_same_v<std::decay_t<T>, abieos::varuint32>) {
        reverse_bin(dest, [&] { abieos::native_to_bin(abieos::bin_to_native<abieos::varuint32>(bin).value, dest); });
    } else {
        fixup_key<T>(dest, [&] { bin_to_bin<T>(dest, bin); });
    }
}

template <typename T>
void key_to_key(std::vector<char>& dest, abieos::input_buffer& bin) {
    if constexpr (std::is_same_v<std::decay_t<T>, abieos::varuint32>) {
        bin_to_bin<uint32_t>(dest, bin);
    } else {
        bin_to_bin<T>(dest, bin);
    }
}

template <typename T>
void query_to_key(std::vector<char>& dest, abieos::input_buffer& bin) {
    if constexpr (std::is_same_v<std::decay_t<T>, abieos::varuint32>) {
        fixup_key<uint32_t>(dest, [&] { bin_to_bin<uint32_t>(dest, bin); });
    } else {
        fixup_key<T>(dest, [&] { bin_to_bin<T>(dest, bin); });
    }
}

template <typename T>
void lower_bound_key(std::vector<char>& dest) {
    if constexpr (
        std::is_unsigned_v<T> || std::is_same_v<std::decay_t<T>, abieos::name> || std::is_same_v<std::decay_t<T>, abieos::uint128> ||
        std::is_same_v<std::decay_t<T>, abieos::checksum256>)
        dest.resize(dest.size() + sizeof(T));
    else
        throw std::runtime_error("unsupported key type");
}

template <typename T>
void upper_bound_key(std::vector<char>& dest) {
    if constexpr (
        std::is_unsigned_v<T> || std::is_same_v<std::decay_t<T>, abieos::name> || std::is_same_v<std::decay_t<T>, abieos::uint128> ||
        std::is_same_v<std::decay_t<T>, abieos::checksum256>)
        dest.resize(dest.size() + sizeof(T), 0xff);
    else
        throw std::runtime_error("unsupported key type");
}

template <typename T>
bool skip_bin(abieos::input_buffer& bin) {
    if constexpr (
        std::is_integral_v<T> || std::is_same_v<std::decay_t<T>, abieos::name> || std::is_same_v<std::decay_t<T>, abieos::uint128> ||
        std::is_same_v<std::decay_t<T>, abieos::checksum256> || std::is_same_v<std::decay_t<T>, abieos::time_point> ||
        std::is_same_v<std::decay_t<T>, abieos::block_timestamp>) {
        if (size_t(bin.end - bin.pos) < sizeof(T))
            throw std::runtime_error("skip past end");
        bin.pos += sizeof(T);
        return true;
    } else {
        return false;
    }
}

template <typename T>
bool skip_key(abieos::input_buffer& bin) {
    if constexpr (
        std::is_integral_v<T> || std::is_same_v<std::decay_t<T>, abieos::name> || std::is_same_v<std::decay_t<T>, abieos::uint128> ||
        std::is_same_v<std::decay_t<T>, abieos::checksum256> || std::is_same_v<std::decay_t<T>, abieos::time_point> ||
        std::is_same_v<std::decay_t<T>, abieos::block_timestamp>) {
        if (size_t(bin.end - bin.pos) < sizeof(T))
            throw std::runtime_error("skip past end");
        bin.pos += sizeof(T);
        return true;
    } else if constexpr (std::is_same_v<std::decay_t<T>, abieos::varuint32>) {
        return skip_key<uint32_t>(bin);
    } else {
        return false;
    }
}

template <typename T>
void fill_empty(std::vector<char>& dest) {
    if constexpr (
        std::is_integral_v<T> || std::is_same_v<std::decay_t<T>, abieos::name> || std::is_same_v<std::decay_t<T>, abieos::uint128> ||
        std::is_same_v<std::decay_t<T>, abieos::checksum256> || std::is_same_v<std::decay_t<T>, abieos::time_point> ||
        std::is_same_v<std::decay_t<T>, abieos::block_timestamp>) {
        dest.insert(dest.end(), sizeof(T), 0);
    } else if constexpr (std::is_same_v<std::decay_t<T>, abieos::bytes>) {
        dest.push_back(0);
    } else {
        throw std::runtime_error("unsupported fill_empty type");
    }
}

template <typename T>
constexpr type make_type_for() {
    return type{bin_to_bin<T>,      bin_to_key<T>, key_to_key<T>, query_to_key<T>, lower_bound_key<T>,
                upper_bound_key<T>, skip_bin<T>,   skip_key<T>,   fill_empty<T>};
}

// clang-format off
const inline std::map<std::string, type> abi_type_to_kv_type = {
    {"bool",                    make_type_for<bool>()},
    {"varuint32",               make_type_for<abieos::varuint32>()},
    {"uint8",                   make_type_for<uint8_t>()},
    {"uint16",                  make_type_for<uint16_t>()},
    {"uint32",                  make_type_for<uint32_t>()},
    {"uint64",                  make_type_for<uint64_t>()},
    {"uint128",                 make_type_for<abieos::uint128>()},
    {"int8",                    make_type_for<int8_t>()},
    {"int16",                   make_type_for<int16_t>()},
    {"int32",                   make_type_for<int32_t>()},
    {"int64",                   make_type_for<int64_t>()},
    {"int128",                  make_type_for<abieos::int128>()},
    {"float64",                 make_type_for<double>()},
    {"float128",                make_type_for<abieos::float128>()},
    {"name",                    make_type_for<abieos::name>()},
    {"string",                  make_type_for<std::string>()},
    {"time_point",              make_type_for<abieos::time_point>()},
    {"time_point_sec",          make_type_for<abieos::time_point_sec>()},
    {"block_timestamp_type",    make_type_for<abieos::block_timestamp>()},
    {"checksum256",             make_type_for<abieos::checksum256>()},
    {"public_key",              make_type_for<abieos::public_key>()},
    {"bytes",                   make_type_for<abieos::bytes>()},
    {"transaction_status",      make_type_for<state_history::transaction_status>()},
};
// clang-format on

// Key                                                                          Value                               Notes   Description
// =================================================================================================================================================
// key_tag::table,  block_num, table_name, present_k, pk,               ## present_v,(fields iff present_v) ## 1,2  ## traces, deltas, reducer_outputs
// key_tag::index,  table_name, index_name, key, ~block_num, !present_k ## (none)                           ## 1    ## indexes. key is superset of pk fields
//
// * Keys are serialized in a lexigraphical sort format. See native_to_bin_key() and bin_to_native_key().
// * Erase range lower_bound(make_table_key(n)) to upper_bound(make_table_key()) to erase blocks >= n.
//   Also remove index entries corresponding to each removed row.
// * pk and fields may be empty
// * block_num is 0 for tables which don't support history (e.g. fill_status)
//
// * present_k (present as a key)
//   * nodeos deltas:     used
//   * all other cases:   =1
//
// * present_v (present as a value)
//   * nodeos deltas:     used
//   * reducer outputs:   used
//   * all other cases:   =1

enum class key_tag : uint8_t {
    table = 0x50,
    index = 0x60,
};

inline key_tag bin_to_key_tag(abieos::input_buffer& b) { return (key_tag)abieos::bin_to_native<uint8_t>(b); }

inline const char* to_string(key_tag t) {
    switch (t) {
    case key_tag::table: return "table";
    case key_tag::index: return "index";
    default: return "?";
    }
}

inline std::string key_to_string(abieos::input_buffer b) {
    using std::to_string;
    std::string result;
    auto        t0 = bin_to_key_tag(b);
    result += to_string(t0);
    // if (t0 == key_tag::block) {
    //     try {
    //         result += " " + to_string(bin_to_native_key<uint32_t>(b));
    //         auto t1 = bin_to_key_tag(b);
    //         result += " " + std::string{to_string(t1)};
    //         // if (t1 == key_tag::table_row) {
    //         //     auto table_name = bin_to_native_key<abieos::name>(b);
    //         //     result += " '" + (std::string)table_name + "' ";
    //         //     abieos::hex(b.pos, b.end, std::back_inserter(result));
    //         //     // } else if (t1 == key_tag::table_delta) {
    //         //     //     auto table_name = bin_to_native_key<abieos::name>(b);
    //         //     //     result += " '" + (std::string)table_name + "' present: " + (bin_to_native_key<bool>(b) ? "true" : "false") + " ";
    //         //     //     abieos::hex(b.pos, b.end, std::back_inserter(result));
    //         // } else {
    //         //     result += " ...";
    //         // }
    //     } catch (...) {
    //         return result + " (deserialize error)";
    //     }
    // } else {
    //     result += " ...";
    // }
    return result;
}

inline void append_table_key(std::vector<char>& dest) { native_to_bin_key(dest, (uint8_t)key_tag::table); }

inline void append_table_key(std::vector<char>& dest, uint32_t block) {
    native_to_bin_key(dest, (uint8_t)key_tag::table);
    native_to_bin_key(dest, block);
}

inline void append_table_key(std::vector<char>& dest, uint32_t block, bool present_k, abieos::name table_name) {
    native_to_bin_key(dest, (uint8_t)key_tag::table);
    native_to_bin_key(dest, block);
    native_to_bin_key(dest, table_name);
    native_to_bin_key(dest, present_k);
}

inline std::vector<char> make_table_key() {
    std::vector<char> result;
    append_table_key(result);
    return result;
}

inline std::vector<char> make_table_key(uint32_t block) {
    std::vector<char> result;
    append_table_key(result, block);
    return result;
}

inline std::vector<char> make_table_key(uint32_t block, bool present_k, abieos::name table_name) {
    std::vector<char> result;
    append_table_key(result, block, present_k, table_name);
    return result;
}

inline void append_index_key(std::vector<char>& dest) { native_to_bin_key(dest, (uint8_t)key_tag::index); }

inline void append_index_key(std::vector<char>& dest, abieos::name table_name, abieos::name index_name) {
    native_to_bin_key(dest, (uint8_t)key_tag::index);
    native_to_bin_key(dest, table_name);
    native_to_bin_key(dest, index_name);
}

inline std::vector<char> make_index_key() {
    std::vector<char> result;
    append_index_key(result);
    return result;
}

inline std::vector<char> make_index_key(abieos::name table_name, abieos::name index_name) {
    std::vector<char> result;
    append_index_key(result, table_name, index_name);
    return result;
}

inline void append_index_suffix(std::vector<char>& dest, uint32_t block) { native_to_bin_key(dest, ~block); }

inline void append_index_suffix(std::vector<char>& dest, uint32_t block, bool present_k) {
    native_to_bin_key(dest, ~block);
    native_to_bin_key(dest, !present_k);
}

inline void read_index_suffix(abieos::input_buffer& bin, uint32_t& block, bool& present_k) {
    block     = ~bin_to_native_key<uint32_t>(bin);
    present_k = !bin_to_native_key<bool>(bin);
}

inline std::vector<char> make_fill_status_key() { return make_table_key(0, true, "fill.status"_n); }

struct received_block {
    uint32_t            block_num = {};
    abieos::checksum256 block_id  = {};
};

ABIEOS_REFLECT(received_block) {
    ABIEOS_MEMBER(received_block, block_num)
    ABIEOS_MEMBER(received_block, block_id)
}

inline std::vector<char> make_received_block_key(uint32_t block) { return make_table_key(block, true, "recvd.block"_n); }
inline std::vector<char> make_block_info_key(uint32_t block) { return make_table_key(block, true, "block.info"_n); }

inline void append_transaction_trace_key(std::vector<char>& dest, uint32_t block, const abieos::checksum256 transaction_id) {
    append_table_key(dest, block, true, "ttrace"_n);
    native_to_bin_key(dest, transaction_id);
}

inline void
append_action_trace_key(std::vector<char>& dest, uint32_t block, const abieos::checksum256 transaction_id, uint32_t action_index) {
    append_table_key(dest, block, true, "atrace"_n);
    native_to_bin_key(dest, transaction_id);
    native_to_bin_key(dest, action_index);
}

struct defs {
    using type = kv::type;

    struct field : query_config::field<defs> {
        std::optional<uint32_t> byte_position = {};
    };

    using key = query_config::key<defs>;

    struct table : query_config::table<defs> {
        abieos::name short_name = {};
    };

    using query = query_config::query<defs>;

    struct config : query_config::config<defs> {
        template <typename M>
        void prepare(const M& type_map) {
            query_config::config<defs>::prepare(type_map);
            for (auto& tab : tables) {
                auto it = table_names.find(tab.name);
                if (it == table_names.end())
                    throw std::runtime_error("query_database: unknown table: " + tab.name);
                tab.short_name = it->second;
            }
        }
    };
}; // defs

using field  = defs::field;
using key    = defs::key;
using table  = defs::table;
using query  = defs::query;
using config = defs::config;

inline void clear_positions(std::vector<field>& fields) {
    for (auto& field : fields)
        field.byte_position.reset();
}

template <typename F>
void fill_positions_impl(const char* begin, abieos::input_buffer& src, bool is_key, F for_each_field) {
    bool present = true;
    for_each_field([&](auto& field) {
        if (present)
            field.byte_position = src.pos - begin;
        if (field.begin_optional) {
            present = abieos::bin_to_native<bool>(src);
        } else {
            if (present) {
                if (is_key) {
                    if (!field.type_obj->skip_key(src))
                        return false;
                } else {
                    if (!field.type_obj->skip_bin(src))
                        return false;
                }
            }
            if (field.end_optional)
                present = true;
        }
        return true;
    });
}

inline void fill_positions_rw(const char* begin, abieos::input_buffer& src, std::vector<field>& fields) {
    clear_positions(fields);
    fill_positions_impl(begin, src, false, [&](auto f) {
        for (auto& field : fields)
            if (!f(field))
                break;
    });
}

inline void fill_positions(abieos::input_buffer src, std::vector<field>& fields) { fill_positions_rw(src.pos, src, fields); }

inline void fill_positions_rw(const char* begin, abieos::input_buffer& src, std::vector<key>& keys) {
    fill_positions_impl(begin, src, true, [&](auto f) {
        for (auto& key : keys)
            if (!f(*key.field))
                break;
    });
}

inline void fill_positions(abieos::input_buffer src, std::vector<key>& fields) { fill_positions_rw(src.pos, src, fields); }

inline bool keys_have_positions(const std::vector<key>& keys) {
    for (auto& key : keys)
        if (!key.field->byte_position)
            return false;
    return true;
}

inline std::vector<char> extract_pk_from_key(abieos::input_buffer key, kv::table& table, std::vector<kv::key>& keys) {
    auto temp = key;
    skip_key<uint8_t>(temp);      // key_tag::index
    skip_key<abieos::name>(temp); // table_name
    skip_key<abieos::name>(temp); // index_name

    clear_positions(table.fields);
    fill_positions_rw(key.pos, temp, keys);

    uint32_t block;
    bool     present_k;
    read_index_suffix(temp, block, present_k);

    std::vector<char> result;
    append_table_key(result, block, present_k, table.short_name);
    for (auto& k : table.keys) {
        if (!k.field->byte_position)
            throw std::runtime_error("secondary index is missing pk fields");
        abieos::input_buffer b = {key.pos + *k.field->byte_position, key.end};
        k.field->type_obj->key_to_key(result, b);
    }
    return result;
}

} // namespace kv
} // namespace state_history

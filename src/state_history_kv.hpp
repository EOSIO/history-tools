// copyright defined in LICENSE.txt

#pragma once
#include "query_config.hpp"
#include "state_history.hpp"

namespace state_history {
namespace kv {

using namespace abieos::literals;

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
void native_to_key(std::vector<char>& bin, const T& obj) {
    fixup_key<T>(bin, [&] { abieos::native_to_bin(obj, bin); });
}

template <typename T>
T key_to_native(abieos::input_buffer& b) {
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
inline void bin_to_bin<transaction_status>(std::vector<char>& dest, abieos::input_buffer& bin) {
    return bin_to_bin<std::underlying_type_t<transaction_status>>(dest, bin);
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
        std::is_same_v<std::decay_t<T>, abieos::block_timestamp> || std::is_same_v<std::decay_t<T>, transaction_status>) {
        if (size_t(bin.end - bin.pos) < sizeof(T))
            throw std::runtime_error("skip past end");
        bin.pos += sizeof(T);
        return true;
    } else if constexpr (std::is_same_v<std::decay_t<T>, abieos::varuint32>) {
        uint32_t    dummy;
        std::string error;
        if (!abieos::read_varuint32(bin, error, dummy))
            throw std::runtime_error("skip past end");
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
        std::is_same_v<std::decay_t<T>, abieos::block_timestamp> || std::is_same_v<std::decay_t<T>, transaction_status>) {
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
    {"uint32?",                 make_type_for<std::optional<uint32_t>>()},
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
    {"transaction_status",      make_type_for<transaction_status>()},
};
// clang-format on

// Key                                                                          Value                               Notes   Description
// =================================================================================================================================================
// key_tag::table,  block_num, table_name, present_k, pk,               ## present_v,(fields iff present_v) ## 1,2  ## traces, deltas, reducer_outputs
// key_tag::index,  table_name, index_name, key, ~block_num, !present_k ## (none)                           ## 1    ## indexes. key is superset of pk fields
//
// * Keys are serialized in a lexigraphical sort format. See native_to_key() and key_to_native().
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
    //         result += " " + to_string(key_to_native<uint32_t>(b));
    //         auto t1 = bin_to_key_tag(b);
    //         result += " " + std::string{to_string(t1)};
    //         // if (t1 == key_tag::table_row) {
    //         //     auto table_name = key_to_native<abieos::name>(b);
    //         //     result += " '" + (std::string)table_name + "' ";
    //         //     abieos::hex(b.pos, b.end, std::back_inserter(result));
    //         //     // } else if (t1 == key_tag::table_delta) {
    //         //     //     auto table_name = key_to_native<abieos::name>(b);
    //         //     //     result += " '" + (std::string)table_name + "' present: " + (key_to_native<bool>(b) ? "true" : "false") + " ";
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

inline void append_table_key(std::vector<char>& dest) { native_to_key(dest, (uint8_t)key_tag::table); }

inline void append_table_key(std::vector<char>& dest, uint32_t block) {
    native_to_key(dest, (uint8_t)key_tag::table);
    native_to_key(dest, block);
}

inline void append_table_key(std::vector<char>& dest, uint32_t block, bool present_k, abieos::name table_name) {
    native_to_key(dest, (uint8_t)key_tag::table);
    native_to_key(dest, block);
    native_to_key(dest, table_name);
    native_to_key(dest, present_k);
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

inline void append_index_key(std::vector<char>& dest) { native_to_key(dest, (uint8_t)key_tag::index); }

inline void append_index_key(std::vector<char>& dest, abieos::name table_name, abieos::name index_name) {
    native_to_key(dest, (uint8_t)key_tag::index);
    native_to_key(dest, table_name);
    native_to_key(dest, index_name);
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

inline void read_table_prefix(abieos::input_buffer& bin, uint32_t& block_num, abieos::name& table_name, bool& present_k) {
    block_num  = key_to_native<uint32_t>(bin);
    table_name = key_to_native<abieos::name>(bin);
    present_k  = key_to_native<bool>(bin);
}

inline void append_index_suffix(std::vector<char>& dest, uint32_t block) { native_to_key(dest, ~block); }

inline void append_index_suffix(std::vector<char>& dest, uint32_t block, bool present_k) {
    native_to_key(dest, ~block);
    native_to_key(dest, !present_k);
}

inline void read_index_prefix(abieos::input_buffer& bin, abieos::name& table, abieos::name& index) {
    table = key_to_native<abieos::name>(bin);
    index = key_to_native<abieos::name>(bin);
}

inline void read_index_suffix(abieos::input_buffer& bin, uint32_t& block, bool& present_k) {
    block     = ~key_to_native<uint32_t>(bin);
    present_k = !key_to_native<bool>(bin);
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
    native_to_key(dest, transaction_id);
}

inline void
append_action_trace_key(std::vector<char>& dest, uint32_t block, const abieos::checksum256 transaction_id, uint32_t action_index) {
    append_table_key(dest, block, true, "atrace"_n);
    native_to_key(dest, transaction_id);
    native_to_key(dest, action_index);
}

struct defs {
    using type  = kv::type;
    using index = query_config::index<defs>;
    using query = query_config::query<defs>;

    struct field : query_config::field<defs> {
        uint32_t field_index = -1; // index within table::fields
    };

    using key   = query_config::key<defs>;
    using table = query_config::table<defs>;

    struct config : query_config::config<defs> {
        template <typename M>
        void prepare(const M& type_map) {
            query_config::config<defs>::prepare(type_map);
            for (auto& table : tables)
                for (uint32_t i = 0; i < table.fields.size(); ++i)
                    table.fields[i].field_index = i;
        }
    };
}; // defs

using field  = defs::field;
using key    = defs::key;
using table  = defs::table;
using index  = defs::index;
using query  = defs::query;
using config = defs::config;

inline void init_positions(std::vector<std::optional<uint32_t>>& positions, size_t size) {
    positions.clear();
    positions.resize(size);
}

template <typename F>
void fill_positions_impl(
    const char* begin, abieos::input_buffer& src, bool is_key, std::vector<std::optional<uint32_t>>& positions, F for_each_field) {
    bool present = true;
    for_each_field([&](auto& field) {
        if (present)
            positions.at(field.field_index) = src.pos - begin;
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

inline void fill_positions_rw(
    const char* begin, abieos::input_buffer& src, const std::vector<field>& fields, std::vector<std::optional<uint32_t>>& positions) {
    fill_positions_impl(begin, src, false, positions, [&](auto f) {
        for (auto& field : fields)
            if (!f(field))
                break;
    });
}

inline void fill_positions(abieos::input_buffer src, const std::vector<field>& fields, std::vector<std::optional<uint32_t>>& positions) {
    fill_positions_rw(src.pos, src, fields, positions);
}

inline void fill_positions_rw(
    const char* begin, abieos::input_buffer& src, const std::vector<key>& keys, std::vector<std::optional<uint32_t>>& positions) {
    fill_positions_impl(begin, src, true, positions, [&](auto f) {
        for (auto& key : keys)
            if (!f(*key.field))
                break;
    });
}

inline void fill_positions(abieos::input_buffer src, const std::vector<key>& fields, std::vector<std::optional<uint32_t>>& positions) {
    fill_positions_rw(src.pos, src, fields, positions);
}

inline bool keys_have_positions(const std::vector<key>& keys, std::vector<std::optional<uint32_t>>& positions) {
    for (auto& key : keys)
        if (!positions.at(key.field->field_index))
            return false;
    return true;
}

inline void extract_keys(
    std::vector<char>& dest, abieos::input_buffer value, const std::vector<kv::key>& keys,
    std::vector<std::optional<uint32_t>>& positions) {
    for (auto& k : keys) {
        if (!positions.at(k.field->field_index))
            throw std::runtime_error("key fields are missing");
        abieos::input_buffer b = {value.pos + *positions[k.field->field_index], value.end};
        k.field->type_obj->bin_to_key(dest, b);
    }
}

inline const char* fill_positions_from_index(
    abieos::input_buffer index, const std::vector<kv::key>& index_keys, uint32_t& block, bool& present_k,
    std::vector<std::optional<uint32_t>>& positions) {

    auto start = index.pos;
    skip_key<uint8_t>(index);      // key_tag::index
    skip_key<abieos::name>(index); // table_name
    skip_key<abieos::name>(index); // index_name

    fill_positions_rw(start, index, index_keys, positions);
    auto suffix_pos = index.pos;
    read_index_suffix(index, block, present_k);
    return suffix_pos;
}

inline std::vector<char> extract_pk(
    abieos::input_buffer index, const kv::table& table, uint32_t block, bool present_k, std::vector<std::optional<uint32_t>>& positions) {
    std::vector<char> result;
    append_table_key(result, block, present_k, table.short_name);
    for (auto& k : table.keys) {
        if (!positions.at(k.field->field_index))
            throw std::runtime_error("secondary index is missing pk fields");
        abieos::input_buffer b = {index.pos + *positions[k.field->field_index], index.end};
        k.field->type_obj->key_to_key(result, b);
    }
    return result;
}

inline std::vector<char> extract_pk_from_index(abieos::input_buffer index, const kv::table& table, const std::vector<kv::key>& index_keys) {
    std::vector<std::optional<uint32_t>> positions;
    init_positions(positions, table.fields.size());
    uint32_t block;
    bool     present_k;
    fill_positions_from_index(index, index_keys, block, present_k, positions);
    return extract_pk(index, table, block, present_k, positions);
}

} // namespace kv
} // namespace state_history

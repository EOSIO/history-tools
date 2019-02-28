// copyright defined in LICENSE.txt

#include <cwchar>

namespace std {
size_t wcslen(const wchar_t* str) { return ::wcslen(str); }
} // namespace std

#include "ex-chain.hpp"
#include "test-common.hpp"

#include <abieos.hpp>
#include <eosio/database.hpp>
#include <eosio/parse-json.hpp>

// todo: move
extern "C" void eosio_assert(uint32_t test, const char* msg) {
    if (!test)
        eosio_assert_message(test, msg, strlen(msg));
}

eosio::datastream<const char*> get_raw_abi(eosio::name name, uint32_t max_block) {
    eosio::datastream<const char*> result = {nullptr, 0};
    auto                           s      = exec_query(eosio::query_account_range_name{
        .max_block   = max_block,
        .first       = name,
        .last        = name,
        .max_results = 1,
    });
    eosio::for_each_query_result<eosio::account>(s, [&](eosio::account& a) {
        if (a.present)
            result = a.abi;
        return true;
    });
    return result;
}

struct abi {
    abieos::abi_def  def{};
    abieos::contract contract{};
};

std::unique_ptr<abi> get_abi(eosio::name name, uint32_t max_block) {
    auto result = std::make_unique<abi>();
    auto raw    = get_raw_abi(name, max_block);
    if (!raw.remaining())
        return {};
    std::string error;
    if (!abieos::check_abi_version(abieos::input_buffer{raw.pos(), raw.pos() + raw.remaining()}, error))
        return {};
    abieos::input_buffer buf{raw.pos(), raw.pos() + raw.remaining()};
    if (!abieos::bin_to_native(result->def, error, buf))
        return {};
    if (!fill_contract(result->contract, error, result->def))
        return {};
    return result;
}

abieos::abi_type* get_table_type(::abi* abi, abieos::name table) {
    if (!abi)
        return nullptr;
    for (auto& table_def : abi->def.tables) {
        if (table_def.name == table) {
            auto it = abi->contract.abi_types.find(table_def.type);
            if (it != abi->contract.abi_types.end())
                return &it->second;
        }
    }
    return nullptr;
}

struct get_table_rows_params {
    bool             json           = false;
    eosio::name      code           = {};
    std::string_view scope          = {};
    eosio::name      table          = {};
    std::string_view table_key      = {}; // todo
    std::string_view lower_bound    = {};
    std::string_view upper_bound    = {};
    uint32_t         limit          = 10;
    std::string_view key_type       = {};
    std::string_view index_position = {};
    std::string_view encode_type    = "dec"; // todo
    bool             reverse        = false; // todo
    bool             show_payer     = false;
};

STRUCT_REFLECT(get_table_rows_params) {
    STRUCT_MEMBER(get_table_rows_params, json);
    STRUCT_MEMBER(get_table_rows_params, code);
    STRUCT_MEMBER(get_table_rows_params, scope);
    STRUCT_MEMBER(get_table_rows_params, table);
    STRUCT_MEMBER(get_table_rows_params, table_key);
    STRUCT_MEMBER(get_table_rows_params, lower_bound);
    STRUCT_MEMBER(get_table_rows_params, upper_bound);
    STRUCT_MEMBER(get_table_rows_params, limit);
    STRUCT_MEMBER(get_table_rows_params, key_type);
    STRUCT_MEMBER(get_table_rows_params, index_position);
    STRUCT_MEMBER(get_table_rows_params, encode_type);
    STRUCT_MEMBER(get_table_rows_params, reverse);
    STRUCT_MEMBER(get_table_rows_params, show_payer);
}

template <int size>
bool starts_with(std::string_view s, const char (&prefix)[size]) {
    if (s.size() < size)
        return false;
    return !strncmp(s.begin(), prefix, size);
}

uint64_t guess_uint64(std::string_view str, std::string_view desc) {
    std::string error;
    uint64_t    result;

    if (abieos::decimal_to_binary(result, error, str))
        return result;

    while (!str.empty() && str.front() == ' ')
        str.remove_prefix(1);
    while (!str.empty() && str.back() == ' ')
        str.remove_suffix(1);

    if (abieos::string_to_name_strict(str, result))
        return result;
    if (str.find(',') != std::string_view::npos && abieos::string_to_symbol(result, error, str))
        return result;
    if (abieos::string_to_symbol_code(result, error, str))
        return result;

    eosio_assert(
        false, ("Could not convert " + std::string(desc) + " string '" + std::string(str) +
                "' to any of the following: uint64_t, name, symbol, or symbol_code")
                   .c_str());
    return 0;
}

uint64_t convert_key(std::string_view key_type, std::string_view key, uint64_t default_value) {
    if (key.empty())
        return default_value;
    if (key_type == "name")
        return eosio::name(key).value;
    std::string error;
    if (!abieos::decimal_to_binary(default_value, error, key))
        eosio_assert(false, ("Invalid key: " + std::string(key)).c_str());
    return default_value;
};

eosio::name get_table_index_name(const get_table_rows_params& p, bool& primary) {
    auto table = p.table;
    auto index = table.value & 0xFFFFFFFFFFFFFFF0ULL;
    if (index != table.value)
        eosio_assert(false, ("Unsupported table name: " + p.table.to_string()).c_str());

    primary      = false;
    uint64_t pos = 0;
    if (p.index_position.empty() || p.index_position == "first" || p.index_position == "primary" || p.index_position == "one") {
        primary = true;
    } else if (starts_with(p.index_position, "sec") || p.index_position == "two") {           // second, secondary
    } else if (starts_with(p.index_position, "ter") || starts_with(p.index_position, "th")) { // tertiary, ternary, third, three
        pos = 1;
    } else if (starts_with(p.index_position, "fou")) { // four, fourth
        pos = 2;
    } else if (starts_with(p.index_position, "fi")) { // five, fifth
        pos = 3;
    } else if (starts_with(p.index_position, "six")) { // six, sixth
        pos = 4;
    } else if (starts_with(p.index_position, "sev")) { // seven, seventh
        pos = 5;
    } else if (starts_with(p.index_position, "eig")) { // eight, eighth
        pos = 6;
    } else if (starts_with(p.index_position, "nin")) { // nine, ninth
        pos = 7;
    } else if (starts_with(p.index_position, "ten")) { // ten, tenth
        pos = 8;
    } else {
        std::string error;
        if (!abieos::decimal_to_binary(pos, error, p.index_position))
            eosio_assert(false, ("Invalid index_position: " + std::string(p.index_position)).c_str());
        if (pos < 2) {
            primary = true;
            pos     = 0;
        } else {
            pos -= 2;
        }
    }
    index |= (pos & 0x000000000000000FULL);
    return eosio::name{index};
} // get_table_index_name

void get_table_rows_primary(
    const get_table_rows_params& params, const eosio::context_data& context, uint64_t scope, abieos::abi_type* table_type) {

    auto lower_bound = convert_key(params.key_type, params.lower_bound, (uint64_t)0);
    auto upper_bound = convert_key(params.key_type, params.upper_bound, (uint64_t)0xffff'ffff'ffff'ffff);

    auto s = exec_query(eosio::query_contract_row_range_code_table_scope_pk{
        .max_block = context.head,
        .first =
            {
                .code        = params.code,
                .table       = params.table,
                .scope       = scope,
                .primary_key = lower_bound,
            },
        .last =
            {
                .code        = params.code,
                .table       = params.table,
                .scope       = scope,
                .primary_key = upper_bound,
            },
        .max_results = std::min((uint32_t)100, params.limit),
    });

    // todo: rope
    std::string result = "{\"rows\":[";
    bool        found  = false;
    eosio::for_each_query_result<eosio::contract_row>(s, [&](eosio::contract_row& r) {
        if (!r.present)
            return true;
        if (found)
            result += ',';
        found = true;
        if (params.show_payer)
            result += "{\"data\":";
        bool decoded = false;
        if (table_type) {
            abieos::input_buffer bin{r.value.pos(), r.value.pos() + r.value.remaining()};
            std::string          error;
            std::string          json_row;
            if (bin_to_json(bin, error, table_type, json_row)) {
                result += json_row;
                decoded = true;
            }
        }
        if (!decoded) {
            result += '"';
            abieos::hex(r.value.pos(), r.value.pos() + r.value.remaining(), std::back_inserter(result));
            result += '"';
        }
        if (params.show_payer)
            result += ",\"payer\":\"" + r.payer.to_string() + "\"}";
        return true;
    });
    result += "]}";
    set_output_data(result);
} // get_table_rows_primary

template <typename T>
void get_table_rows_secondary(
    const get_table_rows_params& params, const eosio::context_data& context, uint64_t scope, abieos::abi_type* table_type) {

    auto lower_bound = convert_key(params.key_type, params.lower_bound, (T)0);
    auto upper_bound = convert_key(params.key_type, params.upper_bound, (T)0xffff'ffff'ffff'ffff);

    auto s = exec_query(eosio::query_contract_index64_range_code_table_scope_sk_pk{
        .max_block = context.head,
        .first =
            {
                .code          = params.code,
                .table         = params.table,
                .scope         = scope,
                .secondary_key = lower_bound,
                .primary_key   = 0,
            },
        .last =
            {
                .code          = params.code,
                .table         = params.table,
                .scope         = scope,
                .secondary_key = upper_bound,
                .primary_key   = 0xffff'ffff'ffff'ffff,
            },
        .max_results = std::min((uint32_t)100, params.limit),
    });

    // todo: rope
    std::string result = "{\"rows\":[";
    bool        found  = false;
    eosio::for_each_query_result<eosio::contract_secondary_index_with_row<T>>(s, [&](eosio::contract_secondary_index_with_row<T>& r) {
        if (!r.present || !r.row_present)
            return true;
        if (found)
            result += ',';
        found = true;
        if (params.show_payer)
            result += "{\"data\":";
        bool decoded = false;
        if (table_type) {
            abieos::input_buffer bin{r.row_value.pos(), r.row_value.pos() + r.row_value.remaining()};
            std::string          error;
            std::string          json_row;
            if (bin_to_json(bin, error, table_type, json_row)) {
                result += json_row;
                decoded = true;
            }
        }
        if (!decoded) {
            result += '"';
            abieos::hex(r.row_value.pos(), r.row_value.pos() + r.row_value.remaining(), std::back_inserter(result));
            result += '"';
        }
        if (params.show_payer)
            result += ",\"payer\":\"" + r.payer.to_string() + "\"}";
        return true;
    });
    result += "]}";
    set_output_data(result);
} // get_table_rows_secondary

// todo: more
void get_table_rows(std::string_view request, const eosio::context_data& context) {
    auto                   params           = eosio::parse_json<get_table_rows_params>(request);
    bool                   primary          = false;
    auto                   table_with_index = get_table_index_name(params, primary);
    std::unique_ptr<::abi> abi              = params.json ? get_abi(params.code, context.head) : nullptr;
    auto                   table_type       = get_table_type(abi.get(), abieos::name{params.table.value});
    auto                   scope            = guess_uint64(params.scope, "scope");

    if (primary)
        get_table_rows_primary(params, context, scope, table_type);
    else if (params.key_type == "i64" || params.key_type == "name")
        get_table_rows_secondary<uint64_t>(params, context, scope, table_type);
    else
        eosio_assert(false, ("unsupported key_type: " + (std::string)params.key_type).c_str());
}

struct request_data {
    std::string_view target  = {nullptr, 0};
    std::string_view request = {nullptr, 0};
};

extern "C" void startup() {
    auto request = eosio::unpack<request_data>(get_input_data());
    auto context = eosio::get_context_data();
    print_range(request.target.begin(), request.target.end());
    eosio::print("\n");

    {
        auto        b = context.head_id.extract_as_byte_array();
        std::string s;
        abieos::hex(b.begin(), b.end(), std::back_inserter(s));
        eosio::print("head ", context.head, " ", s, "\n");
    }
    {
        auto        b = context.irreversible_id.extract_as_byte_array();
        std::string s;
        abieos::hex(b.begin(), b.end(), std::back_inserter(s));
        eosio::print("irreversible ", context.irreversible, " ", s, "\n");
    }
    eosio::print("first ", context.first, "\n");

    if (request.target == "/v1/chain/get_table_rows")
        get_table_rows(request.request, context);
    else
        eosio_assert(false, "not found");
}

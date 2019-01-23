// copyright defined in LICENSE.txt

#include <cwchar>

namespace std {
size_t wcslen(const wchar_t* str) { return ::wcslen(str); }
} // namespace std

#include <abieos.hpp>

#include "ex-chain.hpp"
#include "lib-database.hpp"
#include "lib-parse-json.hpp"
#include "test-common.hpp"

// todo: move
extern "C" void eosio_assert(uint32_t test, const char* msg) {
    if (!test)
        eosio_assert_message(test, msg, strlen(msg));
}

eosio::datastream<const char*> get_raw_abi(eosio::name name, uint32_t max_block) {
    eosio::datastream<const char*> result = {nullptr, 0};
    auto                           s      = exec_query(query_account_range_name{
        .max_block   = max_block,
        .first       = name,
        .last        = name,
        .max_results = 1,
    });
    for_each_query_result<account>(s, [&](account& a) {
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
    bool json  = false;
    name code  = {};
    name scope = {}; // todo: check this type
    name table = {};
    // std::string_view table_key      = {};
    std::string_view lower_bound    = {};
    std::string_view upper_bound    = {};
    uint32_t         limit          = 10;
    std::string_view key_type       = {};
    std::string_view index_position = {};
    // std::string_view encode_type    = "dec";
    // bool             reverse        = false;
    bool show_payer = false;
};

template <typename F>
void for_each_member(get_table_rows_params& obj, F f) {
    f("json", obj.json);
    f("code", obj.code);
    f("scope", obj.scope);
    f("table", obj.table);
    // f("table_key", obj.table_key);
    f("lower_bound", obj.lower_bound);
    f("upper_bound", obj.upper_bound);
    f("limit", obj.limit);
    f("key_type", obj.key_type);
    f("index_position", obj.index_position);
    // f("encode_type", obj.encode_type);
    // f("reverse", obj.reverse);
    f("show_payer", obj.show_payer);
}

template <int size>
bool starts_with(std::string_view s, const char (&prefix)[size]) {
    if (s.size() < size)
        return false;
    return !strncmp(s.begin(), prefix, size);
}

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

// todo: more
void get_table_rows(std::string_view request, const context_data& context) {
    auto                   params           = parse_json<get_table_rows_params>(request);
    bool                   primary          = false;
    auto                   table_with_index = get_table_index_name(params, primary);
    std::unique_ptr<::abi> abi              = params.json ? get_abi(params.code, context.head) : nullptr;
    auto                   table_type       = get_table_type(abi.get(), abieos::name{params.table.value});
    uint64_t               lower_bound      = 0;
    uint64_t               upper_bound      = 0xffff'ffff'ffff'ffff;

    eosio_assert(primary, "secondary not yet implemented");

    auto convert_key = [&](std::string_view key, uint64_t& dest) {
        if (key.empty())
            return;
        if (params.key_type == "name")
            dest = eosio::name(key).value;
        else {
            std::string error;
            if (!abieos::decimal_to_binary(dest, error, key))
                eosio_assert(false, ("Invalid key: " + std::string(key)).c_str());
        }
    };
    convert_key(params.upper_bound, upper_bound);
    convert_key(params.lower_bound, lower_bound);

    auto s = exec_query(query_contract_row_range_code_table_scope_pk{
        .max_block = context.head,
        .first =
            {
                .code        = params.code,
                .table       = params.table,
                .scope       = params.scope.value,
                .primary_key = lower_bound,
            },
        .last =
            {
                .code        = params.code,
                .table       = params.table,
                .scope       = params.scope.value,
                .primary_key = upper_bound,
            },
        .max_results = std::min((uint32_t)100, params.limit),
    });

    // todo: rope
    std::string result = "{\"rows\":[";
    bool        found  = false;
    for_each_query_result<contract_row>(s, [&](contract_row& r) {
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
}

struct request_data {
    std::string_view target  = {nullptr, 0};
    std::string_view request = {nullptr, 0};
};

extern "C" void startup() {
    auto request = unpack<request_data>(get_input_data());
    print_range(request.target.begin(), request.target.end());
    print("\n");
    if (request.target == "/v1/chain/get_table_rows")
        get_table_rows(request.request, get_context_data());
    else
        eosio_assert(false, "not found");
}

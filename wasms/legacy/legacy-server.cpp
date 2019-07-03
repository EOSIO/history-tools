// copyright defined in LICENSE.txt

#include <cwchar>

namespace std {
size_t wcslen(const wchar_t* str) { return ::wcslen(str); }
} // namespace std


#include <abieos.hpp>
#include <eosio/database.hpp>
#include <eosio/input_output.hpp>
#include <eosio/parse_json.hpp>

namespace eosio {

eosio::rope to_json(const eosio::public_key& value) {
    std::string e, s;
    (void)abieos::key_to_string(s, e, value, "", "EOS");
    return s.c_str();
}

eosio::rope to_json(const eosio::signature& value) {
    std::string e, s;
    (void)abieos::signature_to_string(s, e, reinterpret_cast<const abieos::signature&>(value));
    return s.c_str();
}

eosio::checksum256 checksum256_max() {
    std::array<eosio::checksum256::word_t, eosio::checksum256::num_words()> a{};
    a.fill(std::numeric_limits<eosio::checksum256::word_t>::max());
    return a;
}

} // namespace eosio

eosio::datastream<const char*> get_raw_abi(eosio::name name, uint32_t max_block) {
    eosio::datastream<const char*> result = {nullptr, 0};
    auto                           s      = query_database(eosio::query_account_range_name{
        .max_block   = max_block,
        .first       = name,
        .last        = name,
        .max_results = 1,
    });
    eosio::for_each_query_result<eosio::account>(s, [&](eosio::account& a) {
        if (a.present)
            result = *a.abi;
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

struct get_code_result {
    get_code_result(eosio::account a)
    : account_name(a.name)
    // , code_hash(a.code_version) TODO
    // , wasm(a.code)
    , abi(a.abi)
    {}
    eosio::name account_name;
    eosio::checksum256 code_hash;
    eosio::shared_memory<eosio::datastream<const char*>> wasm;
    eosio::shared_memory<eosio::datastream<const char*>> abi;
};

/// \exclude
STRUCT_REFLECT(get_code_result) {
    STRUCT_MEMBER(get_code_result, account_name)
    STRUCT_MEMBER(get_code_result, code_hash)
    STRUCT_MEMBER(get_code_result, wasm)
    STRUCT_MEMBER(get_code_result, abi)
}

struct get_abi_result {
    get_abi_result(eosio::account a)
    : account_name(a.name)
    , abi(a.abi)
    {}
    eosio::name account_name;
    eosio::shared_memory<eosio::datastream<const char*>> abi;
};

/// \exclude
STRUCT_REFLECT(get_abi_result) {
    STRUCT_MEMBER(get_abi_result, account_name)
    STRUCT_MEMBER(get_abi_result, abi)
}

struct get_table_rows_params {
    bool                                   json           = false;
    eosio::name                            code           = {};
    eosio::shared_memory<std::string_view> scope          = {};
    eosio::name                            table          = {};
    eosio::shared_memory<std::string_view> table_key      = {}; // todo
    eosio::shared_memory<std::string_view> lower_bound    = {};
    eosio::shared_memory<std::string_view> upper_bound    = {};
    uint32_t                               limit          = 10;
    eosio::shared_memory<std::string_view> key_type       = {};
    eosio::shared_memory<std::string_view> index_position = {};
    eosio::shared_memory<std::string_view> encode_type    = {"dec"}; // todo
    bool                                   reverse        = false;   // todo
    bool                                   show_payer     = false;
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

struct get_transaction_params {
    eosio::checksum256 id = {};
};

STRUCT_REFLECT(get_transaction_params) {
    STRUCT_MEMBER(get_transaction_params, id);
}

struct get_actions_params {
    eosio::name account_name = {};
    int pos = {};
    int offset = {};
};

STRUCT_REFLECT(get_actions_params) {
    STRUCT_MEMBER(get_actions_params, account_name);
    STRUCT_MEMBER(get_actions_params, pos);
    STRUCT_MEMBER(get_actions_params, offset);
}

struct get_block_params {
    eosio::shared_memory<std::string_view> block_num_or_id = {};
};

STRUCT_REFLECT(get_block_params) {
    STRUCT_MEMBER(get_block_params, block_num_or_id);
}

struct get_account_params {
    eosio::name account_name = {};
};

STRUCT_REFLECT(get_account_params) {
    STRUCT_MEMBER(get_account_params, account_name);
}

struct get_currency_balance_params {
    eosio::name account = {};
    eosio::name code = {};
    eosio::symbol_code symbol = {};
};

STRUCT_REFLECT(get_currency_balance_params) {
    STRUCT_MEMBER(get_currency_balance_params, account);
    STRUCT_MEMBER(get_currency_balance_params, code);
    STRUCT_MEMBER(get_currency_balance_params, symbol);
}

struct account {
    eosio::asset balance = {};
};

STRUCT_REFLECT(account) {
    STRUCT_MEMBER(account, balance);
}

struct producer_info {
   eosio::name           owner;
   double                total_votes;
   eosio::public_key     producer_key; /// a packed public key object
   bool                  is_active = true;
   std::string           url;
   uint32_t              unpaid_blocks;
   eosio::time_point     last_claim_time;
   uint16_t              location;

   uint64_t primary_key()const { return owner.value;                             }
   double   by_votes()const    { return is_active ? -total_votes : total_votes;  }
   bool     active()const      { return is_active;                               }
   void     deactivate()       { producer_key = eosio::public_key(); is_active = false; }

};

STRUCT_REFLECT(producer_info) {
    STRUCT_MEMBER(producer_info, owner);
    STRUCT_MEMBER(producer_info, total_votes);
    STRUCT_MEMBER(producer_info, producer_key);
    STRUCT_MEMBER(producer_info, is_active);
    STRUCT_MEMBER(producer_info, url);
    STRUCT_MEMBER(producer_info, unpaid_blocks);
    STRUCT_MEMBER(producer_info, last_claim_time);
    STRUCT_MEMBER(producer_info, location);
}

struct get_producer_schedule_result {
    std::vector<producer_info> rows;
    double total_producer_weight = 0.0;
};

/// \exclude
STRUCT_REFLECT(get_producer_schedule_result) {
    STRUCT_MEMBER(get_producer_schedule_result, rows);
    STRUCT_MEMBER(get_producer_schedule_result, total_producer_weight);
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

    eosio::check(
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
        eosio::check(false, ("Invalid key: " + std::string(key)).c_str());
    return default_value;
};

eosio::name get_table_index_name(const get_table_rows_params& p, bool& primary) {
    auto table = p.table;
    auto index = table.value & 0xFFFFFFFFFFFFFFF0ULL;
    if (index != table.value)
        eosio::check(false, ("Unsupported table name: " + p.table.to_string()).c_str());

    primary      = false;
    uint64_t pos = 0;
    if (p.index_position->empty() || *p.index_position == "first" || *p.index_position == "primary" || *p.index_position == "one") {
        primary = true;
    } else if (starts_with(*p.index_position, "sec") || *p.index_position == "two") {           // second, secondary
    } else if (starts_with(*p.index_position, "ter") || starts_with(*p.index_position, "th")) { // tertiary, ternary, third, three
        pos = 1;
    } else if (starts_with(*p.index_position, "fou")) { // four, fourth
        pos = 2;
    } else if (starts_with(*p.index_position, "fi")) { // five, fifth
        pos = 3;
    } else if (starts_with(*p.index_position, "six")) { // six, sixth
        pos = 4;
    } else if (starts_with(*p.index_position, "sev")) { // seven, seventh
        pos = 5;
    } else if (starts_with(*p.index_position, "eig")) { // eight, eighth
        pos = 6;
    } else if (starts_with(*p.index_position, "nin")) { // nine, ninth
        pos = 7;
    } else if (starts_with(*p.index_position, "ten")) { // ten, tenth
        pos = 8;
    } else {
        std::string error;
        if (!abieos::decimal_to_binary(pos, error, *p.index_position))
            eosio::check(false, ("Invalid index_position: " + std::string(*p.index_position)).c_str());
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
    const get_table_rows_params& params, const eosio::database_status& status, uint64_t scope, abieos::abi_type* table_type) {

    auto lower_bound = convert_key(*params.key_type, *params.lower_bound, (uint64_t)0);
    auto upper_bound = convert_key(*params.key_type, *params.upper_bound, (uint64_t)0xffff'ffff'ffff'ffff);

    auto s = query_database(eosio::query_contract_row_range_code_table_scope_pk{
        .max_block = status.head,
        .first =
            {
                .code        = params.code,
                .table       = params.table,
                .scope       = eosio::name{scope},
                .primary_key = lower_bound,
            },
        .last =
            {
                .code        = params.code,
                .table       = params.table,
                .scope       = eosio::name{scope},
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
            abieos::input_buffer bin{r.value->pos(), r.value->pos() + r.value->remaining()};
            std::string          error;
            std::string          json_row;
            if (bin_to_json(bin, error, table_type, json_row)) {
                result += json_row;
                decoded = true;
            }
        }
        if (!decoded) {
            result += '"';
            abieos::hex(r.value->pos(), r.value->pos() + r.value->remaining(), std::back_inserter(result));
            result += '"';
        }
        if (params.show_payer)
            result += ",\"payer\":\"" + r.payer.to_string() + "\"}";
        return true;
    });
    result += "]}";
    eosio::set_output_data(result);
} // get_table_rows_primary

template <typename T>
void get_table_rows_secondary(
    const get_table_rows_params& params, const eosio::database_status& status, uint64_t scope, abieos::abi_type* table_type) {

    auto lower_bound = convert_key(*params.key_type, *params.lower_bound, (T)0);
    auto upper_bound = convert_key(*params.key_type, *params.upper_bound, (T)0xffff'ffff'ffff'ffff);

    auto s = query_database(eosio::query_contract_index64_range_code_table_scope_sk_pk{
        .max_block = status.head,
        .first =
            {
                .code          = params.code,
                .table         = params.table,
                .scope         = eosio::name{scope},
                .secondary_key = lower_bound,
                .primary_key   = 0,
            },
        .last =
            {
                .code          = params.code,
                .table         = params.table,
                .scope         = eosio::name{scope},
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
            abieos::input_buffer bin{r.row_value->pos(), r.row_value->pos() + r.row_value->remaining()};
            std::string          error;
            std::string          json_row;
            if (bin_to_json(bin, error, table_type, json_row)) {
                result += json_row;
                decoded = true;
            }
        }
        if (!decoded) {
            result += '"';
            abieos::hex(r.row_value->pos(), r.row_value->pos() + r.row_value->remaining(), std::back_inserter(result));
            result += '"';
        }
        if (params.show_payer)
            result += ",\"payer\":\"" + r.payer.to_string() + "\"}";
        return true;
    });
    result += "]}";
    eosio::set_output_data(result);
} // get_table_rows_secondary

void get_table_rows(std::string_view request, const eosio::database_status& status) {
    auto                   params           = eosio::parse_json<get_table_rows_params>(request);
    bool                   primary          = false;
    auto                   table_with_index = get_table_index_name(params, primary);
    std::unique_ptr<::abi> abi              = params.json ? get_abi(params.code, status.head) : nullptr;
    auto                   table_type       = get_table_type(abi.get(), abieos::name{params.table.value});
    auto                   scope            = guess_uint64(*params.scope, "scope");

    if (primary)
        get_table_rows_primary(params, status, scope, table_type);
    else if (*params.key_type == "i64" || *params.key_type == "name")
        get_table_rows_secondary<uint64_t>(params, status, scope, table_type);
    else
        eosio::check(false, ("unsupported key_type: " + (std::string)(*params.key_type)).c_str());
}

void get_producer_schedule(std::string_view /*request*/, const eosio::database_status& status) {
    get_table_rows_params params {
        .code = "eosio"_n,
        .table = "producers"_n,
        .scope = "eosio",
        .key_type = "i64",
        .json = true,
        .limit = 10,
    };
    auto scope            = guess_uint64(*params.scope, "scope");

    auto lower_bound = convert_key(*params.key_type, *params.lower_bound, (uint64_t)0);
    auto upper_bound = convert_key(*params.key_type, *params.upper_bound, (uint64_t)0xffff'ffff'ffff'ffff);

    auto s = query_database(eosio::query_contract_row_range_code_table_scope_pk{
        .max_block = status.head,
        .first =
            {
                .code        = params.code,
                .table       = params.table,
                .scope       = eosio::name{scope},
                //.primary_key = lower_bound,
                .primary_key = 0,
            },
        .last =
            {
                .code        = params.code,
                .table       = params.table,
                .scope       = eosio::name{scope},
                //.primary_key = upper_bound,
                .primary_key = 0xffff'ffff'ffff'ffff,
            },
        .max_results = std::min((uint32_t)100, params.limit),
    });

    size_t count = 0;
    get_producer_schedule_result producers;
    std::string result;
    eosio::for_each_contract_row<producer_info>(s, [&](eosio::contract_row& r, producer_info* p) {
        if(++count > params.limit)
            return true;
        producers.rows.push_back(*p);
        producers.total_producer_weight += p->total_votes;
        return true;
    });
    result += eosio::to_json(producers).sv();
    eosio::print(result);
    eosio::set_output_data(result);
}

void get_currency_balance(std::string_view request, const eosio::database_status& status) {
    auto user_params = eosio::parse_json<get_currency_balance_params>(request);
    get_table_rows_params params {
        .code = user_params.code,
        .table = "accounts"_n,
        .key_type = "i64",
        .json = true,
        .limit = 10,
    };
    bool primary = false;
    auto                   table_with_index = get_table_index_name(params, primary);
    std::unique_ptr<::abi> abi              = params.json ? get_abi(params.code, status.head) : nullptr;
    auto                   table_type       = get_table_type(abi.get(), abieos::name{params.table.value});

    if(!primary)
        eosio::check(false, ("accounts table missing or missing primary index"));

    auto lower_bound = convert_key(*params.key_type, *params.lower_bound, (uint64_t)0);
    auto upper_bound = convert_key(*params.key_type, *params.upper_bound, (uint64_t)0xffff'ffff'ffff'ffff);

    auto s = query_database(eosio::query_contract_row_range_code_table_scope_pk{
        .max_block = status.head,
        .first =
            {
                .code = params.code,
                .table = params.table,
                .scope = user_params.account,
                .primary_key = user_params.symbol.raw(),
            },
        .last =
            {
                .code = params.code,
                .table = params.table,
                .scope = user_params.account,
                .primary_key = user_params.symbol.raw(),
            },
        .max_results = std::min((uint32_t)100, params.limit),
    });

    std::vector<eosio::asset> balances;
    std::string result;
    eosio::for_each_contract_row<account>(s, [&](eosio::contract_row& /*r*/, account* a) {
        balances.emplace_back(a->balance);
        return true;
    });
    result += eosio::to_json(balances).sv();
    eosio::set_output_data(result);
}

void get_transaction(std::string_view request, const eosio::database_status& /*status*/) {
    auto params = eosio::parse_json<get_transaction_params>(request);

    auto s = query_database(eosio::query_transaction_receipt {
        .max_block = std::numeric_limits<uint32_t>::max(),
        .first =
            {
                .transaction_id = params.id,
                .block_num = std::numeric_limits<uint32_t>::min(),
                .action_ordinal = std::numeric_limits<uint32_t>::min(),
            },
        .last =
            {
                .transaction_id = params.id,
                .block_num = std::numeric_limits<uint32_t>::max(),
                .action_ordinal = std::numeric_limits<uint32_t>::max(),
            },
        .max_results = 1,
    });

    std::string result;
    eosio::for_each_query_result<eosio::action_trace>(s, [&](eosio::action_trace& r) {
        result += to_json(r).sv();
        return true;
    });
    eosio::set_output_data(result);
}

void get_actions(std::string_view request, const eosio::database_status& /*status*/) {
    auto params = eosio::parse_json<get_actions_params>(request);
    eosio::print(params.account_name);
    auto s = query_database(eosio::query_action_trace_receipt_receiver {
        .max_block = std::numeric_limits<uint32_t>::max(),
        .first =
            {
                .receipt_receiver = params.account_name,
                .block_num = std::numeric_limits<uint32_t>::min(),
                .transaction_id = {},
                .action_ordinal = std::numeric_limits<uint32_t>::min(),
            },
        .last =
            {
                .receipt_receiver = params.account_name,
                .block_num = std::numeric_limits<uint32_t>::max(),
                .transaction_id = eosio::checksum256_max(),
                .action_ordinal = std::numeric_limits<uint32_t>::max(),
            },
        .max_results = uint32_t(std::abs(params.offset)),
    });

    std::vector<eosio::action_trace> actions;
    std::string result;
    eosio::for_each_query_result<eosio::action_trace>(s, [&](eosio::action_trace& r) {
        actions.emplace_back(r);
        return true;
    });
    result += eosio::to_json(actions).sv();
    eosio::print(result);
    eosio::set_output_data(result);
}

void get_block(std::string_view request, const eosio::database_status& /*status*/) {
    auto params = eosio::parse_json<get_block_params>(request);
    std::string error;
    uint32_t    block_num_or_id;

    if (!abieos::decimal_to_binary(block_num_or_id, error, *params.block_num_or_id))
        return;

    auto s = query_database(eosio::query_block_info_range_index{
        .first = block_num_or_id,
        .last = block_num_or_id,
        .max_results = 1,
    });

    std::string result;
    eosio::for_each_query_result<eosio::block_info>(s, [&](eosio::block_info& r) {
        result += to_json(r).sv();
        return true;
    });
    eosio::set_output_data(result);
}

void get_account(std::string_view request, const eosio::database_status& /*status*/) {
    auto params = eosio::parse_json<get_account_params>(request);

    auto s = query_database(eosio::query_account_range_name{
        .max_block = std::numeric_limits<uint32_t>::max(),
        .first = params.account_name,
        .last = params.account_name,
        .max_results = 1,
    });

    std::string result;
    eosio::for_each_query_result<eosio::account>(s, [&](eosio::account& r) {
        result += to_json(r).sv();
        return true;
    });
    eosio::set_output_data(result);
}

void get_code(std::string_view request, const eosio::database_status& /*status*/) {
    auto params = eosio::parse_json<get_account_params>(request);

    auto s = query_database(eosio::query_account_range_name{
        .max_block = std::numeric_limits<uint32_t>::max(),
        .first = params.account_name,
        .last = params.account_name,
        .max_results = 1,
    });

    std::string result;
    eosio::for_each_query_result<eosio::account>(s, [&](eosio::account& r) {
        get_code_result gcr(r);
        result += eosio::to_json(gcr).sv();
        return true;
    });
    eosio::set_output_data(result);
}

void get_abi(std::string_view request, const eosio::database_status& /*status*/) {
    auto params = eosio::parse_json<get_account_params>(request);

    auto s = query_database(eosio::query_account_range_name{
        .max_block = std::numeric_limits<uint32_t>::max(),
        .first = params.account_name,
        .last = params.account_name,
        .max_results = 1,
    });

    std::string result;
    eosio::for_each_query_result<eosio::account>(s, [&](eosio::account& r) {
        get_abi_result gar(r);
        result += eosio::to_json(gar).sv();
        return true;
    });
    eosio::set_output_data(result);
}
struct request_data {
    eosio::shared_memory<std::string_view> target  = {};
    eosio::shared_memory<std::string_view> request = {};
};

extern "C" __attribute__((eosio_wasm_entry)) void initialize() {}

extern "C" void run_query() {
    auto request = eosio::unpack<request_data>(eosio::get_input_data());
    auto status  = eosio::get_database_status();
    if (*request.target == "/v1/chain/get_table_rows")
        get_table_rows(*request.request, status);
    else if (*request.target == "/v1/history/get_transaction")
        get_transaction(*request.request, status);
    else if (*request.target == "/v1/history/get_actions")
        get_actions(*request.request, status);
    else if (*request.target == "/v1/chain/get_block")
        get_block(*request.request, status);
    else if (*request.target == "/v1/chain/get_account")
        get_account(*request.request, status);
    else if (*request.target == "/v1/chain/get_code")
        get_code(*request.request, status);
    else if (*request.target == "/v1/chain/get_abi")
        get_abi(*request.request, status);
    else if (*request.target == "/v1/chain/get_producer_schedule")
        get_producer_schedule(*request.request, status);
    else if (*request.target == "/v1/chain/get_currency_balance")
        get_currency_balance(*request.request, status);
    else
        eosio::check(false, "not found");
}

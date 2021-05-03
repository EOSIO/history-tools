#define BOOST_TEST_MODULE ship_sql
#include "test_protocol.hpp"
#include <boost/test/included/unit_test.hpp>

namespace state_history {
namespace pg {
std::string        sql_str(test_protocol::transaction_status v) { return to_string(v); }
std::string        sql_str(const test_protocol::recurse_transaction_trace& v);
} // namespace pg
} // namespace state_history

#include <abieos_sql_converter.hpp>

namespace test_protocol {
    constexpr const char* get_type_name(transaction_status*) { return "transaction_status"; }
}

namespace eosio {
template <>
inline constexpr bool is_basic_abi_type<input_stream> = true;
template <>
constexpr bool is_basic_abi_type<test_protocol::transaction_status> = true;

template <>
inline abi_type* add_type(abi& a, test_protocol::transaction_status*) {
    return std::addressof(a.abi_types.try_emplace("transaction_status", "transaction_status", abi_type::builtin{}, nullptr).first->second);
}

inline abi_type* add_type(abi& a, std::vector<test_protocol::recurse_transaction_trace>*) {
    abi_type& element_type =
        a.abi_types.try_emplace("recurse_transaction_trace", "recurse_transaction_trace", abi_type::builtin{}, nullptr).first->second;
    std::string name      = "recurse_transaction_trace?";
    auto [iter, inserted] = a.abi_types.try_emplace(name, name, abi_type::optional{&element_type}, optional_abi_serializer);
    return &iter->second;
}
} // namespace eosio


bool operator==(const abieos_sql_converter::field_def& lhs, const abieos_sql_converter::field_def& rhs) {
    return lhs.name == rhs.name && lhs.type == rhs.type;
}

std::ostream& operator<<(std::ostream& os, const abieos_sql_converter::field_def& f) {
    return os << "{ " << f.name << " , " << f.type << "}";
}

namespace state_history {
namespace pg {
std::string sql_str(const test_protocol::recurse_transaction_trace& v) {
    return sql_str(std::visit([](auto& x) { return x.id; }, v.recurse));
}
template<> inline constexpr type_names names_for<test_protocol::transaction_status>        = type_names{"transaction_status","transaction_status_type"};
template<> inline constexpr type_names names_for<test_protocol::recurse_transaction_trace> = type_names{"recurse_transaction_trace","varchar"};

} // namespace pg
} // namespace state_history

struct test_fixture_t {
    eosio::abi abi;
    abieos_sql_converter converter;

    test_fixture_t() {
        eosio::abi_def empty_def;
        eosio::convert(empty_def, abi);
        converter.schema_name     = R"("test")";

        eosio::add_type(abi, (std::vector<test_protocol::recurse_transaction_trace>*)nullptr);

        using basic_types = std::tuple<
            bool, uint8_t, int8_t, uint16_t, int16_t, uint32_t, int32_t, uint64_t, int64_t, double, std::string, unsigned __int128,
            __int128, eosio::float128, eosio::varuint32, eosio::varint32, eosio::name, eosio::checksum256, eosio::time_point,
            eosio::time_point_sec, eosio::block_timestamp, eosio::public_key, eosio::signature, eosio::bytes, eosio::symbol,
            test_protocol::transaction_status, test_protocol::recurse_transaction_trace>;

        converter.register_basic_types<basic_types>();
    }

    template <typename T> 
    abieos_sql_converter::union_fields_t
    get_union_fields() {
        std::string type_name = get_type_name((T*)nullptr);
        return abieos_sql_converter::union_fields_t(converter.schema_name, *abi.get_type(type_name)->as_variant(), converter.basic_converters);
    }
};

namespace tt = boost::test_tools;

BOOST_TEST_SPECIALIZED_COLLECTION_COMPARE(std::vector<abieos_sql_converter::field_def>)
BOOST_TEST_SPECIALIZED_COLLECTION_COMPARE(std::vector<std::string>)

BOOST_AUTO_TEST_SUITE(ship_sql_test_suite)

BOOST_FIXTURE_TEST_CASE(nested_varaint_test, test_fixture_t) {
    
    using namespace std::string_literals;

    abi.add_type<test_protocol::global_property>();
    abi.add_type<test_protocol::action_trace>();

    {
        auto result   = get_union_fields<test_protocol::chain_config>();
        auto expected = std::vector<abieos_sql_converter::field_def>{{"max_block_net_usage", "decimal"},
                                                                     {"target_block_net_usage_pct", "bigint"},
                                                                     {"max_transaction_net_usage", "bigint"},
                                                                     {"base_per_transaction_net_usage", "bigint"},
                                                                     {"net_usage_leeway", "bigint"},
                                                                     {"context_free_discount_net_usage_num", "bigint"},
                                                                     {"context_free_discount_net_usage_den", "bigint"},
                                                                     {"max_block_cpu_usage", "bigint"},
                                                                     {"target_block_cpu_usage_pct", "bigint"},
                                                                     {"max_transaction_cpu_usage", "bigint"},
                                                                     {"min_transaction_cpu_usage", "bigint"},
                                                                     {"max_transaction_lifetime", "bigint"},
                                                                     {"deferred_trx_expiration_window", "bigint"},
                                                                     {"max_transaction_delay", "bigint"},
                                                                     {"max_inline_action_size", "bigint"},
                                                                     {"max_inline_action_depth", "integer"},
                                                                     {"max_authority_depth", "integer"},
                                                                     {"max_action_return_value_size", "bigint"}};
        BOOST_TEST(result == expected);
    }
    {
        auto result   = get_union_fields<test_protocol::global_property>();
        auto expected = std::vector<abieos_sql_converter::field_def>{
            {"proposed_schedule_block_num", "bigint"},
            {"global_property_v0_proposed_schedule", "\"test\".producer_schedule"},
            {"proposed_schedule", "\"test\".producer_authority_schedule"},
            {"configuration", "\"test\"."s + get_type_name((test_protocol::chain_config*)nullptr)},
            {"chain_id", "varchar(64)"},
            {"kv_configuration", "\"test\".kv_database_config"},
            {"wasm_configuration", "\"test\".wasm_config"},
            {"extension", "\"test\".variant_global_property_extension_v0"}};

        BOOST_TEST(result == expected);
    }
    {

        auto result = get_union_fields<test_protocol::block_signing_authority>();

        auto expected = std::vector<abieos_sql_converter::field_def>{{"threshold", "bigint"}, {"keys", "\"test\".key_weight[]"}};
        BOOST_TEST(result == expected);
    }
    {
        auto result = get_union_fields<test_protocol::action_trace>();
        auto expected =
            std::vector<abieos_sql_converter::field_def>{{"action_ordinal", "bigint"},
                                                         {"creator_action_ordinal", "bigint"},
                                                         {"receipt", "\"test\"."s + get_type_name((test_protocol::action_receipt*)nullptr)},
                                                         {"receiver", "varchar(13)"},
                                                         {"act", "\"test\".action"},
                                                         {"context_free", "bool"},
                                                         {"elapsed", "bigint"},
                                                         {"console", "varchar"},
                                                         {"account_ram_deltas", "\"test\".account_delta[]"},
                                                         {"account_disk_deltas", "\"test\".account_delta[]"},
                                                         {"except", "varchar"},
                                                         {"error_code", "decimal"},
                                                         {"return_value", "bytea"}};

        BOOST_TEST(result == expected);
    }
}

BOOST_FIXTURE_TEST_CASE(create_global_property_table_test, test_fixture_t) {
    std::vector<std::string> statements;
    auto  exec                = [&statements](std::string stmt) { statements.push_back(stmt); };
    auto& global_property_abi = *abi.add_type<test_protocol::global_property>();

    converter.create_table("global_property", global_property_abi, "block_num bigint", {"block_num"}, exec);

    std::vector<std::string> expected = {
        R"xxx(create type "test"."producer_key" as ("producer_name" varchar(13), "block_signing_key" varchar))xxx",
        R"xxx(create type "test"."producer_schedule" as ("version" bigint, "producers" "test".producer_key[]))xxx",
        R"xxx(create type "test"."variant_chain_config_v0_chain_config_v1" as ("max_block_net_usage" decimal,"target_block_net_usage_pct" bigint,"max_transaction_net_usage" bigint,"base_per_transaction_net_usage" bigint,"net_usage_leeway" bigint,"context_free_discount_net_usage_num" bigint,"context_free_discount_net_usage_den" bigint,"max_block_cpu_usage" bigint,"target_block_cpu_usage_pct" bigint,"max_transaction_cpu_usage" bigint,"min_transaction_cpu_usage" bigint,"max_transaction_lifetime" bigint,"deferred_trx_expiration_window" bigint,"max_transaction_delay" bigint,"max_inline_action_size" bigint,"max_inline_action_depth" integer,"max_authority_depth" integer,"max_action_return_value_size" bigint))xxx",
        R"xxx(create type "test"."key_weight" as ("key" varchar, "weight" integer))xxx",
        R"xxx(create type "test"."variant_block_signing_authority_v0" as ("threshold" bigint,"keys" "test".key_weight[]))xxx",
        R"xxx(create type "test"."producer_authority" as ("producer_name" varchar(13), "authority" "test".variant_block_signing_authority_v0))xxx",
        R"xxx(create type "test"."producer_authority_schedule" as ("version" bigint, "producers" "test".producer_authority[]))xxx",
        R"xxx(create type "test"."kv_database_config" as ("max_key_size" bigint, "max_value_size" bigint, "max_iterators" bigint))xxx",
        R"xxx(create type "test"."wasm_config" as ("max_mutable_global_bytes" bigint, "max_table_elements" bigint, "max_section_elements" bigint, "max_linear_memory_init" bigint, "max_func_local_bytes" bigint, "max_nested_structures" bigint, "max_symbol_bytes" bigint, "max_module_bytes" bigint, "max_code_bytes" bigint, "max_pages" bigint, "max_call_depth" bigint))xxx",
        R"xxx(create type "test"."transaction_hook" as ("type" bigint, "contract" varchar(13), "action" varchar(13)))xxx",
        R"xxx(create type "test"."variant_global_property_extension_v0" as ("proposed_security_group_block_num" bigint,"proposed_security_group_participants" varchar(13)[],"transaction_hooks" "test".transaction_hook[]))xxx",
        R"xxx(create table "test"."global_property" (block_num bigint, "proposed_schedule_block_num" bigint, "global_property_v0_proposed_schedule" "test".producer_schedule, "proposed_schedule" "test".producer_authority_schedule, "configuration" "test".variant_chain_config_v0_chain_config_v1, "chain_id" varchar(64), "kv_configuration" "test".kv_database_config, "wasm_configuration" "test".wasm_config, "extension" "test".variant_global_property_extension_v0, primary key("block_num")))xxx"};

    BOOST_TEST(statements == expected);
}

BOOST_FIXTURE_TEST_CASE(create_transaction_trace_table_test, test_fixture_t) {
    std::vector<std::string> statements;
    auto  exec                = [&statements](std::string stmt) { statements.push_back(stmt); };
    auto& transaction_trace_abi = *abi.add_type<test_protocol::transaction_trace>();

    converter.create_table("transaction_trace", transaction_trace_abi, "block_num bigint", {"block_num"}, exec);
    // std::copy(statements.begin(), statements.end(), std::ostream_iterator<std::string>(std::cout, "\n"));
    std::vector<std::string> expected = {
        R"xxx(create type "test"."account_auth_sequence" as ("account" varchar(13), "sequence" decimal))xxx",
        R"xxx(create type "test"."variant_action_receipt_v0" as ("receiver" varchar(13),"act_digest" varchar(64),"global_sequence" decimal,"recv_sequence" decimal,"auth_sequence" "test".account_auth_sequence[],"code_sequence" bigint,"abi_sequence" bigint))xxx",
        R"xxx(create type "test"."permission_level" as ("actor" varchar(13), "permission" varchar(13)))xxx",
        R"xxx(create type "test"."action" as ("account" varchar(13), "name" varchar(13), "authorization" "test".permission_level[], "data" bytea))xxx",
        R"xxx(create type "test"."account_delta" as ("account" varchar(13), "delta" bigint))xxx",
        R"xxx(create type "test"."variant_action_trace_v0_action_trace_v1" as ("action_ordinal" bigint,"creator_action_ordinal" bigint,"receipt" "test".variant_action_receipt_v0,"receiver" varchar(13),"act" "test".action,"context_free" bool,"elapsed" bigint,"console" varchar,"account_ram_deltas" "test".account_delta[],"account_disk_deltas" "test".account_delta[],"except" varchar,"error_code" decimal,"return_value" bytea))xxx",
        R"xxx(create type "test"."extension" as ("type" integer, "data" bytea))xxx",
        R"xxx(create type "test"."variant_checksum256_bytes" as ("checksum256" varchar(64),"bytes" bytea))xxx",
        R"xxx(create type "test"."variant_prunable_data_full_legacy_prunable_data_none_prunable_data_partial_prunable_data_full" as ("signatures" varchar[],"packed_context_free_data" bytea,"prunable_digest" varchar(64),"prunable_data_partial_context_free_segments" "test".variant_checksum256_bytes[],"context_free_segments" bytea[]))xxx",
        R"xxx(create type "test"."variant_partial_transaction_v0_partial_transaction_v1" as ("expiration" timestamp,"ref_block_num" integer,"ref_block_prefix" bigint,"max_net_usage_words" bigint,"max_cpu_usage_ms" smallint,"delay_sec" bigint,"transaction_extensions" "test".extension[],"signatures" varchar[],"context_free_data" bytea[],"prunable_data" "test".variant_prunable_data_full_legacy_prunable_data_none_prunable_data_partial_prunable_data_full))xxx",
        R"xxx(create table "test"."transaction_trace" (block_num bigint, "id" varchar(64), "status" "test".transaction_status_type, "cpu_usage_us" bigint, "net_usage_words" bigint, "elapsed" bigint, "net_usage" decimal, "scheduled" bool, "action_traces" "test".variant_action_trace_v0_action_trace_v1[], "account_ram_delta" "test".account_delta, "except" varchar, "error_code" decimal, "failed_dtrx_trace" varchar, "partial" "test".variant_partial_transaction_v0_partial_transaction_v1, primary key("block_num")))xxx"};
    BOOST_TEST(statements == expected);
}


template <typename T>
std::vector<std::string> to_sql_values(abieos_sql_converter& converter, const eosio::abi_type& abi, const T& v) {
    std::vector<std::string> values;
    auto                     buf = eosio::convert_to_bin(v);
    eosio::input_stream      bin{buf};
    if (abi.as_struct()) {
        converter.to_sql_values(bin, *abi.as_struct(), values);
    } else if (abi.as_variant()) {
        converter.to_sql_values(bin, abi.name, *abi.as_variant(), values);
    }
    return values;
}

BOOST_FIXTURE_TEST_CASE(to_sql_values_test, test_fixture_t) {
    
    abi.add_type<test_protocol::global_property>();
    using namespace eosio::literals;

    auto& chain_config_abi = *abi.get_type(get_type_name((test_protocol::chain_config*)nullptr));

    {
        test_protocol::chain_config config_v0 = test_protocol::chain_config_v0{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17};
        std::vector<std::string>    values    = to_sql_values(converter, chain_config_abi, config_v0);
        auto                        expected =
            std::vector<std::string>{"1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17", ""};
        BOOST_TEST(values == expected);
    }
    {
        test_protocol::chain_config config_v1 =
            test_protocol::chain_config_v1{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18};
        std::vector<std::string> values = to_sql_values(converter, chain_config_abi, config_v1);
        auto                     expected =
            std::vector<std::string>{"1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17", "18"};
        BOOST_TEST(values == expected);
    }
    {
        test_protocol::authority auth{1,
                                      {{eosio::public_key_from_string("PUB_K1_6Uaww2itj2Ne7ADEdyqpHbsg42rtNQGSNyomEoREdxAHvShLZq"), 1}},
                                      {{{"eosio.prods"_n, "active"_n}, 1}},
                                      {}};

        test_protocol::permission_v0 perm{"eosio"_n, "active"_n, ""_n, eosio::time_point{}, auth};
        auto&                        permission_abi = *abi.add_type<test_protocol::permission>();

        std::vector<std::string> values = to_sql_values(converter, permission_abi, test_protocol::permission{perm});

        auto expected = std::vector<std::string>{
            "eosio", "active", "", "\\N",
            "(1,\"{\\\\\"(\\\\\\\\\\\\\"PUB_K1_6Uaww2itj2Ne7ADEdyqpHbsg42rtNQGSNyomEoREdxAHvShLZq\\\\\\\\\\\\\",1)\\\\\"}\",\"{\\\\\"("
            "\\\\\\\\\\\\\"(\\\\\\\\\\\\\\\\\\\\\\\\\\\\\"eosio.prods\\\\\\\\\\\\\\\\\\\\\\\\\\\\\","
            "\\\\\\\\\\\\\\\\\\\\\\\\\\\\\"active\\\\\\\\\\\\\\\\\\\\\\\\\\\\\")\\\\\\\\\\\\\",1)\\\\\"}\",\"{}\")"};

        BOOST_TEST(values == expected);
    }
    {
        test_protocol::transaction_trace_v0 transaction;
        test_protocol::partial_transaction_v1 pt_v1;
        pt_v1.prunable_data.emplace();
        transaction.partial.emplace(pt_v1);
        auto& transaction_trace_abi = *abi.add_type<test_protocol::transaction_trace>();

        std::vector<std::string> values = to_sql_values(converter, transaction_trace_abi, test_protocol::transaction_trace{transaction});

        auto expected = std::vector<std::string>{"",
                                                 "executed",
                                                 "0",
                                                 "0",
                                                 "0",
                                                 "0",
                                                 "false",
                                                 "{}",
                                                 "\\N",
                                                 "\\N",
                                                 "\\N",
                                                 "\\N",
                                                 R"xxx((,0,0,0,0,0,"{}","{}","{}","(\\"{}\\",\\"\\\\\\\\\\\\\\\\x\\",,\\"{}\\",\\"{}\\")"))xxx"};
        BOOST_TEST(values == expected);   

        transaction.partial.emplace(test_protocol::partial_transaction_v1{});
        values = to_sql_values(converter, transaction_trace_abi, test_protocol::transaction_trace{transaction});
        

        expected = std::vector<std::string>{
            "", "executed", "0", "0", "0", "0", "false", "{}", "\\N", "\\N", "\\N", "\\N", R"xxx((,0,0,0,0,0,"{}","{}","{}",))xxx"};
        BOOST_TEST(values == expected);
    }
    { 
        test_protocol::account_metadata_v0 metadata; 
        auto& account_metadata_abi = *abi.add_type<test_protocol::account_metadata>();
        std::vector<std::string> values = to_sql_values(converter, account_metadata_abi, test_protocol::account_metadata{metadata});
        // std::copy(values.begin(), values.end(), std::ostream_iterator<std::string>(std::cout, "\n"));
        auto expected = std::vector<std::string>{"", "false", "\\N", "\\N"};
        BOOST_TEST(values == expected);
    }
}

BOOST_AUTO_TEST_SUITE_END()
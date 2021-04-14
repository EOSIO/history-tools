#define BOOST_TEST_MODULE ship_sql
#include "test_protocol.hpp"
#include <abieos_sql_converter.hpp>
#include <boost/test/included/unit_test.hpp>

namespace eosio {
template <>
inline constexpr bool is_basic_abi_type<input_stream> = true;
}

bool operator==(const abieos_sql_converter::field_def& lhs, const abieos_sql_converter::field_def& rhs) {
    return lhs.name == rhs.name && lhs.type == rhs.type;
}

std::ostream& operator<<(std::ostream& os, const abieos_sql_converter::field_def& f) {
    return os << "{ " << f.name << " , " << f.type << "}";
}

struct test_fixture_t {
    eosio::abi abi;
    abieos_sql_converter converter;

    test_fixture_t() {
        eosio::abi_def empty_def;
        eosio::convert(empty_def, abi);
        converter.schema_name     = R"("test")";

        using basic_types = std::tuple<
            bool, uint8_t, int8_t, uint16_t, int16_t, uint32_t, int32_t, uint64_t, int64_t, double, std::string, unsigned __int128,
            __int128, eosio::float128, eosio::varuint32, eosio::varint32, eosio::name, eosio::checksum256, eosio::time_point,
            eosio::time_point_sec, eosio::block_timestamp, eosio::public_key, eosio::signature, eosio::bytes, eosio::symbol,
            eosio::ship_protocol::transaction_status, eosio::ship_protocol::recurse_transaction_trace>;

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

BOOST_FIXTURE_TEST_CASE(create_table_test, test_fixture_t) {
    std::vector<std::string> statements;
    auto  exec                = [&statements](std::string stmt) { statements.push_back(stmt); };
    auto& global_property_abi = *abi.add_type<test_protocol::global_property>();

    converter.create_table("global_property", global_property_abi, "block_num bigint", {"block_num"}, exec);
    // std::copy(statements.begin(), statements.end(), std::ostream_iterator<std::string>(std::cout, "\n"));

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
}
BOOST_AUTO_TEST_SUITE_END()
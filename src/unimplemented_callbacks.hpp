#pragma once

#include "basic_callbacks.hpp"

namespace history_tools {

template <typename Derived>
struct unimplemented_callbacks {
    template <typename T>
    T unimplemented(const char* name) {
        throw std::runtime_error("wasm called " + std::string(name) + ", which is unimplemented");
    }

    // compiler_builtins
    void    __ashlti3(int, int64_t, int64_t, int) { return unimplemented<void>("__ashlti3"); }
    void    __ashrti3(int, int64_t, int64_t, int) { return unimplemented<void>("__ashrti3"); }
    void    __lshlti3(int, int64_t, int64_t, int) { return unimplemented<void>("__lshlti3"); }
    void    __lshrti3(int, int64_t, int64_t, int) { return unimplemented<void>("__lshrti3"); }
    void    __divti3(int, int64_t, int64_t, int64_t, int64_t) { return unimplemented<void>("__divti3"); }
    void    __udivti3(int, int64_t, int64_t, int64_t, int64_t) { return unimplemented<void>("__udivti3"); }
    void    __modti3(int, int64_t, int64_t, int64_t, int64_t) { return unimplemented<void>("__modti3"); }
    void    __umodti3(int, int64_t, int64_t, int64_t, int64_t) { return unimplemented<void>("__umodti3"); }
    void    __multi3(int, int64_t, int64_t, int64_t, int64_t) { return unimplemented<void>("__multi3"); }
    void    __addtf3(int, int64_t, int64_t, int64_t, int64_t) { return unimplemented<void>("__addtf3"); }
    void    __subtf3(int, int64_t, int64_t, int64_t, int64_t) { return unimplemented<void>("__subtf3"); }
    void    __multf3(int, int64_t, int64_t, int64_t, int64_t) { return unimplemented<void>("__multf3"); }
    void    __divtf3(int, int64_t, int64_t, int64_t, int64_t) { return unimplemented<void>("__divtf3"); }
    int     __eqtf2(int64_t, int64_t, int64_t, int64_t) { return unimplemented<int>("__eqtf2"); }
    int     __netf2(int64_t, int64_t, int64_t, int64_t) { return unimplemented<int>("__netf2"); }
    int     __getf2(int64_t, int64_t, int64_t, int64_t) { return unimplemented<int>("__getf2"); }
    int     __gttf2(int64_t, int64_t, int64_t, int64_t) { return unimplemented<int>("__gttf2"); }
    int     __lttf2(int64_t, int64_t, int64_t, int64_t) { return unimplemented<int>("__lttf2"); }
    int     __letf2(int64_t, int64_t, int64_t, int64_t) { return unimplemented<int>("__letf2"); }
    int     __cmptf2(int64_t, int64_t, int64_t, int64_t) { return unimplemented<int>("__cmptf2"); }
    int     __unordtf2(int64_t, int64_t, int64_t, int64_t) { return unimplemented<int>("__unordtf2"); }
    void    __negtf2(int, int64_t, int64_t) { return unimplemented<void>("__negtf2 "); }
    void    __floatsitf(int, int) { return unimplemented<void>("__floatsitf "); }
    void    __floatunsitf(int, int) { return unimplemented<void>("__floatunsitf "); }
    void    __floatditf(int, int64_t) { return unimplemented<void>("__floatditf "); }
    void    __floatunditf(int, int64_t) { return unimplemented<void>("__floatunditf "); }
    double  __floattidf(int64_t, int64_t) { return unimplemented<double>("__floattidf "); }
    double  __floatuntidf(int64_t, int64_t) { return unimplemented<double>("__floatuntidf "); }
    double  __floatsidf(int) { return unimplemented<double>("__floatsidf"); }
    void    __extendsftf2(int, float) { return unimplemented<void>("__extendsftf2"); }
    void    __extenddftf2(int, double) { return unimplemented<void>("__extenddftf2"); }
    void    __fixtfti(int, int64_t, int64_t) { return unimplemented<void>("__fixtfti"); }
    int64_t __fixtfdi(int64_t, int64_t) { return unimplemented<int64_t>("__fixtfdi"); }
    int     __fixtfsi(int64_t, int64_t) { return unimplemented<int>("__fixtfsi"); }
    void    __fixunstfti(int, int64_t, int64_t) { return unimplemented<void>("__fixunstfti"); }
    int64_t __fixunstfdi(int64_t, int64_t) { return unimplemented<int64_t>("__fixunstfdi"); }
    int     __fixunstfsi(int64_t, int64_t) { return unimplemented<int>("__fixunstfsi"); }
    void    __fixsfti(int, float) { return unimplemented<void>("__fixsfti"); }
    void    __fixdfti(int, double) { return unimplemented<void>("__fixdfti"); }
    void    __fixunssfti(int, float) { return unimplemented<void>("__fixunssfti"); }
    void    __fixunsdfti(int, double) { return unimplemented<void>("__fixunsdfti"); }
    double  __trunctfdf2(int64_t, int64_t) { return unimplemented<double>("__trunctfdf2"); }
    float   __trunctfsf2(int64_t, int64_t) { return unimplemented<float>("__trunctfsf2"); }

    // privileged_api
    int     is_feature_active(int64_t) { return unimplemented<int>("is_feature_active"); }
    void    activate_feature(int64_t) { return unimplemented<void>("activate_feature"); }
    void    get_resource_limits(int64_t, int, int, int) { return unimplemented<void>("get_resource_limits"); }
    void    set_resource_limits(int64_t, int64_t, int64_t, int64_t) { return unimplemented<void>("set_resource_limits"); }
    int64_t set_proposed_producers(int, int) { return unimplemented<int64_t>("set_proposed_producers"); }
    int     get_blockchain_parameters_packed(int, int) { return unimplemented<int>("get_blockchain_parameters_packed"); }
    void    set_blockchain_parameters_packed(int, int) { return unimplemented<void>("set_blockchain_parameters_packed"); }
    int     is_privileged(int64_t) { return unimplemented<int>("is_privileged"); }
    void    set_privileged(int64_t, int) { return unimplemented<void>("set_privileged"); }
    void    preactivate_feature(int) { return unimplemented<void>("preactivate_feature"); }

    // producer_api
    int get_active_producers(int, int) { return unimplemented<int>("get_active_producers"); }

#define DB_SECONDARY_INDEX_METHODS_SIMPLE(IDX)                                                                                             \
    int  db_##IDX##_store(int64_t, int64_t, int64_t, int64_t, int) { return unimplemented<int>("db_" #IDX "_store"); }                     \
    void db_##IDX##_remove(int) { return unimplemented<void>("db_" #IDX "_remove"); }                                                      \
    void db_##IDX##_update(int, int64_t, int) { return unimplemented<void>("db_" #IDX "_update"); }                                        \
    int  db_##IDX##_find_primary(int64_t, int64_t, int64_t, int, int64_t) { return unimplemented<int>("db_" #IDX "_find_primary"); }       \
    int  db_##IDX##_find_secondary(int64_t, int64_t, int64_t, int, int) { return unimplemented<int>("db_" #IDX "_find_secondary"); }       \
    int  db_##IDX##_lowerbound(int64_t, int64_t, int64_t, int, int) { return unimplemented<int>("db_" #IDX "_lowerbound"); }               \
    int  db_##IDX##_upperbound(int64_t, int64_t, int64_t, int, int) { return unimplemented<int>("db_" #IDX "_upperbound"); }               \
    int  db_##IDX##_end(int64_t, int64_t, int64_t) { return unimplemented<int>("db_" #IDX "_end"); }                                       \
    int  db_##IDX##_next(int, int) { return unimplemented<int>("db_" #IDX "_next"); }                                                      \
    int  db_##IDX##_previous(int, int) { return unimplemented<int>("db_" #IDX "_previous"); }

#define DB_SECONDARY_INDEX_METHODS_ARRAY(IDX)                                                                                              \
    int  db_##IDX##_store(int64_t, int64_t, int64_t, int64_t, int, int) { return unimplemented<int>("db_" #IDX "_store"); }                \
    void db_##IDX##_remove(int) { return unimplemented<void>("db_" #IDX "_remove"); }                                                      \
    void db_##IDX##_update(int, int64_t, int, int) { return unimplemented<void>("db_" #IDX "_update"); }                                   \
    int  db_##IDX##_find_primary(int64_t, int64_t, int64_t, int, int, int64_t) { return unimplemented<int>("db_" #IDX "_find_primary"); }  \
    int  db_##IDX##_find_secondary(int64_t, int64_t, int64_t, int, int, int) { return unimplemented<int>("db_" #IDX "_find_secondary"); }  \
    int  db_##IDX##_lowerbound(int64_t, int64_t, int64_t, int, int, int) { return unimplemented<int>("db_" #IDX "_lowerbound"); }          \
    int  db_##IDX##_upperbound(int64_t, int64_t, int64_t, int, int, int) { return unimplemented<int>("db_" #IDX "_upperbound"); }          \
    int  db_##IDX##_end(int64_t, int64_t, int64_t) { return unimplemented<int>("db_" #IDX "_end"); }                                       \
    int  db_##IDX##_next(int, int) { return unimplemented<int>("db_" #IDX "_next"); }                                                      \
    int  db_##IDX##_previous(int, int) { return unimplemented<int>("db_" #IDX "_previous"); }

    // database_api
    DB_SECONDARY_INDEX_METHODS_SIMPLE(idx64)
    DB_SECONDARY_INDEX_METHODS_SIMPLE(idx128)
    DB_SECONDARY_INDEX_METHODS_ARRAY(idx256)
    DB_SECONDARY_INDEX_METHODS_SIMPLE(idx_double)
    DB_SECONDARY_INDEX_METHODS_SIMPLE(idx_long_double)

#undef DB_SECONDARY_INDEX_METHODS_SIMPLE
#undef DB_SECONDARY_INDEX_METHODS_ARRAY

    // crypto_api
    void assert_recover_key(int, int, int, int, int) { return unimplemented<void>("assert_recover_key"); }
    int  recover_key(int, int, int, int, int) { return unimplemented<int>("recover_key"); }
    void assert_sha256(int, int, int) { return unimplemented<void>("assert_sha256"); }
    void assert_sha1(int, int, int) { return unimplemented<void>("assert_sha1"); }
    void assert_sha512(int, int, int) { return unimplemented<void>("assert_sha512"); }
    void assert_ripemd160(int, int, int) { return unimplemented<void>("assert_ripemd160"); }
    void sha1(int, int, int) { return unimplemented<void>("sha1"); }
    void sha256(int, int, int) { return unimplemented<void>("sha256"); }
    void sha512(int, int, int) { return unimplemented<void>("sha512"); }
    void ripemd160(int, int, int) { return unimplemented<void>("ripemd160"); }

    // permission_api
    int check_transaction_authorization(int, int, int, int, int, int) { return unimplemented<int>("check_transaction_authorization"); }
    int check_permission_authorization(int64_t, int64_t, int, int, int, int, int64_t) {
        return unimplemented<int>("check_permission_authorization");
    }
    int64_t get_permission_last_used(int64_t, int64_t) { return unimplemented<int64_t>("get_permission_last_used"); }
    int64_t get_account_creation_time(int64_t) { return unimplemented<int64_t>("get_account_creation_time"); }

    // system_api
    int64_t current_time() { return unimplemented<int64_t>("current_time"); }
    int64_t publication_time() { return unimplemented<int64_t>("publication_time"); }
    int     is_feature_activated(int) { return unimplemented<int>("is_feature_activated"); }
    int64_t get_sender() { return unimplemented<int64_t>("get_sender"); }

    // context_free_system_api
    void eosio_assert(int, int) { return unimplemented<void>("eosio_assert"); }
    void eosio_assert_code(int, int64_t) { return unimplemented<void>("eosio_assert_code"); }
    void eosio_exit(int) { return unimplemented<void>("eosio_exit"); }

    // authorization_api
    void require_recipient(int64_t) { return unimplemented<void>("require_recipient"); }
    void require_auth(int64_t) { return unimplemented<void>("require_auth"); }
    void require_auth2(int64_t, int64_t) { return unimplemented<void>("require_auth2"); }
    int  has_auth(int64_t) { return unimplemented<int>("has_auth"); }
    int  is_account(int64_t) { return unimplemented<int>("is_account"); }

    // console_api
    void prints(int) { return unimplemented<void>("prints"); }
    void prints_l(int, int) { return unimplemented<void>("prints_l"); }
    void printi(int64_t) { return unimplemented<void>("printi"); }
    void printui(int64_t) { return unimplemented<void>("printui"); }
    void printi128(int) { return unimplemented<void>("printi128"); }
    void printui128(int) { return unimplemented<void>("printui128"); }
    void printsf(float) { return unimplemented<void>("printsf"); }
    void printdf(double) { return unimplemented<void>("printdf"); }
    void printqf(int) { return unimplemented<void>("printqf"); }
    void printn(int64_t) { return unimplemented<void>("printn"); }
    void printhex(int, int) { return unimplemented<void>("printhex"); }

    // context_free_transaction_api
    int read_transaction(int, int) { return unimplemented<int>("read_transaction"); }
    int transaction_size() { return unimplemented<int>("transaction_size"); }
    int expiration() { return unimplemented<int>("expiration"); }
    int tapos_block_prefix() { return unimplemented<int>("tapos_block_prefix"); }
    int tapos_block_num() { return unimplemented<int>("tapos_block_num"); }
    int get_action(int, int, int, int) { return unimplemented<int>("get_action"); }

    // transaction_api
    void send_inline(int, int) { return unimplemented<void>("send_inline"); }
    void send_context_free_inline(int, int) { return unimplemented<void>("send_context_free_inline"); }
    void send_deferred(int, int64_t, int, int, int32_t) { return unimplemented<void>("send_deferred"); }
    int  cancel_deferred(int) { return unimplemented<int>("cancel_deferred"); }

    // context_free_api
    int get_context_free_data(int, int, int) { return unimplemented<int>("get_context_free_data"); }

    template <typename Rft, typename Allocator>
    static void register_callbacks() {
        // compiler_builtins
        Rft::template add<Derived, &Derived::__ashlti3, Allocator>("env", "__ashlti3");
        Rft::template add<Derived, &Derived::__ashrti3, Allocator>("env", "__ashrti3");
        Rft::template add<Derived, &Derived::__lshlti3, Allocator>("env", "__lshlti3");
        Rft::template add<Derived, &Derived::__lshrti3, Allocator>("env", "__lshrti3");
        Rft::template add<Derived, &Derived::__divti3, Allocator>("env", "__divti3");
        Rft::template add<Derived, &Derived::__udivti3, Allocator>("env", "__udivti3");
        Rft::template add<Derived, &Derived::__modti3, Allocator>("env", "__modti3");
        Rft::template add<Derived, &Derived::__umodti3, Allocator>("env", "__umodti3");
        Rft::template add<Derived, &Derived::__multi3, Allocator>("env", "__multi3");
        Rft::template add<Derived, &Derived::__addtf3, Allocator>("env", "__addtf3");
        Rft::template add<Derived, &Derived::__subtf3, Allocator>("env", "__subtf3");
        Rft::template add<Derived, &Derived::__multf3, Allocator>("env", "__multf3");
        Rft::template add<Derived, &Derived::__divtf3, Allocator>("env", "__divtf3");
        Rft::template add<Derived, &Derived::__eqtf2, Allocator>("env", "__eqtf2");
        Rft::template add<Derived, &Derived::__netf2, Allocator>("env", "__netf2");
        Rft::template add<Derived, &Derived::__getf2, Allocator>("env", "__getf2");
        Rft::template add<Derived, &Derived::__gttf2, Allocator>("env", "__gttf2");
        Rft::template add<Derived, &Derived::__lttf2, Allocator>("env", "__lttf2");
        Rft::template add<Derived, &Derived::__letf2, Allocator>("env", "__letf2");
        Rft::template add<Derived, &Derived::__cmptf2, Allocator>("env", "__cmptf2");
        Rft::template add<Derived, &Derived::__unordtf2, Allocator>("env", "__unordtf2");
        Rft::template add<Derived, &Derived::__negtf2, Allocator>("env", "__negtf2");
        Rft::template add<Derived, &Derived::__floatsitf, Allocator>("env", "__floatsitf");
        Rft::template add<Derived, &Derived::__floatunsitf, Allocator>("env", "__floatunsitf");
        Rft::template add<Derived, &Derived::__floatditf, Allocator>("env", "__floatditf");
        Rft::template add<Derived, &Derived::__floatunditf, Allocator>("env", "__floatunditf");
        Rft::template add<Derived, &Derived::__floattidf, Allocator>("env", "__floattidf");
        Rft::template add<Derived, &Derived::__floatuntidf, Allocator>("env", "__floatuntidf");
        Rft::template add<Derived, &Derived::__floatsidf, Allocator>("env", "__floatsidf");
        Rft::template add<Derived, &Derived::__extendsftf2, Allocator>("env", "__extendsftf2");
        Rft::template add<Derived, &Derived::__extenddftf2, Allocator>("env", "__extenddftf2");
        Rft::template add<Derived, &Derived::__fixtfti, Allocator>("env", "__fixtfti");
        Rft::template add<Derived, &Derived::__fixtfdi, Allocator>("env", "__fixtfdi");
        Rft::template add<Derived, &Derived::__fixtfsi, Allocator>("env", "__fixtfsi");
        Rft::template add<Derived, &Derived::__fixunstfti, Allocator>("env", "__fixunstfti");
        Rft::template add<Derived, &Derived::__fixunstfdi, Allocator>("env", "__fixunstfdi");
        Rft::template add<Derived, &Derived::__fixunstfsi, Allocator>("env", "__fixunstfsi");
        Rft::template add<Derived, &Derived::__fixsfti, Allocator>("env", "__fixsfti");
        Rft::template add<Derived, &Derived::__fixdfti, Allocator>("env", "__fixdfti");
        Rft::template add<Derived, &Derived::__fixunssfti, Allocator>("env", "__fixunssfti");
        Rft::template add<Derived, &Derived::__fixunsdfti, Allocator>("env", "__fixunsdfti");
        Rft::template add<Derived, &Derived::__trunctfdf2, Allocator>("env", "__trunctfdf2");
        Rft::template add<Derived, &Derived::__trunctfsf2, Allocator>("env", "__trunctfsf2");

        // privileged_api
        Rft::template add<Derived, &Derived::is_feature_active, Allocator>("env", "is_feature_active");
        Rft::template add<Derived, &Derived::activate_feature, Allocator>("env", "activate_feature");
        Rft::template add<Derived, &Derived::get_resource_limits, Allocator>("env", "get_resource_limits");
        Rft::template add<Derived, &Derived::set_resource_limits, Allocator>("env", "set_resource_limits");
        Rft::template add<Derived, &Derived::set_proposed_producers, Allocator>("env", "set_proposed_producers");
        Rft::template add<Derived, &Derived::get_blockchain_parameters_packed, Allocator>("env", "get_blockchain_parameters_packed");
        Rft::template add<Derived, &Derived::set_blockchain_parameters_packed, Allocator>("env", "set_blockchain_parameters_packed");
        Rft::template add<Derived, &Derived::is_privileged, Allocator>("env", "is_privileged");
        Rft::template add<Derived, &Derived::set_privileged, Allocator>("env", "set_privileged");
        Rft::template add<Derived, &Derived::preactivate_feature, Allocator>("env", "preactivate_feature");

        // producer_api
        Rft::template add<Derived, &Derived::get_active_producers, Allocator>("env", "get_active_producers");

#define DB_SECONDARY_INDEX_METHODS_SIMPLE(IDX)                                                                                             \
    Rft::template add<Derived, &Derived::db_##IDX##_store, Allocator>("env", "db_" #IDX "_store");                                         \
    Rft::template add<Derived, &Derived::db_##IDX##_remove, Allocator>("env", "db_" #IDX "_remove");                                       \
    Rft::template add<Derived, &Derived::db_##IDX##_update, Allocator>("env", "db_" #IDX "_update");                                       \
    Rft::template add<Derived, &Derived::db_##IDX##_find_primary, Allocator>("env", "db_" #IDX "_find_primary");                           \
    Rft::template add<Derived, &Derived::db_##IDX##_find_secondary, Allocator>("env", "db_" #IDX "_find_secondary");                       \
    Rft::template add<Derived, &Derived::db_##IDX##_lowerbound, Allocator>("env", "db_" #IDX "_lowerbound");                               \
    Rft::template add<Derived, &Derived::db_##IDX##_upperbound, Allocator>("env", "db_" #IDX "_upperbound");                               \
    Rft::template add<Derived, &Derived::db_##IDX##_end, Allocator>("env", "db_" #IDX "_end");                                             \
    Rft::template add<Derived, &Derived::db_##IDX##_next, Allocator>("env", "db_" #IDX "_next");                                           \
    Rft::template add<Derived, &Derived::db_##IDX##_previous, Allocator>("env", "db_" #IDX "_previous");

#define DB_SECONDARY_INDEX_METHODS_ARRAY(IDX)                                                                                              \
    Rft::template add<Derived, &Derived::db_##IDX##_store, Allocator>("env", "db_" #IDX "_store");                                         \
    Rft::template add<Derived, &Derived::db_##IDX##_remove, Allocator>("env", "db_" #IDX "_remove");                                       \
    Rft::template add<Derived, &Derived::db_##IDX##_update, Allocator>("env", "db_" #IDX "_update");                                       \
    Rft::template add<Derived, &Derived::db_##IDX##_find_primary, Allocator>("env", "db_" #IDX "_find_primary");                           \
    Rft::template add<Derived, &Derived::db_##IDX##_find_secondary, Allocator>("env", "db_" #IDX "_find_secondary");                       \
    Rft::template add<Derived, &Derived::db_##IDX##_lowerbound, Allocator>("env", "db_" #IDX "_lowerbound");                               \
    Rft::template add<Derived, &Derived::db_##IDX##_upperbound, Allocator>("env", "db_" #IDX "_upperbound");                               \
    Rft::template add<Derived, &Derived::db_##IDX##_end, Allocator>("env", "db_" #IDX "_end");                                             \
    Rft::template add<Derived, &Derived::db_##IDX##_next, Allocator>("env", "db_" #IDX "_next");                                           \
    Rft::template add<Derived, &Derived::db_##IDX##_previous, Allocator>("env", "db_" #IDX "_previous");

        // database_api
        DB_SECONDARY_INDEX_METHODS_SIMPLE(idx64)
        DB_SECONDARY_INDEX_METHODS_SIMPLE(idx128)
        DB_SECONDARY_INDEX_METHODS_ARRAY(idx256)
        DB_SECONDARY_INDEX_METHODS_SIMPLE(idx_double)
        DB_SECONDARY_INDEX_METHODS_SIMPLE(idx_long_double)

#undef DB_SECONDARY_INDEX_METHODS_SIMPLE
#undef DB_SECONDARY_INDEX_METHODS_ARRAY

        // crypto_api
        Rft::template add<Derived, &Derived::assert_recover_key, Allocator>("env", "assert_recover_key");
        Rft::template add<Derived, &Derived::recover_key, Allocator>("env", "recover_key");
        Rft::template add<Derived, &Derived::assert_sha256, Allocator>("env", "assert_sha256");
        Rft::template add<Derived, &Derived::assert_sha1, Allocator>("env", "assert_sha1");
        Rft::template add<Derived, &Derived::assert_sha512, Allocator>("env", "assert_sha512");
        Rft::template add<Derived, &Derived::assert_ripemd160, Allocator>("env", "assert_ripemd160");
        Rft::template add<Derived, &Derived::sha1, Allocator>("env", "sha1");
        Rft::template add<Derived, &Derived::sha256, Allocator>("env", "sha256");
        Rft::template add<Derived, &Derived::sha512, Allocator>("env", "sha512");
        Rft::template add<Derived, &Derived::ripemd160, Allocator>("env", "ripemd160");

        // permission_api
        Rft::template add<Derived, &Derived::check_transaction_authorization, Allocator>("env", "check_transaction_authorization");
        Rft::template add<Derived, &Derived::check_permission_authorization, Allocator>("env", "check_permission_authorization");
        Rft::template add<Derived, &Derived::get_permission_last_used, Allocator>("env", "get_permission_last_used");
        Rft::template add<Derived, &Derived::get_account_creation_time, Allocator>("env", "get_account_creation_time");

        // system_api
        Rft::template add<Derived, &Derived::current_time, Allocator>("env", "current_time");
        Rft::template add<Derived, &Derived::publication_time, Allocator>("env", "publication_time");
        Rft::template add<Derived, &Derived::is_feature_activated, Allocator>("env", "is_feature_activated");
        Rft::template add<Derived, &Derived::get_sender, Allocator>("env", "get_sender");

        // context_free_system_api
        Rft::template add<Derived, &Derived::eosio_assert, Allocator>("env", "eosio_assert");
        Rft::template add<Derived, &Derived::eosio_assert_code, Allocator>("env", "eosio_assert_code");
        Rft::template add<Derived, &Derived::eosio_exit, Allocator>("env", "eosio_exit");

        // authorization_api
        Rft::template add<Derived, &Derived::require_recipient, Allocator>("env", "require_recipient");
        Rft::template add<Derived, &Derived::require_auth, Allocator>("env", "require_auth");
        Rft::template add<Derived, &Derived::require_auth2, Allocator>("env", "require_auth2");
        Rft::template add<Derived, &Derived::has_auth, Allocator>("env", "has_auth");
        Rft::template add<Derived, &Derived::is_account, Allocator>("env", "is_account");

        // console_api
        Rft::template add<Derived, &Derived::prints, Allocator>("env", "prints");
        Rft::template add<Derived, &Derived::prints_l, Allocator>("env", "prints_l");
        Rft::template add<Derived, &Derived::printi, Allocator>("env", "printi");
        Rft::template add<Derived, &Derived::printui, Allocator>("env", "printui");
        Rft::template add<Derived, &Derived::printi128, Allocator>("env", "printi128");
        Rft::template add<Derived, &Derived::printui128, Allocator>("env", "printui128");
        Rft::template add<Derived, &Derived::printsf, Allocator>("env", "printsf");
        Rft::template add<Derived, &Derived::printdf, Allocator>("env", "printdf");
        Rft::template add<Derived, &Derived::printqf, Allocator>("env", "printqf");
        Rft::template add<Derived, &Derived::printn, Allocator>("env", "printn");
        Rft::template add<Derived, &Derived::printhex, Allocator>("env", "printhex");

        // context_free_transaction_api
        Rft::template add<Derived, &Derived::read_transaction, Allocator>("env", "read_transaction");
        Rft::template add<Derived, &Derived::transaction_size, Allocator>("env", "transaction_size");
        Rft::template add<Derived, &Derived::expiration, Allocator>("env", "expiration");
        Rft::template add<Derived, &Derived::tapos_block_prefix, Allocator>("env", "tapos_block_prefix");
        Rft::template add<Derived, &Derived::tapos_block_num, Allocator>("env", "tapos_block_num");
        Rft::template add<Derived, &Derived::get_action, Allocator>("env", "get_action");

        // transaction_api
        Rft::template add<Derived, &Derived::send_inline, Allocator>("env", "send_inline");
        Rft::template add<Derived, &Derived::send_context_free_inline, Allocator>("env", "send_context_free_inline");
        Rft::template add<Derived, &Derived::send_deferred, Allocator>("env", "send_deferred");
        Rft::template add<Derived, &Derived::cancel_deferred, Allocator>("env", "cancel_deferred");

        // context_free_api
        Rft::template add<Derived, &Derived::get_context_free_data, Allocator>("env", "get_context_free_data");
    } // register_callbacks()
};    // unimplemented_callbacks

} // namespace history_tools

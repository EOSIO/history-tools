// copyright defined in LICENSE.txt

#pragma once
#include <eosio/database.hpp>
#include <eosio/tagged_variant.hpp>

/// \file
/// ```c++
/// using absolute_block     = tagged_type<"absolute"_n, int32_t>;
/// using head_block         = tagged_type<"head"_n, int32_t>;
/// using irreversible_block = tagged_type<"irreversible"_n, int32_t>;
///
/// using block_select       = tagged_variant<
///                                serialize_tag_as_index,
///                                absolute_block,
///                                head_block,
///                                irreversible_block>;
///
/// block_select make_absolute_block(int32_t i);
///
/// uint32_t get_block_num(const block_select& sel, const database_status& status);
/// ```
/// `block_select` identifies blocks. This appears in JSON as one of the following:
/// ```
/// ["absolute", 1234]          Block 1234
/// ["head", -10]               10 blocks behind head
/// ["irreversible", -4]        4 blocks behind irreversible
/// ```
///
/// ```c++
/// block_select make_absolute_block(int32_t i);
/// ```
/// Returns a `block_select` which references block `i`
///
/// ```c++
/// uint32_t get_block_num(const block_select& sel, const database_status& status);
/// ```
/// Returns the block that `sel` references.

/// \exclude
namespace eosio {

using absolute_block     = tagged_type<"absolute"_n, int32_t>;
using head_block         = tagged_type<"head"_n, int32_t>;
using irreversible_block = tagged_type<"irreversible"_n, int32_t>;
using block_select       = tagged_variant<serialize_tag_as_index, absolute_block, head_block, irreversible_block>;

inline std::string_view schema_type_name(block_select*) { return "eosio::block_select"; }

inline block_select make_absolute_block(int32_t i) {
    block_select result;
    result.value.emplace<0>(i);
    return result;
}

inline uint32_t get_block_num(const block_select& sel, const database_status& status) {
    switch (sel.value.index()) {
    case 0: return std::max((int32_t)0, std::get<0>(sel.value));
    case 1: return std::max((int32_t)0, (int32_t)status.head + std::get<1>(sel.value));
    case 2: return std::max((int32_t)0, (int32_t)status.irreversible + std::get<2>(sel.value));
    default: return 0x7fffffff;
    }
}

} // namespace eosio

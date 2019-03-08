// copyright defined in LICENSE.txt

#pragma once
#include <eosio/name.hpp>
#include <eosio/varint.hpp>
#include <variant>

/// \file
/// A `tagged_variant` is a type-safe union; it may contain one value from a set of types. Each type
/// is tagged with a name; this name appears in the JSON representation and optionally in the binary
/// serialization.
///
/// Example:
/// ```c++
/// using token_query_request = eosio::tagged_variant<                                //
///     eosio::serialize_tag_as_name,                                                 //
///     eosio::tagged_type<"transfer"_n, token_transfer_request>,                     //
///     eosio::tagged_type<"bal.mult.acc"_n, balances_for_multiple_accounts_request>, //
///     eosio::tagged_type<"bal.mult.tok"_n, balances_for_multiple_tokens_request>>;  //
/// ```
///
/// The JSON representation takes one of the following forms:
///
/// ```
/// ["transfer", { ...token_transfer_request properties... }]
/// ["bal.mult.acc", { ...balances_for_multiple_accounts_request properties... }]
/// ["bal.mult.tok", { ...balances_for_multiple_tokens_request properties... }]
/// ```
///
/// Since this example uses the `eosio::serialize_tag_as_name` flag, the binary serialization
/// for instances of this type contains:
/// * 8-byte `eosio::name` containing "transfer", "bal.mult.acc", or "bal.mult.tok"
/// * bytes for `token_transfer_request`, `balances_for_multiple_accounts_request`,
///   or balances_for_multiple_tokens_request
///
/// If the `serialize_tag_as_index` was used instead, the binary serialization would contain:
/// * 1-byte `eosio::unsigned_int` containing 0, 1, or 2
/// * bytes for `token_transfer_request`, `balances_for_multiple_accounts_request`,
///   or balances_for_multiple_tokens_request
///
/// The 1-byte `eosio::unsigned_int` format is more compact, and is compatible with
/// ABI 1.1's variant serialization. The 8-byte `name` format provides an easier path to
/// upgrading the format of binary data in the future.

namespace eosio {

/// Pairs a name with a type
template <name::raw N, typename T>
struct tagged_type {
    static inline constexpr eosio::name name = eosio::name{N};

    /// \exclude
    static inline constexpr bool type_is_empty = false;

    using type = T;
};

/// \exclude
template <name::raw N>
struct tagged_empty_type {
    static inline constexpr eosio::name name          = eosio::name{N};
    static inline constexpr bool        type_is_empty = true;
    using type                                        = tagged_empty_type;
};

/// \exclude
template <typename T>
struct is_tagged_empty_type {
    static constexpr bool value = false;
};

/// \exclude
template <name::raw N>
struct is_tagged_empty_type<tagged_empty_type<N>> {
    static constexpr bool value = true;
};

/// \exclude
template <typename T>
static inline constexpr bool is_named_empty_type_v = is_tagged_empty_type<T>::value;

/// Options for `tagged_variant`
enum tagged_variant_options {
    serialize_tag_as_name  = 1,
    serialize_tag_as_index = 2,
};

// todo: compile-time check for duplicate keys
/// Type-safe union
template <tagged_variant_options Options, typename... NamedTypes>
struct tagged_variant {
    std::variant<typename NamedTypes::type...> value;
    static inline constexpr name               keys[] = {NamedTypes::name...};
};

/// \exclude
template <size_t I, typename DataStream, tagged_variant_options Options, typename... NamedTypes>
DataStream& deserialize_named_variant_impl(DataStream& ds, tagged_variant<Options, NamedTypes...>& v, size_t i) {
    if constexpr (I < sizeof...(NamedTypes)) {
        if (i == I) {
            auto& q = v.value;
            auto& x = q.template emplace<I>();
            if constexpr (!is_named_empty_type_v<std::decay_t<decltype(x)>>)
                ds >> x;
            return ds;
        } else {
            return deserialize_named_variant_impl<I + 1>(ds, v, i);
        }
    } else {
        check(false, "invalid variant index");
        return ds;
    }
}

/// \exclude
template <typename DataStream, tagged_variant_options Options, typename... NamedTypes>
DataStream& operator>>(DataStream& ds, tagged_variant<Options, NamedTypes...>& v) {
    if constexpr (!!(Options & serialize_tag_as_name)) {
        eosio::name name;
        ds >> name;
        for (size_t i = 0; i < sizeof...(NamedTypes); ++i)
            if (name == v.keys[i])
                return deserialize_named_variant_impl<0>(ds, v, i);
        check(false, "invalid variant index name");
    } else if constexpr (!!(Options & serialize_tag_as_index)) {
        unsigned_int i;
        ds >> i;
        return deserialize_named_variant_impl<0>(ds, v, i);
    }
    return ds;
}

/// \exclude
template <typename DataStream, tagged_variant_options Options, typename... NamedTypes>
DataStream& operator<<(DataStream& ds, const tagged_variant<Options, NamedTypes...>& v) {
    if constexpr (!!(Options & serialize_tag_as_name))
        ds << tagged_variant<Options, NamedTypes...>::keys[v.value.index()];
    else if constexpr (!!(Options & serialize_tag_as_index))
        ds << unsigned_int{v.value.index()};
    std::visit(
        [&](auto& x) {
            if constexpr (!is_named_empty_type_v<std::decay_t<decltype(x)>>)
                ds << x;
        },
        v.value);
    return ds;
}

} // namespace eosio

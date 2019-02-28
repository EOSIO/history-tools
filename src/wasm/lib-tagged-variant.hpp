// copyright defined in LICENSE.txt

#pragma once
#include <eosiolib/name.hpp>
#include <eosiolib/varint.hpp>
#include <variant>

namespace eosio {

template <name::raw N, typename T>
struct tagged_type {
    static inline constexpr eosio::name name          = eosio::name{N};
    static inline constexpr bool        type_is_empty = false;
    using type                                        = T;
};

template <name::raw N>
struct tagged_empty_type {
    static inline constexpr eosio::name name          = eosio::name{N};
    static inline constexpr bool        type_is_empty = true;
    using type                                        = tagged_empty_type;
};

template <typename T>
struct is_tagged_empty_type {
    static constexpr bool value = false;
};

template <name::raw N>
struct is_tagged_empty_type<tagged_empty_type<N>> {
    static constexpr bool value = true;
};

template <typename T>
static inline constexpr bool is_named_empty_type_v = is_tagged_empty_type<T>::value;

enum tagged_variant_options {
    serialize_tag_as_name  = 1,
    serialize_tag_as_index = 2,
};

// todo: compile-time check for duplicate keys
template <tagged_variant_options Options, typename... NamedTypes>
struct tagged_variant {
    std::variant<typename NamedTypes::type...> value;
    static inline constexpr name               keys[] = {NamedTypes::name...};
};

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
        eosio_assert(false, "invalid variant index");
        return ds;
    }
}

template <typename DataStream, tagged_variant_options Options, typename... NamedTypes>
DataStream& operator>>(DataStream& ds, tagged_variant<Options, NamedTypes...>& v) {
    if constexpr (!!(Options & serialize_tag_as_name)) {
        eosio::name name;
        ds >> name;
        for (size_t i = 0; i < sizeof...(NamedTypes); ++i)
            if (name == v.keys[i])
                return deserialize_named_variant_impl<0>(ds, v, i);
        eosio_assert(false, "invalid variant index name");
    } else if constexpr (!!(Options & serialize_tag_as_index)) {
        unsigned_int i;
        ds >> i;
        return deserialize_named_variant_impl<0>(ds, v, i);
    }
    return ds;
}

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

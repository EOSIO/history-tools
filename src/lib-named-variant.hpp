// copyright defined in LICENSE.txt

#pragma once
#include <eosiolib/name.hpp>
#include <variant>

template <eosio::name::raw N, typename T>
struct named_type {
    static inline constexpr eosio::name name = eosio::name{N};
    using type                               = T;
};

// todo: compile-time check for duplicate keys
template <typename... NamedTypes>
struct named_variant {
    std::variant<typename NamedTypes::type...> value;
    static inline constexpr eosio::name        keys[] = {NamedTypes::name...};
};

template <size_t I, typename DataStream, typename... NamedTypes>
DataStream& deserialize_named_variant_impl(DataStream& ds, named_variant<NamedTypes...>& v, size_t i) {
    if constexpr (I < sizeof...(NamedTypes)) {
        if (i == I) {
            auto& q = v.value;
            auto& x = q.template emplace<I>();
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

template <typename DataStream, typename... NamedTypes>
DataStream& operator>>(DataStream& ds, named_variant<NamedTypes...>& v) {
    eosio::name name;
    ds >> name;
    for (size_t i = 0; i < sizeof...(NamedTypes); ++i)
        if (name == v.keys[i])
            return deserialize_named_variant_impl<0>(ds, v, i);
    eosio_assert(false, "invalid variant index name");
    return ds;
}

template <typename DataStream, typename... NamedTypes>
DataStream& operator<<(DataStream& ds, const named_variant<NamedTypes...>& v) {
    ds << named_variant<NamedTypes...>::keys[v.value.index()];
    std::visit([&](auto& v) { ds << v; }, v.value);
    return ds;
}

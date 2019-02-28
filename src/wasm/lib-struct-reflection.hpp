// copyright defined in LICENSE.txt

#pragma once

#include <type_traits>

namespace eosio {

template <auto P>
struct member_ptr;

template <class C, typename M>
const C* class_from_void(M C::*, const void* v) {
    return reinterpret_cast<const C*>(v);
}

template <class C, typename M>
C* class_from_void(M C::*, void* v) {
    return reinterpret_cast<C*>(v);
}

template <auto P>
auto& member_from_void(const member_ptr<P>&, const void* p) {
    return class_from_void(P, p)->*P;
}

template <auto P>
auto& member_from_void(const member_ptr<P>&, void* p) {
    return class_from_void(P, p)->*P;
}

template <auto P>
struct member_ptr {
    using member_type                  = std::decay_t<decltype(member_from_void(std::declval<member_ptr<P>>(), std::declval<void*>()))>;
    static constexpr member_type* null = nullptr;
};

#define STRUCT_REFLECT(STRUCT)                                                                                                             \
    template <typename F>                                                                                                                  \
    void for_each_member(STRUCT*, F f)

#define STRUCT_MEMBER(STRUCT, MEMBER) f(#MEMBER, eosio::member_ptr<&STRUCT::MEMBER>{});

} // namespace eosio

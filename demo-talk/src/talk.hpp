#pragma once

#include <eosio/shared_memory.hpp>
#include <eosio/struct_reflection.hpp>
#include <eosio/tagged_variant.hpp>
#include <optional>
#include <vector>

#ifdef WASM_QL
using talk_string = eosio::shared_memory<std::string_view>; // wasm-ql: this type is more efficient
#else
using talk_string = std::string; // contracts need this type
#endif

// Message table
struct [[eosio::table("message"), eosio::contract("talk")]] message {
    uint64_t    id       = {};
    uint64_t    reply_to = {}; // Non-0 if this is a reply
    eosio::name user     = {};
    talk_string content  = {};

    uint64_t primary_key() const { return id; }
    uint64_t get_reply_to() const { return reply_to; }
};

// JSON <> binary conversion
STRUCT_REFLECT(message) {
    STRUCT_MEMBER(message, id)
    STRUCT_MEMBER(message, reply_to)
    STRUCT_MEMBER(message, user)
    STRUCT_MEMBER(message, content)
}

// Describes the position of a message within a tree
struct message_position {
    std::vector<uint64_t> parent_ids = {}; // [..., great-grandparent, grandparent, parent]. Empty for a top-level post.
    uint64_t              id         = {}; // message id
};

STRUCT_REFLECT(message_position) {
    STRUCT_MEMBER(message_position, parent_ids)
    STRUCT_MEMBER(message_position, id)
}

// Parameters for a query
struct get_messages_request {
    message_position begin        = {}; // Where to begin
    uint32_t         max_messages = {}; // Maximum messages to retrieve
};

STRUCT_REFLECT(get_messages_request) {
    STRUCT_MEMBER(get_messages_request, begin)
    STRUCT_MEMBER(get_messages_request, max_messages)
}

// Query response
struct get_messages_response {
    std::vector<message>            messages = {};
    std::optional<message_position> more     = {};

    EOSLIB_SERIALIZE(get_messages_response, (messages)(more))
};

STRUCT_REFLECT(get_messages_response) {
    STRUCT_MEMBER(get_messages_response, messages)
    STRUCT_MEMBER(get_messages_response, more)
}

// The set of available queries. Each query has a name which identifies it.
using talk_query_request = eosio::tagged_variant< //
    eosio::serialize_tag_as_name,                 //
    eosio::tagged_type<"get.messages"_n, get_messages_request>>;

// The set of available responses. Each response has a name which identifies it.
// The name must match the request's name.
using talk_query_response = eosio::tagged_variant< //
    eosio::serialize_tag_as_name,                  //
    eosio::tagged_type<"get.messages"_n, get_messages_response>>;

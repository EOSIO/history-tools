#pragma once

#include <regex>
#include <fc/filesystem.hpp>
#include <fc/io/json.hpp>
#include "util.hpp"
#include "action_abi.hpp"




/**
* Given a string in the form <key>=<value> where key cannot contain an `=` character and value can contain anything
* return a pair of the two independent strings
*
* @param input
* @return
*/
std::pair<std::string, std::string> parse_kv_pairs( const std::string& input ) {
    // EOS_ASSERT(!input.empty(), chain::plugin_config_exception, "Key-Value Pair is Empty");
    auto delim = input.find("=");
    // EOS_ASSERT(delim != std::string::npos, chain::plugin_config_exception, "Missing \"=\"");
    // EOS_ASSERT(delim != 0, chain::plugin_config_exception, "Missing Key");
    // EOS_ASSERT(delim + 1 != input.size(), chain::plugin_config_exception, "Missing Value");
    return std::make_pair(input.substr(0, delim), input.substr(delim + 1));
}


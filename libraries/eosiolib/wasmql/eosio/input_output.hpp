// copyright defined in LICENSE.txt

#pragma once

#include <eosio/print.hpp>
#include <eosio/rope.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace eosio {

/// \exclude
typedef void* cb_alloc_fn(void* cb_alloc_data, size_t size);

/// \exclude
extern "C" __attribute__((eosio_wasm_import)) uint32_t get_input_data(char* dest, uint32_t size);

/// Get data provided to the wasm
inline std::vector<char> get_input_data() {
    std::vector<char> result(get_input_data(nullptr, 0));
    get_input_data(result.data(), result.size());
    return result;
}

/// Get data provided to the wasm
inline std::string get_input_data_str() {
    std::string result(get_input_data(nullptr, 0), 0);
    get_input_data(result.data(), result.size());
    return result;
}

extern "C" {
/// Set the wasm's output data
__attribute__((eosio_wasm_import)) void set_output_data(const char* data, uint32_t size);
}

/// Set the wasm's output data
inline void set_output_data(const std::vector<char>& v) { set_output_data(v.data(), v.size()); }

/// Set the wasm's output data
inline void set_output_data(const std::string_view& v) { set_output_data(v.data(), v.size()); }

/// Set the wasm's output data
inline void set_output_data(rope v) { return set_output_data(v.sv()); }

} // namespace eosio

// copyright defined in LICENSE.txt

#pragma once

namespace eosio {

typedef void* cb_alloc_fn(void* cb_alloc_data, size_t size);

extern "C" void get_input_data(void* cb_alloc_data, cb_alloc_fn* cb_alloc);

template <typename Alloc_fn>
inline void get_input_data(Alloc_fn alloc_fn) {
    get_input_data(&alloc_fn, [](void* cb_alloc_data, size_t size) -> void* { //
        return (*reinterpret_cast<Alloc_fn*>(cb_alloc_data))(size);
    });
}

inline std::vector<char> get_input_data() {
    std::vector<char> result;
    get_input_data([&result](size_t size) {
        result.resize(size);
        return result.data();
    });
    return result;
}

extern "C" void set_output_data(const char* begin, const char* end);
inline void     set_output_data(const std::vector<char>& v) { set_output_data(v.data(), v.data() + v.size()); }
inline void     set_output_data(const std::string_view& v) { set_output_data(v.data(), v.data() + v.size()); }

} // namespace eosio

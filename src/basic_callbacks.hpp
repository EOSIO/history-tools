#pragma once

#include <abieos_exception.hpp>
#include <eosio/vm/backend.hpp>

namespace history_tools {

struct assert_exception : std::exception {
    std::string msg;

    assert_exception(std::string&& msg)
        : msg(std::move(msg)) {}

    const char* what() const noexcept override { return msg.c_str(); }
};

template <typename Backend>
struct wasm_state {
    eosio::vm::wasm_allocator& wa;
    Backend&                   backend;
};

template <typename Derived>
struct basic_callbacks {
    Derived& derived() { return static_cast<Derived&>(*this); }

    void check_bounds(const char* begin, const char* end) {
        if (begin > end)
            throw std::runtime_error("bad memory");
        // todo: check bounds
    }

    void check_bounds(const char* begin, uint32_t size) {
        // todo: check bounds
    }

    char* alloc(uint32_t cb_alloc_data, uint32_t cb_alloc, uint32_t size) {
        // todo: verify cb_alloc isn't in imports
        auto& state = derived().state;
        if (state.backend.get_module().tables.size() < 0 || state.backend.get_module().tables[0].table.size() < cb_alloc)
            throw std::runtime_error("cb_alloc is out of range");
        auto result = state.backend.get_context().execute(
            &derived(), eosio::vm::jit_visitor(42), state.backend.get_module().tables[0].table[cb_alloc], cb_alloc_data, size);
        if (!result || !result->template is_a<eosio::vm::i32_const_t>())
            throw std::runtime_error("cb_alloc returned incorrect type");
        char* begin = state.wa.template get_base_ptr<char>() + result->to_ui32();
        check_bounds(begin, begin + size);
        return begin;
    }

    void set_data(uint32_t cb_alloc_data, uint32_t cb_alloc, abieos::input_buffer data) {
        memcpy(alloc(cb_alloc_data, cb_alloc, data.end - data.pos), data.pos, data.end - data.pos);
    }

    template <typename T>
    void set_data(uint32_t cb_alloc_data, uint32_t cb_alloc, const T& data) {
        memcpy(alloc(cb_alloc_data, cb_alloc, data.size()), data.data(), data.size());
    }

    void abort() { throw std::runtime_error("called abort"); }

    void eosio_assert_message(bool test, const char* msg, size_t msg_len) {
        check_bounds(msg, msg + msg_len);
        if (!test)
            throw assert_exception(std::string(msg, msg_len));
    }

    void print_range(const char* begin, const char* end) {
        check_bounds(begin, end);
        std::cerr.write(begin, end - begin);
    }

    template <typename Rft, typename Allocator>
    static void register_callbacks() {
        Rft::template add<Derived, &Derived::abort, Allocator>("env", "abort");
        Rft::template add<Derived, &Derived::eosio_assert_message, Allocator>("env", "eosio_assert_message");
        Rft::template add<Derived, &Derived::print_range, Allocator>("env", "print_range");
    }
};

} // namespace history_tools

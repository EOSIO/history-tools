#include "state_history_connection.hpp"
#include "state_history_rocksdb.hpp"
#include <abieos_exception.hpp>
#include <eosio/vm/backend.hpp>
#include <fc/exception/exception.hpp>

struct callbacks;
using backend_t = eosio::vm::backend<callbacks, eosio::vm::jit>;
using rhf_t     = eosio::vm::registered_host_functions<callbacks>;

struct assert_exception : std::exception {
    std::string msg;

    assert_exception(std::string&& msg)
        : msg(std::move(msg)) {}

    const char* what() const noexcept override { return msg.c_str(); }
};

namespace eosio {
namespace vm {

template <>
struct wasm_type_converter<const char*> : linear_memory_access {
    auto from_wasm(void* ptr) { return (const char*)ptr; }
};

template <>
struct wasm_type_converter<char*> : linear_memory_access {
    auto from_wasm(void* ptr) { return (char*)ptr; }
};

template <typename T>
struct wasm_type_converter<T&> : linear_memory_access {
    auto from_wasm(uint32_t val) {
        EOS_VM_ASSERT(val != 0, wasm_memory_exception, "references cannot be created for null pointers");
        void* ptr = get_ptr(val);
        validate_ptr<T>(ptr, 1);
        return eosio::vm::aligned_ref_wrapper<T, alignof(T)>{ptr};
    }
};

} // namespace vm
} // namespace eosio

class combined_db {
  public:
    class iterator;

  private:
    using change_map = std::map<std::vector<char>, std::pair<bool, std::vector<char>>>;

    state_history::rdb::database db{"db.rocksdb", {}, {}, true};
    rocksdb::WriteBatch          write_batch;
    change_map                   changes;

    static bool key_eq(abieos::input_buffer a, abieos::input_buffer b) { return std::equal(a.pos, a.end, b.pos, b.end); }

    static bool key_less(abieos::input_buffer a, abieos::input_buffer b) {
        return std::lexicographical_compare(a.pos, a.end, b.pos, b.end);
    }

    struct iterator_impl {
        friend combined_db;
        friend iterator;

        combined_db&                       combined;
        bool                               valid = false;
        std::unique_ptr<rocksdb::Iterator> rocks_it;
        change_map::iterator               change_it;

        iterator_impl(combined_db& combined)
            : combined{combined}
            , rocks_it{combined.db.db->NewIterator(rocksdb::ReadOptions())}
            , change_it{combined.changes.end()} {}

        iterator_impl(const iterator_impl&) = delete;
        iterator_impl& operator=(const iterator_impl&) = delete;

        void move_to_begin() {
            valid = true;
            rocks_it->SeekToFirst();
            change_it = combined.changes.begin();
        }

        void move_to_end() { valid = false; }

        bool is_end() {
            invalidate_if_removed();
            return !valid || (!rocks_it->Valid() && change_it == combined.changes.end());
        }

        std::optional<std::pair<abieos::input_buffer, abieos::input_buffer>> get_kv() {
            invalidate_if_removed();
            if (!valid)
                return {};
            auto r = deref_rocks_it();
            auto c = deref_change_it();
            if (r && (!c || key_less(r->first, c->first)))
                return r;
            else
                return c;
        }

        iterator_impl& operator++() {
            if (!valid)
                return *this;
            auto r = deref_rocks_it();
            auto c = deref_change_it();
            if (r && (!c || key_less(r->first, c->first)))
                rocks_it->Next();
            else if (c)
                ++change_it;
            return *this;
        }

        void invalidate_if_removed() {
            if (change_it != combined.changes.end() && !change_it->second.first)
                valid = false;
        }

        std::optional<std::pair<abieos::input_buffer, abieos::input_buffer>> deref_rocks_it() {
            if (rocks_it->Valid())
                return {{state_history::rdb::to_input_buffer(rocks_it->key()), state_history::rdb::to_input_buffer(rocks_it->value())}};
            else
                return {};
        }

        std::optional<std::pair<abieos::input_buffer, abieos::input_buffer>> deref_change_it() {
            if (change_it != combined.changes.end())
                return {{{change_it->first.data(), change_it->first.data() + change_it->first.size()},
                         {change_it->second.second.data(), change_it->second.second.data() + change_it->second.second.size()}}};
            else
                return {};
        }
    }; // iterator_impl
    friend iterator_impl;

  public:
    class iterator {
        friend combined_db;

      private:
        std::unique_ptr<iterator_impl> impl;

        iterator(std::unique_ptr<iterator_impl> impl)
            : impl(std::move(impl)) {}

      public:
        iterator()                = default;
        iterator(const iterator&) = delete;
        iterator(iterator&&)      = default;

        iterator& operator=(const iterator&) = delete;
        iterator& operator=(iterator&&) = default;

        friend bool operator==(const iterator& a, const iterator& b) {
            auto akv = a.get_kv();
            auto bkv = b.get_kv();
            if (!akv || !bkv)
                return !akv && !bkv;
            return combined_db::key_eq(akv->first, bkv->first);
        }

        friend bool operator<(const iterator& a, const iterator& b) {
            auto akv = a.get_kv();
            auto bkv = b.get_kv();
            if (!akv || !bkv)
                return akv && !bkv;
            return combined_db::key_less(akv->first, bkv->first);
        }

        iterator& operator++() {
            if (impl)
                ++*impl;
            return *this;
        }

        bool is_end() { return !impl || impl->is_end(); }

        std::optional<std::pair<abieos::input_buffer, abieos::input_buffer>> get_kv() const {
            if (impl)
                return impl->get_kv();
            else
                return {};
        }
    };
    friend iterator;

    void discard_changes() {
        write_batch.Clear();
        changes.clear();
    }

    void write_changes() {
        write(db, write_batch);
        discard_changes();
    }

    iterator begin(iterator it = {}) {
        if (!it.impl)
            it.impl = std::make_unique<iterator_impl>(*this);
        it.impl->move_to_begin();
        return it;
    }

    iterator end() { return {}; }

    void set(abieos::input_buffer k, abieos::input_buffer v) {
        write_batch.Put(state_history::rdb::to_slice(k), state_history::rdb::to_slice(v));
        changes[{k.pos, k.end}] = {true, {v.pos, v.end}};
    }

    void erase(abieos::input_buffer k) {
        write_batch.Delete(state_history::rdb::to_slice(k));
        changes[{k.pos, k.end}] = {false, {}};
    }
}; // combined_db

struct state {
    const char*                                         wasm;
    eosio::vm::wasm_allocator&                          wa;
    backend_t&                                          backend;
    std::vector<char>                                   args;
    abieos::input_buffer                                bin;
    combined_db                                         db;
    std::vector<std::shared_ptr<combined_db::iterator>> iterators;

    state(const char* wasm, eosio::vm::wasm_allocator& wa, backend_t& backend, std::vector<char> args)
        : wasm{wasm}
        , wa{wa}
        , backend{backend}
        , args{args} {}
};

struct callbacks {
    ::state& state;

    void check_bounds(const char* begin, const char* end) {
        if (begin > end)
            throw std::runtime_error("bad memory");
        // todo: check bounds
    }

    char* alloc(uint32_t cb_alloc_data, uint32_t cb_alloc, uint32_t size) {
        // todo: verify cb_alloc isn't in imports
        if (state.backend.get_module().tables.size() < 0 || state.backend.get_module().tables[0].table.size() < cb_alloc)
            throw std::runtime_error("cb_alloc is out of range");
        auto result = state.backend.get_context().execute(
            this, eosio::vm::jit_visitor(42), state.backend.get_module().tables[0].table[cb_alloc], cb_alloc_data, size);
        if (!result || !result->is_a<eosio::vm::i32_const_t>())
            throw std::runtime_error("cb_alloc returned incorrect type");
        char* begin = state.wa.get_base_ptr<char>() + result->to_ui32();
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
            throw ::assert_exception(std::string(msg, msg_len));
    }

    void print_range(const char* begin, const char* end) {
        check_bounds(begin, end);
        std::cerr.write(begin, end - begin);
    }

    void get_bin(uint32_t cb_alloc_data, uint32_t cb_alloc) { set_data(cb_alloc_data, cb_alloc, state.bin); }
    void get_args(uint32_t cb_alloc_data, uint32_t cb_alloc) { set_data(cb_alloc_data, cb_alloc, state.args); }

    void kv_set(const char* k_begin, const char* k_end, const char* v_begin, const char* v_end) {
        check_bounds(k_begin, k_end);
        check_bounds(v_begin, v_end);
        state.db.set({k_begin, k_end}, {v_begin, v_end});
    }

    void kv_erase(const char* k_begin, const char* k_end) {
        check_bounds(k_begin, k_end);
        state.db.erase({k_begin, k_end});
    }

    combined_db::iterator* get_it(uint32_t index) {
        if (index >= state.iterators.size())
            throw std::runtime_error("iterator does not exist");
        return state.iterators[index].get();
    }

    void check(const rocksdb::Status& status) {
        if (!status.IsNotFound() && !status.ok())
            throw std::runtime_error("rocksdb error: " + status.ToString());
    }

    uint32_t kv_it_create() {
        state.iterators.push_back(std::make_unique<combined_db::iterator>(state.db.end()));
        return state.iterators.size() - 1;
    }

    bool kv_it_is_end(uint32_t index) { return get_it(index)->is_end(); }

    bool kv_it_set_begin(uint32_t index) {
        auto* it = get_it(index);
        *it      = state.db.begin(std::move(*it));
        return !it->is_end();
    }

    bool kv_it_incr(uint32_t index) {
        auto* it = get_it(index);
        ++*it;
        return !it->is_end();
    }

    bool kv_it_key(uint32_t index, uint32_t cb_alloc_data, uint32_t cb_alloc) {
        auto* it = get_it(index);
        auto  kv = it->get_kv();
        if (!kv)
            return false;
        auto& k = kv->first;
        set_data(cb_alloc_data, cb_alloc, k);
        return true;
    }

    bool kv_it_value(uint32_t index, uint32_t cb_alloc_data, uint32_t cb_alloc) {
        auto* it = get_it(index);
        auto  kv = it->get_kv();
        if (!kv)
            return false;
        auto& v = kv->second;
        set_data(cb_alloc_data, cb_alloc, v);
        return true;
    }
}; // callbacks

void register_callbacks() {
    rhf_t::add<callbacks, &callbacks::abort, eosio::vm::wasm_allocator>("env", "abort");
    rhf_t::add<callbacks, &callbacks::eosio_assert_message, eosio::vm::wasm_allocator>("env", "eosio_assert_message");
    rhf_t::add<callbacks, &callbacks::print_range, eosio::vm::wasm_allocator>("env", "print_range");
    rhf_t::add<callbacks, &callbacks::get_bin, eosio::vm::wasm_allocator>("env", "get_bin");
    rhf_t::add<callbacks, &callbacks::get_args, eosio::vm::wasm_allocator>("env", "get_args");
    rhf_t::add<callbacks, &callbacks::kv_set, eosio::vm::wasm_allocator>("env", "kv_set");
    rhf_t::add<callbacks, &callbacks::kv_erase, eosio::vm::wasm_allocator>("env", "kv_erase");
    rhf_t::add<callbacks, &callbacks::kv_it_create, eosio::vm::wasm_allocator>("env", "kv_it_create");
    rhf_t::add<callbacks, &callbacks::kv_it_is_end, eosio::vm::wasm_allocator>("env", "kv_it_is_end");
    rhf_t::add<callbacks, &callbacks::kv_it_set_begin, eosio::vm::wasm_allocator>("env", "kv_it_set_begin");
    rhf_t::add<callbacks, &callbacks::kv_it_incr, eosio::vm::wasm_allocator>("env", "kv_it_incr");
    rhf_t::add<callbacks, &callbacks::kv_it_key, eosio::vm::wasm_allocator>("env", "kv_it_key");
    rhf_t::add<callbacks, &callbacks::kv_it_value, eosio::vm::wasm_allocator>("env", "kv_it_value");
}

struct ship_connection_state : state_history::connection_callbacks, std::enable_shared_from_this<ship_connection_state> {
    std::shared_ptr<::state>                   state;
    std::shared_ptr<state_history::connection> connection;

    ship_connection_state(const std::shared_ptr<::state>& state)
        : state(state) {}

    void start(boost::asio::io_context& ioc) {
        state_history::connection_config config{"127.0.0.1", "8080"};
        connection = std::make_shared<state_history::connection>(ioc, config, shared_from_this());
        connection->connect();
    }

    void received_abi(std::string_view abi) override {
        ilog("received_abi");
        connection->send(state_history::get_status_request_v0{});
    }

    bool received(state_history::get_status_result_v0& status, abieos::input_buffer bin) override {
        ilog("received status");
        connection->request_blocks(status, 0, {});
        return true;
    }

    bool received(state_history::get_blocks_result_v0& result, abieos::input_buffer bin) override {
        ilog("received block ${n}", ("n", result.this_block ? result.this_block->block_num : -1));
        callbacks cb{*state};
        state->iterators.clear();
        state->db.discard_changes();
        state->bin = bin;
        state->backend.initialize(&cb);
        // backend(&cb, "env", "initialize"); // todo: needs change to eosio-cpp
        state->backend(&cb, "env", "start", 0);
        state->db.write_changes();
        return true;
    }

    void closed(bool retry) override { ilog("closed"); }
};

static void run(const char* wasm, const std::vector<std::string>& args) {
    eosio::vm::wasm_allocator wa;
    auto                      code = backend_t::read_wasm(wasm);
    backend_t                 backend(code);
    auto                      state = std::make_shared<::state>(wasm, wa, backend, abieos::native_to_bin(args));
    //callbacks                 cb{*state};
    backend.set_wasm_allocator(&wa);

    rhf_t::resolve(backend.get_module());

    boost::asio::io_context          ioc;
    state_history::connection_config config{"127.0.0.1", "8080"};
    auto                             ship_state = std::make_shared<ship_connection_state>(state);
    ship_state->start(ioc);
    ioc.run();
}

const char usage[] = "usage: eosio-tester [-h or --help] [-v or --verbose] file.wasm [args for wasm]\n";

int main(int argc, char* argv[]) {
    fc::logger::get(DEFAULT_LOGGER).set_log_level(fc::log_level::off);

    bool show_usage = false;
    bool error      = false;
    int  next_arg   = 1;
    while (next_arg < argc && argv[next_arg][0] == '-') {
        if (!strcmp(argv[next_arg], "-h") || !strcmp(argv[next_arg], "--help"))
            show_usage = true;
        else if (!strcmp(argv[next_arg], "-v") || !strcmp(argv[next_arg], "--verbose"))
            fc::logger::get(DEFAULT_LOGGER).set_log_level(fc::log_level::debug);
        else {
            std::cerr << "unknown option: " << argv[next_arg] << "\n";
            error = true;
        }
        ++next_arg;
    }
    if (next_arg >= argc)
        error = true;
    if (show_usage || error) {
        std::cerr << usage;
        return error;
    }
    try {
        std::vector<std::string> args{argv + next_arg + 1, argv + argc};
        register_callbacks();
        run(argv[next_arg], args);
        return 0;
    } catch (eosio::vm::exception& e) {
        std::cerr << "vm::exception: " << e.detail() << "\n";
    } catch (::assert_exception& e) {
        std::cerr << "assert failed: " << e.what() << "\n";
    } catch (std::exception& e) {
        std::cerr << "std::exception: " << e.what() << "\n";
    } catch (fc::exception& e) {
        std::cerr << "fc::exception: " << e.to_string() << "\n";
    }
    return 1;
}

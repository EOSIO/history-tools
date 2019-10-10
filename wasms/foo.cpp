#include "../src/state_history.hpp"
#include <eosio/asset.hpp>
#include <eosio/check.hpp>
#include <eosio/datastream.hpp>
#include <eosio/name.hpp>
#include <eosio/print.hpp>
#include <vector>

using namespace eosio;
using namespace state_history;
using namespace abieos::literals;

namespace eosio {

ABIEOS_REFLECT(name) { //
    ABIEOS_MEMBER(name, value);
}

void native_to_bin(const symbol& obj, std::vector<char>& bin) { abieos::native_to_bin(obj.raw(), bin); }

ABIEOS_NODISCARD inline bool bin_to_native(symbol& obj, abieos::bin_to_native_state& state, bool start) {
    uint64_t raw;
    if (!abieos::bin_to_native(raw, state, start))
        return false;
    obj = symbol(raw);
    return true;
}

ABIEOS_REFLECT(asset) {
    ABIEOS_MEMBER(asset, amount);
    ABIEOS_MEMBER(asset, symbol);
}

} // namespace eosio

namespace abieos {

template <typename T>
inline constexpr bool serial_reversible = std::is_signed_v<T> || std::is_unsigned_v<T>;

template <>
inline constexpr bool serial_reversible<abieos::name> = true;

template <>
inline constexpr bool serial_reversible<eosio::name> = true;

template <>
inline constexpr bool serial_reversible<abieos::uint128> = true;

template <>
inline constexpr bool serial_reversible<abieos::checksum256> = true;

template <typename F>
void reverse_bin(std::vector<char>& bin, F f) {
    auto s = bin.size();
    f();
    std::reverse(bin.begin() + s, bin.end());
}

template <typename T, typename F>
auto fixup_key(std::vector<char>& bin, F f) -> std::enable_if_t<serial_reversible<T>, void> {
    reverse_bin(bin, f);
}

template <typename T>
void native_to_key(const T& obj, std::vector<char>& bin);

void native_to_key(const std::string& obj, std::vector<char>& bin) {
    for (auto ch : obj) {
        if (ch) {
            bin.push_back(ch);
        } else {
            bin.push_back(0);
            bin.push_back(0);
        }
    }
    bin.push_back(0);
    bin.push_back(1);
}

template <int i, typename... Ts>
void native_to_key_tuple(const std::tuple<Ts...>& obj, std::vector<char>& bin) {
    if constexpr (i < sizeof...(Ts)) {
        native_to_key(std::get<i>(obj), bin);
        native_to_key_tuple<i + 1>(obj, bin);
    }
}

template <typename... Ts>
void native_to_key(const std::tuple<Ts...>& obj, std::vector<char>& bin) {
    native_to_key_tuple<0>(obj, bin);
}

template <typename T>
void native_to_key(const T& obj, std::vector<char>& bin) {
    if constexpr (serial_reversible<std::decay_t<T>>) {
        fixup_key<T>(bin, [&] { abieos::native_to_bin(obj, bin); });
    } else {
        for_each_field((T*)nullptr, [&](auto* name, auto member_ptr) { //
            native_to_key(member_from_void(member_ptr, &obj), bin);
        });
    }
}

template <typename T>
std::vector<char> native_to_key(const T& obj) {
    std::vector<char> bin;
    native_to_key(obj, bin);
    return bin;
}

} // namespace abieos

namespace eosio {
namespace internal_use_do_not_use {

#define IMPORT extern "C" __attribute__((eosio_wasm_import))

IMPORT void get_bin(void* cb_alloc_data, void* (*cb_alloc)(void* cb_alloc_data, size_t size));
IMPORT void kv_set(const char* k_begin, const char* k_end, const char* v_begin, const char* v_end);
IMPORT void kv_erase(const char* k_begin, const char* k_end);
IMPORT uint32_t kv_it_create();
IMPORT bool     kv_it_is_end(uint32_t index);
IMPORT int      kv_it_compare(uint32_t a, uint32_t b);
IMPORT bool     kv_it_set_begin(uint32_t index);
IMPORT bool     kv_it_incr(uint32_t index);
IMPORT bool     kv_it_key(uint32_t index, void* cb_alloc_data, void* (*cb_alloc)(void* cb_alloc_data, size_t size));
IMPORT bool     kv_it_value(uint32_t index, void* cb_alloc_data, void* (*cb_alloc)(void* cb_alloc_data, size_t size));

template <typename Alloc_fn>
inline void get_bin(Alloc_fn alloc_fn) {
    return get_bin(&alloc_fn, [](void* cb_alloc_data, size_t size) -> void* { //
        return (*reinterpret_cast<Alloc_fn*>(cb_alloc_data))(size);
    });
}

void kv_set(const std::vector<char>& k, const std::vector<char>& v) {
    kv_set(k.data(), k.data() + k.size(), v.data(), v.data() + v.size());
}

} // namespace internal_use_do_not_use

inline const std::vector<char>& get_bin() {
    static std::optional<std::vector<char>> bytes;
    if (!bytes) {
        internal_use_do_not_use::get_bin([&](size_t size) {
            bytes.emplace();
            bytes->resize(size);
            return bytes->data();
        });
    }
    return *bytes;
}

template <typename T>
T construct_from_stream(datastream<const char*>& ds) {
    T obj{};
    ds >> obj;
    return obj;
}

template <typename... Ts>
struct type_list {};

template <int i, typename... Ts>
struct skip;

template <int i, typename T, typename... Ts>
struct skip<i, T, Ts...> {
    using types = typename skip<i - 1, Ts...>::type;
};

template <typename T, typename... Ts>
struct skip<1, T, Ts...> {
    using types = type_list<Ts...>;
};

template <typename F, typename... DataStreamArgs, typename... FixedArgs>
void dispatch_mixed(F f, type_list<DataStreamArgs...>, abieos::input_buffer bin, FixedArgs... fixedArgs) {
    datastream<const char*> ds(bin.pos, bin.end - bin.pos);
    std::apply(f, std::tuple<FixedArgs..., DataStreamArgs...>{fixedArgs..., construct_from_stream<DataStreamArgs>(ds)...});
}

template <typename... Ts>
struct serial_dispatcher;

template <typename C, typename... Args, typename... FixedArgs>
struct serial_dispatcher<void (C::*)(Args...) const, FixedArgs...> {
    template <typename F>
    static void dispatch(F f, abieos::input_buffer bin, FixedArgs... fixedArgs) {
        dispatch_mixed(f, typename skip<sizeof...(FixedArgs), std::decay_t<Args>...>::types{}, bin, fixedArgs...);
    }
};

struct action_context {
    const transaction_trace& ttrace;
    const action_trace&      atrace;
};

struct handle_action_base {
    abieos::name contract;
    abieos::name action;

    virtual void dispatch(const action_context& context, abieos::input_buffer bin) = 0;

    static std::vector<handle_action_base*>& get_actions() {
        static std::vector<handle_action_base*> actions;
        return actions;
    }
};

template <typename F>
struct handle_action : handle_action_base {
    F f;

    handle_action(abieos::name contract, abieos::name action, F f)
        : f(f) {
        this->contract = contract;
        this->action   = action;
        get_actions().push_back(this);
    }
    handle_action(const handle_action&) = delete;
    handle_action& operator=(const handle_action&) = delete;

    void dispatch(const action_context& context, abieos::input_buffer bin) override {
        serial_dispatcher<decltype(&F::operator()), const action_context&>::dispatch(f, bin, context);
    }
};

} // namespace eosio

extern "C" __attribute__((eosio_wasm_entry)) void initialize() {}

extern "C" __attribute__((eosio_wasm_entry)) void start() {
    auto res = std::get<get_blocks_result_v0>(assert_bin_to_native<result>(get_bin()));
    if (!res.this_block || !res.traces || !res.deltas)
        return;
    auto traces = assert_bin_to_native<std::vector<transaction_trace>>(*res.traces);
    auto deltas = assert_bin_to_native<std::vector<table_delta>>(*res.deltas);
    print("block ", res.this_block->block_num, " traces: ", traces.size(), " deltas: ", deltas.size(), "\n");
    for (auto& trace : traces) {
        auto& t = std::get<transaction_trace_v0>(trace);
        // print("    trace: status: ", to_string(t.status), " action_traces: ", t.action_traces.size(), "\n");
        if (t.status != state_history::transaction_status::executed)
            continue;
        for (auto& atrace : t.action_traces) {
            auto& at = std::get<action_trace_v0>(atrace);
            if (at.receiver != at.act.account)
                continue;
            for (auto& handler : handle_action_base::get_actions()) {
                if (at.receiver == handler->contract && at.act.name == handler->action) {
                    handler->dispatch({trace, atrace}, at.act.data);
                    break;
                }
            }
            // if (at.receiver == "eosio.token"_n && at.act.name == "transfer"_n) {
            //     print("transfer\n");
            // }
        }
    }
    // for (auto& delta : deltas) {
    //     auto& d = std::get<table_delta_v0>(delta);
    //     print("    table: ", d.name, " rows: ", d.rows.size(), "\n");
    // }

    auto k = abieos::native_to_bin(res.this_block->block_num);
    auto v = get_bin();
    internal_use_do_not_use::kv_set(k.data(), k.data() + k.size(), v.data(), v.data() + v.size());
}

/////////////////////////////////////////

template <typename T>
class table {
  public:
    class index {
      private:
        abieos::name index_name                = {};
        std::vector<char> (*get_key)(const T&) = {};
        table*            t                    = {};
        bool              is_primary           = false;
        std::vector<char> prefix               = {};

        void initialize(table* t, bool is_primary) {
            this->t          = t;
            this->is_primary = is_primary;
            native_to_key(t->table_name, prefix);
            native_to_key(index_name, prefix);
        }

      public:
        friend table;

        friend class iterator;
        class iterator {
            friend table;
            friend index;

          public:
            iterator()                = default;
            iterator(const iterator&) = delete;
            iterator(iterator&&)      = default;

            iterator& operator=(const iterator&) = delete;
            iterator& operator=(iterator&&) = default;

            friend int compare(const iterator& a, const iterator& b) {
                bool a_end = !a.it || kv_it_is_end(a.it);
                bool b_end = !b.it || kv_it_is_end(b.it);
                if (a_end && b_end)
                    return 0;
                else if (a_end && !b_end)
                    return 1;
                else if (!a_end && b_end)
                    return -1;
                else
                    return kv_it_compare(a.it, b.it);
            }

            friend bool operator==(const iterator& a, const iterator& b) { return compare(a, b) == 0; }
            friend bool operator!=(const iterator& a, const iterator& b) { return compare(a, b) != 0; }
            friend bool operator<(const iterator& a, const iterator& b) { return compare(a, b) < 0; }
            friend bool operator<=(const iterator& a, const iterator& b) { return compare(a, b) <= 0; }
            friend bool operator>(const iterator& a, const iterator& b) { return compare(a, b) > 0; }
            friend bool operator>=(const iterator& a, const iterator& b) { return compare(a, b) >= 0; }

          private:
            uint32_t it = 0;
        };

        index(abieos::name index_name, std::vector<char> (*get_key)(const T&))
            : index_name{index_name}
            , get_key{get_key} {}
    }; // index

    using iterator = typename index::iterator;

    template <typename... Indexes>
    table(abieos::name table_context, abieos::name table_name, index& primary_index, Indexes&... secondary_indexes)
        : table_context{table_context}
        , table_name{table_name}
        , primary_index{primary_index}
        , secondary_indexes{&secondary_indexes...} {

        primary_index.initialize(this, true);
        (secondary_indexes.initialize(this, false), ...);
    }

    void insert(const T& obj) {
        // todo: handle overwrite
        auto pk = primary_index.get_key(obj);
        internal_use_do_not_use::kv_set(pk, abieos::native_to_bin(obj));
        for (auto* ind : secondary_indexes) {
            auto sk = ind->get_key(obj);
            sk.insert(sk.end(), pk.begin(), pk.end());
            // todo: re-encode the key to make pk extractable and make value empty
            internal_use_do_not_use::kv_set(sk, pk);
        }
    }

  private:
    abieos::name        table_context;
    abieos::name        table_name;
    index&              primary_index;
    std::vector<index*> secondary_indexes;
};

struct my_struct {
    abieos::name n1;
    abieos::name n2;
    std::string  s1;
    std::string  s2;

    auto primary_key() const { return std::tie(n1, n2); }
    auto foo_key() const { return std::tie(s1); }
    auto bar_key() const { return std::tie(s2); }
};

struct my_other_struct {
    abieos::name n1;
    abieos::name n2;
    std::string  s1;
    std::string  s2;
    std::string  s3;

    auto primary_key() const { return std::tie(n1, n2); }
    auto foo_key() const { return std::tie(s1); }
    auto bar_key() const { return std::tie(s2); }
};

struct my_table : table<my_struct> {
    index primary_index{"primary"_n, [](const auto& obj) { return abieos::native_to_key(obj.primary_key()); }};
    index foo_index{"foo"_n, [](const auto& obj) { return abieos::native_to_key(obj.foo_key()); }};
    index bar_index{"bar"_n, [](const auto& obj) { return abieos::native_to_key(obj.bar_key()); }};

    my_table()
        : table("my.context"_n, "my.table"_n, primary_index, foo_index, bar_index) {}
};

struct my_variant_table : table<std::variant<my_struct, my_other_struct>> {
    index primary_index{
        "primary"_n, [](const auto& v) { return std::visit([](const auto& obj) { return abieos::native_to_key(obj.primary_key()); }, v); }};
    index foo_index{"foo"_n,
                    [](const auto& v) { return std::visit([](const auto& obj) { return abieos::native_to_key(obj.foo_key()); }, v); }};
    index bar_index{"bar"_n,
                    [](const auto& v) { return std::visit([](const auto& obj) { return abieos::native_to_key(obj.bar_key()); }, v); }};

    my_variant_table()
        : table("my.context"_n, "my.vtable"_n, primary_index, foo_index, bar_index) {}
};

/////////////////////////////////////////

struct transfer_data {
    uint64_t     recv_sequence = {};
    eosio::name  from          = {};
    eosio::name  to            = {};
    eosio::asset quantity      = {};
    std::string  memo          = {};

    auto primary_key() const { return recv_sequence; }
    auto from_key() const { return std::tie(from, recv_sequence); }
    auto to_key() const { return std::tie(to, recv_sequence); }
};

ABIEOS_REFLECT(transfer_data) {
    ABIEOS_MEMBER(transfer_data, recv_sequence);
    ABIEOS_MEMBER(transfer_data, from);
    ABIEOS_MEMBER(transfer_data, to);
    ABIEOS_MEMBER(transfer_data, quantity);
    ABIEOS_MEMBER(transfer_data, memo);
}

struct transfer_history : table<transfer_data> {
    index primary_index{"primary"_n, [](const auto& obj) { return abieos::native_to_key(obj.primary_key()); }};
    index from_index{"from"_n, [](const auto& obj) { return abieos::native_to_key(obj.from_key()); }};
    index to_index{"to"_n, [](const auto& obj) { return abieos::native_to_key(obj.to_key()); }};

    transfer_history()
        : table("my.context"_n, "my.table"_n, primary_index, from_index, to_index) {}
};

transfer_history xfer_hist;

eosio::handle_action token_transfer(
    "eosio.token"_n, "transfer"_n,
    [](const action_context& context, eosio::name from, eosio::name to, const eosio::asset& quantity, const std::string& memo) {
        // print(
        //     "    ", std::get<0>(*std::get<0>(context.atrace).receipt).recv_sequence, " transfer ", from, " ", to, " ", quantity, " ", memo,
        //     "\n");
        xfer_hist.insert({std::get<0>(*std::get<0>(context.atrace).receipt).recv_sequence, from, to, quantity, memo});
    });

eosio::handle_action
    eosio_buyrex("eosio"_n, "buyrex"_n, [](const action_context& context, eosio::name from, const eosio::asset& amount) { //
        print("    buyrex ", from, " ", amount, "\n");
        // for (auto data : xfer_hist.primary_index) {
        // }
    });

// eosio::handle_action eosio_sellrex("eosio"_n, "sellrex"_n, [](const action_context& context, eosio::name from, const eosio::asset& rex) { //
//     print("    sellrex ", from, " ", rex, "\n");
// });

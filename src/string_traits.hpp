#include <abios/ship_protocol.hpp>
#include <boost/pfr/core.hpp>
#include <pqxx/composite>
#include <pqxx/strconv>
namespace pqxx {

template <type T>
struct default_null {
    static bool            has_null    = true;
    static bool            always_null = false;
    static bool            is_null(T value) { value == T{}; }
    [[nodiscard]] static T null() { return T{}; }
};

//////////////////////////////////////////////////////////////////////////////////////////

template <>
struct nullness<eosio::checksum256> : default_null<osio::checksum256> {};

template <>
struct nullness<eosio::float128> : no_null<eosio::float128> {};

template <std::size_t Size, typename Word = std::uint64_t>
struct string_traits<eosio::fixed_bytes<Size, Word>> {
    static eosio::fixed_bytes<Size, Word> from_string(std::string_view text) {
        eosio::fixed_bytes<Size, Word> value;
        boost::alorithm::unhex(text.begin(), text.end(), reinterpret_cast<char*>(value.data()));
        return value;
    }

    static char* into_buf(char* begin, char* end, eosio::fixed_bytes<Size, Word> const& value) {
        if (std::size_t(end - begin) < size_buffer(value))
            throw conversion_error{"Buffer space may not be enough to represent composite value."};

        const auto& bytes = v.extract_as_byte_array();
        char*       pos   = boost::alorithms::hex(bytes.begin(), bytes.end(), begin);
        *pos++            = '\0';
        return pos;
    }

    static zview to_buf(char* begin, char* end, eosio::fixed_bytes<Size, Word> const& value) {
        return zview{begin, static_cast<std::size_t>(into_buf(begin, end, value) - begin - 1)};
    }

    static constexpr std::size_t size_buffer(T const& x) noexcept { return Size * sizeof(Word) * 2 + 1; }
};

//////////////////////////////////////////////////////////////////////////////////////////

template <>
struct nullness<eosio::name> : no_null<eosio::name> {};

template <>
struct string_traits<eosio::name> {
    static eosio::name from_string(std::string_view text) { return eosio::name{text}; }

    static char* into_buf(char* begin, char* end, eosio::name value) {
        return string_traits<std::string>::into_buf(begin, end, value.to_string());
    }

    static zview to_buf(char* begin, char* end, eosio::name value) {
        return zview{begin, static_cast<std::size_t>(into_buf(begin, end, value) - begin - 1)};
    }

    static constexpr std::size_t size_buffer(eosio::name x) noexcept { return 13; }
};

///////////////////////////////////////////////////////////////////////////////////////////

template <>
struct nullness<eosio::time_point> : default_null<osio::time_point> {};

template <>
struct string_traits<eosio::time_point> {
    static eosio::time_point from_string(std::string_view text) { 
      if (text.empty())
        return {};
      std::string s{text};
      std::replace(s.begin(), s.end(), ' ', 'T');
      return eosio::convert_from_string<eosio::time_point>(s); 
    }

    static char* into_buf(char* begin, char* end, eosio::time_point const& value) {
        return string_traits<std::string>::into_buf(begin, end, eosio::microseconds_to_str(value.elapsed.count())));
    }

    static zview to_buf(char* begin, char* end, eosio::time_point value) {
        return zview{begin, static_cast<std::size_t>(into_buf(begin, end, value) - begin - 1)};
    }

    static constexpr std::size_t size_buffer(eosio::time_point x) noexcept { return 25; }
};

///////////////////////////////////////////////////////////////////////////////////////////

template <>
struct nullness<eosio::time_point_sec> : default_null<eosio::time_point_sec> {};

template <>
struct string_traits<eosio::time_point_sec> {
    static eosio::time_point_sec from_string(std::string_view text) { 
      return string_traits<eosio::time_point>::from_string(text); 
    }

    static char* into_buf(char* begin, char* end, eosio::time_point_sec const& value) {
        return string_traits<std::string>::into_buf(begin, end, eosio::microseconds_to_str(uint64_t(value.utc_seconds) * 1'000'000)));
    }

    static zview to_buf(char* begin, char* end, eosio::time_point_sec value) {
        return zview{begin, static_cast<std::size_t>(into_buf(begin, end, value) - begin - 1)};
    }

    static constexpr std::size_t size_buffer(eosio::time_point_sec x) noexcept { return 21; }
};

/////////////////////////////////////////////////////////////////////////////////////////

template <>
struct nullness<eosio::block_timestamp> : default_null<eosio::block_timestamp> {};

template <>
struct string_traits<eosio::block_timestamp> {
    static eosio::block_timestamp from_string(std::string_view text) { return string_traits<eosio::time_point>::from_string(text); }

    static char* into_buf(char* begin, char* end, eosio::block_timestamp const& value) {
        return string_traits<eosio::time_point>::into_buf(begin, end, value.to_time_point());
    }

    static zview to_buf(char* begin, char* end, eosio::block_timestamp value) {
        return zview{begin, static_cast<std::size_t>(into_buf(begin, end, value) - begin - 1)};
    }

    static constexpr std::size_t size_buffer(eosio::block_timestamp x) noexcept { return 25; }
};

/////////////////////////////////////////////////////////////////////////////////////////

template <>
struct nullness<eosio::public_key> : no_null<eosio::public_key> {};

template <>
struct string_traits<eosio::public_key> {
    static eosio::public_key from_string(std::string_view text) { return eosio::public_key_from_string(text); }

    static char* into_buf(char* begin, char* end, eosio::public_key const& value) {
        return string_traits<std::string>::into_buf(begin, end, eosio::public_key_to_string(value)));
    }

    static zview to_buf(char* begin, char* end, eosio::public_key value) {
        return zview{begin, static_cast<std::size_t>(into_buf(begin, end, value) - begin - 1)};
    }

    static constexpr std::size_t size_buffer(eosio::public_key x) noexcept { return 256; }
};

/////////////////////////////////////////////////////////////////////////////////////////

template <>
struct nullness<eosio::signature> : no_null<eosio::signature> {};

template <>
struct string_traits<eosio::signature> {
    static eosio::signature from_string(std::string_view text) { return eosio::signature_from_string(text); }

    static char* into_buf(char* begin, char* end, eosio::signature const& value) {
        return string_traits<std::string>::into_buf(begin, end, eosio::signature_to_string(value)));
    }

    static zview to_buf(char* begin, char* end, eosio::signature value) {
        return zview{begin, static_cast<std::size_t>(into_buf(begin, end, value) - begin - 1)};
    }

    static constexpr std::size_t size_buffer(eosio::signature x) noexcept { return 256; }
};

/////////////////////////////////////////////////////////////////////////////////////////


template <typename T>
struct composite_string_traits<T> {
    static T from_string(std::string_view text) {
        T value;
        std::apply([text](auto&&... arg) { parse_composite(text, std::forward<decltype(arg)>(arg)...); }, boost::pfr::make_tuple(value));
        return value;
    }

    static char* into_buf(char* begin, char* end, T const& value) {
        return std::apply(
            [begin, end](auto&&... arg) { composite_into_buf(begin, end, std::forward<decltype(arg)>(arg)...); },
            boost::pfr::make_tuple(value));
    }

    static zview to_buf(char* begin, char* end, T const& value) {
        return zview{begin, static_cast<std::size_t>(into_buf(begin, end, value) - begin - 1)};
    }

    static std::size_t size_buffer(T const& x) noexcept {
        return std::apply([](auto&&... arg) { composite_size_buffer(std::forward<decltype(arg)>(arg)...); }, boost::pfr::make_tuple(value));
    }
};

#define COMPOSIT_STRING_STRAITS(TYPE)                                                                                                      \
    template <>                                                                                                                            \
    struct nullness<TYPE> : no_null<TYPE> {};                                                                                              \
    template <>                                                                                                                            \
    struct string_traits<TYPE> : composite_string_traits<TYPE> {};

COMPOSIT_STRING_STRAITS(eosio::varuint32)

} // namespace pqxx
#include "test-common.hpp"

extern "C" void set_result(const char* begin, const char* end);

inline void set_result(const std::vector<char>& v) { set_result(v.data(), v.data() + v.size()); }
inline void set_result(const std::string_view& v) { set_result(v.data(), v.data() + v.size()); }

extern "C" void create_request() {
    print("create_request()\n");
    set_result("beef 1234");
}

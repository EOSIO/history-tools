// copyright defined in LICENSE.txt

#include <memory>
#include <string>
#include <vector>

using namespace std;

extern "C" void prints(const char* begin, const char* end);
extern "C" void printi32(int32_t);

extern "C" void* memcpy(void* __restrict dest, const void* __restrict src, size_t size) {
    auto d = reinterpret_cast<char*>(dest);
    auto s = reinterpret_cast<const char*>(src);
    while (size--)
        *d++ = *s++;
    return dest;
}

extern "C" void* memset(void* dest, int v, size_t size) {
    auto d = reinterpret_cast<char*>(dest);
    while (size--)
        *d++ = v;
    return dest;
}

inline void print() {}

template <int size, typename... Args>
inline void print(const char (&s)[size], Args&&... args);

template <typename... Args>
inline void print(const string& s, Args&&... args);

template <typename... Args>
inline void print(const std::vector<char>& s, Args&&... args);

template <typename... Args>
inline void print(int32_t i, Args&&... args);

template <int size, typename... Args>
inline void print(const char (&s)[size], Args&&... args) {
    prints(s, s + size);
    print(std::forward<Args>(args)...);
}

template <typename... Args>
inline void print(const string& s, Args&&... args) {
    prints(s.c_str(), s.c_str() + s.size());
    print(std::forward<Args>(args)...);
}

template <typename... Args>
inline void print(const std::vector<char>& s, Args&&... args) {
    prints(s.data(), s.data() + s.size());
    print(std::forward<Args>(args)...);
}

template <typename... Args>
inline void print(int32_t i, Args&&... args) {
    printi32(i);
    print(std::forward<Args>(args)...);
}

typedef void* cb_alloc_fn(void* cb_alloc_data, size_t size);

extern "C" void testdb(void* cb_alloc_data, cb_alloc_fn* cb_alloc);

template <typename Alloc_fn>
inline void testdb(Alloc_fn alloc_fn) {
    testdb(&alloc_fn, [](void* cb_alloc_data, size_t size) -> void* { //
        return (*reinterpret_cast<Alloc_fn*>(cb_alloc_data))(size);
    });
}

extern "C" void startup() {
    print("\nstart wasm\n");

    std::vector<char> s;
    testdb([&s](size_t size) {
        print("in callback: ", size, "\n");
        s.resize(size);
        return s.data();
    });

    print("s.size(): ", s.size(), "\n");
    print("\n<<<<<\n", s, ">>>>>\n\n");
    print("end wasm\n\n");
}

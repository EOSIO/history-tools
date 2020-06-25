## Header file <code>shared_memory.hpp</code></strong>


```
namespace eosio
{
    template <typename T>
    struct shared_memory;

    template <typename Stream>
    datastream<Stream>& operator>>(datastream<Stream>& ds, shared_memory<datastream<Stream>>& dest);

    template <typename Stream1, typename Stream2>
    datastream<Stream1>& operator<<(datastream<Stream1>& ds, shared_memory<datastream<Stream2>> const& obj);

    template <typename Stream>
    datastream<Stream>& operator>>(datastream<Stream>& ds, shared_memory<std::string_view>& dest);

    template <typename Stream>
    datastream<Stream>& operator<<(datastream<Stream>& ds, shared_memory<std::string_view> const& obj);
}
```



#### Struct <code>eosio::shared_memory</code></strong>


```
template <typename T>
struct shared_memory
{
    T value = {};

    T& operator*();

    T const& operator*() const;

    T* operator->();

    T const* operator->() const;
};
```


Tag objects which share memory with streams or with other things. These reduce deserialization overhead, but require the source memory isn’t freed and remains untouched.

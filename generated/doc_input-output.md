# Header file `input-output.hpp`

``` cpp
namespace eosio
{
    std::vector<char> get_input_data();

    extern "C" void set_output_data(char const* begin, char const* end);

    void set_output_data(std::vector<char> const& v);

    void set_output_data(std::string_view const& v);
}
```

### Function `eosio::get_input_data`

``` cpp
std::vector<char> get_input_data();
```

Get data provided to the wasm

-----

### Function `eosio::set_output_data`

``` cpp
void set_output_data(char const* begin, char const* end);
```

Set the wasm’s output data

-----

### Function `eosio::set_output_data`

``` cpp
void set_output_data(std::vector<char> const& v);
```

Set the wasm’s output data

-----

### Function `eosio::set_output_data`

``` cpp
void set_output_data(std::string_view const& v);
```

Set the wasm’s output data

-----

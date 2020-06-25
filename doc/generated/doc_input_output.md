## Header file <code>input_output.hpp</code></strong>


```
namespace eosio
{
    int get_input_data();

    extern "C" void set_output_data(char const* begin, char const* end);

    void set_output_data(int const& v);

    void set_output_data(eosio::rope v);
}
```

#### Function <code>eosio::get_input_data</code></strong>


```
int get_input_data();
```


Get data provided to the wasm



---



#### Function <code>eosio::set_output_data</code></strong>


```
void set_output_data(char const* begin, char const* end);
```


Set the wasm’s output data



---



#### Function <code>eosio::set_output_data</code></strong>


```
void set_output_data(int const& v);
```


Set the wasm’s output data



---



#### Function <code>eosio::set_output_data</code></strong>


```
void set_output_data(eosio::rope v);
```

Set the wasm’s output data

---

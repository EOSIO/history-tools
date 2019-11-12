# Database

## Defining a Basic Table

```c++
struct my_struct {
   name     n1;
   name     n2;
   string   foo;
   string   bar;

   auto primary_key() const { return make_key(n1, n2); }
   auto foo_key() const {     return make_key(foo); }
   auto bar_key() const {     return make_key(foo, bar); }
};

struct my_table: table<my_struct> {
   index primary_index{"primary"_n,   &my_struct::primary_key};
   index foo_index{    "foo"_n,       &my_struct::foo_key};
   index bar_index{    "bar"_n,       &my_struct::bar_key};

   my_table(context& c) {
      init(c, "my.contract"_n, "my.table"_n, primary_index, foo_index, bar_index);
   }
};
```

A table has 1 or more fields and 1 or more indexes. Each index has 1 or more fields. The
primary index doesn't allow duplicates; the others do.

## Defining a Variant Table

A variant table may store different structs. New structs may be added over time.

```c++
struct my_struct {
   // see above
};

struct my_other_struct {
   name     n1;
   name     n2;
   string   foo;

   auto primary_key() const { return make_key(n1, n2); }
   auto foo_key() const {     return make_key(foo); }
   auto bar_key() const {     return make_key(foo, foo); }
};

struct my_table: table<my_struct, my_other_struct> {
   index primary_index{"primary"_n,   &my_struct::primary_key,   &my_other_struct::primary_key};
   index foo_index{    "foo"_n,       &my_struct::foo_key,       &my_other_struct::foo_key};
   index bar_index{    "bar"_n,       &my_struct::bar_key,       &my_other_struct::bar_key};

   my_table(context& c) {
      init("my.contract"_n, "my.table"_n, primary_index, foo_index, bar_index);
   }
};
```

## Adding Rows to a Table

To add rows to a table, instantiate it then use `insert()`. This automatically replaces existing rows
when they match the primary key.

```c++
void add_a_row(context& c) {
   my_table t{c};
   my_struct s{
      .n1 = "something"_n,
      .n2 = "else"_n,
      .foo = "a string",
      .bar = "another string",
   };
   t.insert(s);
}
```

## Finding Rows

To find rows using an index, use `find()`.

```c++
void find_a_row(context& c) {
   my_table t{c};
   auto it = t.foo_index.find("a string");
   if (it != t.foo_index.end()) {
      my_struct obj = it.read();
      // use obj
   } else {
      // not found
   }
}
```

## Looping through a range

This loops through a range of keys:

```c++
void loop(context& c) {
   my_table t{c};
   for(my_struct obj: t.bar_index.range({"aa", "bb"}, {"xx", "yy"})) {
      // use obj
   }
}
```
